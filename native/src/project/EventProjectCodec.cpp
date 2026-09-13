#include "project/EventProjectCodec.h"
#include "telemetry/OutingLaps.h"
#include "project/ProjectLimits.h"

#include <QCryptographicHash>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QSet>
#include <cmath>

namespace FlappedEar {
namespace {

bool fail(QString *error, const QString &message)
{
    if (error) *error = message;
    return false;
}

bool validText(const QJsonValue &value, const qsizetype limit)
{
    return value.isString() && !value.toString().trimmed().isEmpty()
        && value.toString().size() <= limit;
}

bool validReference(const QJsonValue &value)
{
    if (!value.isObject()) return false;
    const QJsonObject reference = value.toObject();
    for (const QString &key : {QStringLiteral("relativePath"), QStringLiteral("absolutePath")}) {
        if (reference.contains(key) && !reference.value(key).isString()) return false;
        if (reference.value(key).toString().contains(QChar::Null)) return false;
    }
    const QString relative = reference.value(QStringLiteral("relativePath")).toString();
    const QString absolute = reference.value(QStringLiteral("absolutePath")).toString();
    if (relative.trimmed().isEmpty() && absolute.trimmed().isEmpty()) return false;
    if (!relative.isEmpty() && QDir::isAbsolutePath(relative)) return false;
    if (!absolute.isEmpty() && !QDir::isAbsolutePath(absolute)) return false;
    return !reference.contains(QStringLiteral("fingerprint"))
        || reference.value(QStringLiteral("fingerprint")).isObject();
}

QJsonObject primaryFingerprint(const QJsonObject &run)
{
    for (const auto &value : run.value("sources").toObject().value("telemetry").toArray()) {
        const auto source = value.toObject();
        if (source.value("id") == run.value("primaryTelemetrySourceId"))
            return source.value("reference").toObject().value("fingerprint").toObject();
    }
    return {};
}

bool validConfiguration(const QJsonObject &run)
{
    if (!run.contains("trackConfiguration")) return true;
    if (!run.value("trackConfiguration").isObject()) return false;
    const auto config = run.value("trackConfiguration").toObject();
    const auto layout = config.value("layoutId");
    const auto direction = config.value("direction");
    const auto revision = config.value("gateRevision");
    static const QRegularExpression revisionPattern("^gates-v1:[0-9a-f]{64}$");
    return (layout.isNull() || validText(layout, ProjectLimits::maximumIdCharacters))
        && direction.isString() && QStringList{"unknown", "clockwise", "counterclockwise"}.contains(direction.toString())
        && (revision.isNull() || (revision.isString() && revisionPattern.match(revision.toString()).hasMatch()))
        && config.value("sourceId") == run.value("primaryTelemetrySourceId")
        && config.value("sourceFingerprint").isObject()
        && config.value("sourceFingerprint").toObject() == primaryFingerprint(run);
}

ProjectSourceReference sourceReference(const QJsonObject &object)
{
    return {object.value(QStringLiteral("relativePath")).toString(),
            object.value(QStringLiteral("absolutePath")).toString(),
            object.value(QStringLiteral("fingerprint")).toObject()};
}

QDir referenceDirectory(const QString &projectPath)
{
    const QFileInfo directory(QFileInfo(projectPath).absolutePath());
    const QString canonical = directory.canonicalFilePath();
    return QDir(canonical.isEmpty() ? directory.absoluteFilePath() : canonical);
}

QJsonObject rebaseReference(QJsonObject reference, const QString &oldPath, const QString &newPath)
{
    const QJsonObject known = EventProjectCodec::referenceForSave(sourceReference(reference), oldPath, newPath);
    reference.remove(QStringLiteral("relativePath"));
    reference.remove(QStringLiteral("absolutePath"));
    reference.remove(QStringLiteral("fingerprint"));
    for (auto it = known.begin(); it != known.end(); ++it) reference.insert(it.key(), it.value());
    return reference;
}

} // namespace

bool EventProjectCodec::isEvent(const QJsonObject &project)
{
    return project.value(QStringLiteral("version")).toDouble() == 3.0;
}

bool EventProjectCodec::validate(const QJsonObject &project, QString *error)
{
    for (const QString &key : {QStringLiteral("sources"), QStringLiteral("sync"),
                              QStringLiteral("videoPath"), QStringLiteral("vboPath")}) {
        if (project.contains(key)) return fail(error, QStringLiteral("Event projects cannot contain root %1.").arg(key));
    }
    const QJsonObject event = project.value(QStringLiteral("event")).toObject();
    if (!validText(event.value(QStringLiteral("id")), ProjectLimits::maximumIdCharacters)
        || !validText(event.value(QStringLiteral("name")), ProjectLimits::maximumTemplateNameCharacters)
        || !validText(event.value(QStringLiteral("activeRunId")), ProjectLimits::maximumIdCharacters)
        || !event.value(QStringLiteral("runs")).isArray()) {
        return fail(error, QStringLiteral("Event metadata is missing or malformed."));
    }
    if (!validLapExclusions(event.value("lapExclusions"), event.value("id").toString()))
        return fail(error, QStringLiteral("Lap exclusions contain an invalid reference, duplicate or reason."));
    const QJsonArray runs = event.value(QStringLiteral("runs")).toArray();
    if (runs.isEmpty() || runs.size() > maximumRuns) return fail(error, QStringLiteral("An event must contain 1–64 runs."));
    QSet<QString> runIds;
    QSet<QString> sourceIds;
    for (const QJsonValue &value : runs) {
        const QJsonObject run = value.toObject();
        const QString id = run.value(QStringLiteral("id")).toString();
        if (!validText(run.value(QStringLiteral("id")), ProjectLimits::maximumIdCharacters)
            || runIds.contains(id)
            || !validText(run.value(QStringLiteral("name")), ProjectLimits::maximumTemplateNameCharacters)
            || !validText(run.value(QStringLiteral("primaryTelemetrySourceId")), ProjectLimits::maximumIdCharacters)) {
            return fail(error, QStringLiteral("Run metadata or identity is invalid."));
        }
        if (run.value("name").toString().contains(QChar::Null))
            return fail(error, QStringLiteral("Run name contains NUL."));
        for (const auto *key : {"notes", "conditions", "setupChanges"}) {
            const auto text = run.value(key);
            if (!text.isUndefined() && !text.isNull() && (!text.isString()
                || text.toString().size() > ProjectLimits::maximumStringCharacters
                || text.toString().contains(QChar::Null)))
                return fail(error, QStringLiteral("Run notes, conditions and setup changes must be bounded text or unknown."));
        }
        runIds.insert(id);
        const QJsonObject sources = run.value(QStringLiteral("sources")).toObject();
        const QJsonValue telemetryValue = sources.value(QStringLiteral("telemetry"));
        const QJsonArray telemetry = telemetryValue.toArray();
        if (!telemetryValue.isArray() || telemetry.isEmpty() || telemetry.size() > maximumSourcesPerRun) {
            return fail(error, QStringLiteral("Each run must contain 1–8 telemetry sources."));
        }
        bool primaryFound = false;
        for (const QJsonValue &sourceValue : telemetry) {
            const QJsonObject source = sourceValue.toObject();
            const QString sourceId = source.value(QStringLiteral("id")).toString();
            if (!validText(source.value(QStringLiteral("id")), ProjectLimits::maximumIdCharacters)
                || sourceIds.contains(sourceId) || !validReference(source.value(QStringLiteral("reference")))) {
                return fail(error, QStringLiteral("Telemetry source identity or reference is invalid."));
            }
            sourceIds.insert(sourceId);
            primaryFound |= sourceId == run.value(QStringLiteral("primaryTelemetrySourceId")).toString();
        }
        if (!primaryFound || sourceIds.size() > maximumTelemetrySources) {
            return fail(error, QStringLiteral("Primary telemetry source is missing or event source limit exceeded."));
        }
        if (!validConfiguration(run)) {
            return fail(error, QStringLiteral("Track configuration or its primary source binding is invalid."));
        }
        if (sources.contains(QStringLiteral("video")) && !validReference(sources.value(QStringLiteral("video")))) {
            return fail(error, QStringLiteral("Run video reference is invalid."));
        }
        const QJsonObject sync = run.value(QStringLiteral("sync")).toObject();
        const QJsonValue offset = sync.value(QStringLiteral("offset"));
        const QJsonValue scale = sync.value(QStringLiteral("timeScale"));
        if (!offset.isDouble() || !scale.isDouble() || !std::isfinite(offset.toDouble())
            || !std::isfinite(scale.toDouble()) || scale.toDouble() <= 0.0) {
            return fail(error, QStringLiteral("Run synchronization is invalid."));
        }
    }
    if (!runIds.contains(event.value(QStringLiteral("activeRunId")).toString())) {
        return fail(error, QStringLiteral("Active run does not belong to the event."));
    }
    return true;
}

QJsonObject EventProjectCodec::unknownTrackConfiguration(
    const QString &sourceId, const QJsonObject &fingerprint, const QString &gateRevision)
{
    return {{"layoutId", QJsonValue::Null}, {"direction", "unknown"},
        {"gateRevision", gateRevision.isEmpty() ? QJsonValue(QJsonValue::Null) : QJsonValue(gateRevision)},
        {"sourceId", sourceId}, {"sourceFingerprint", fingerprint}};
}

QJsonObject EventProjectCodec::trackConfiguration(const QJsonObject &run)
{
    return run.contains("trackConfiguration") ? run.value("trackConfiguration").toObject()
        : unknownTrackConfiguration(run.value("primaryTelemetrySourceId").toString(), primaryFingerprint(run));
}

QByteArray EventProjectCodec::lapDerivationKey(const QJsonObject &run)
{
    return QCryptographicHash::hash(QJsonDocument(QJsonObject{{"version", "lap-derivation-v1"},
        {"runId", run.value("id")}, {"sourceId", run.value("primaryTelemetrySourceId")},
        {"sourceFingerprint", primaryFingerprint(run)}, {"trackConfiguration", trackConfiguration(run)}})
        .toJson(QJsonDocument::Compact), QCryptographicHash::Sha256).toHex();
}

QJsonObject EventProjectCodec::editorProjection(const QJsonObject &project)
{
    if (!isEvent(project)) return project;
    QJsonObject editor = project;
    editor.remove(QStringLiteral("event"));
    editor.insert(QStringLiteral("version"), 2);
    const QJsonObject event = project.value(QStringLiteral("event")).toObject();
    for (const QJsonValue &value : event.value(QStringLiteral("runs")).toArray()) {
        const QJsonObject run = value.toObject();
        if (run.value(QStringLiteral("id")) != event.value(QStringLiteral("activeRunId"))) continue;
        const QJsonObject runSources = run.value(QStringLiteral("sources")).toObject();
        QJsonObject sources;
        if (runSources.contains(QStringLiteral("video"))) sources.insert(QStringLiteral("video"), runSources.value(QStringLiteral("video")));
        for (const QJsonValue &sourceValue : runSources.value(QStringLiteral("telemetry")).toArray()) {
            const QJsonObject source = sourceValue.toObject();
            if (source.value(QStringLiteral("id")) == run.value(QStringLiteral("primaryTelemetrySourceId"))) {
                sources.insert(QStringLiteral("telemetry"), source.value(QStringLiteral("reference")));
            }
        }
        editor.insert(QStringLiteral("sources"), sources);
        editor.insert(QStringLiteral("sync"), run.value(QStringLiteral("sync")));
        break;
    }
    return editor;
}

QJsonObject EventProjectCodec::referenceForSave(
    const ProjectSourceReference &reference, const QString &previousProjectPath, const QString &targetProjectPath)
{
    if (reference.isEmpty()) return {};
    QString absolute = ProjectSourceReferenceCodec::resolve(reference, previousProjectPath);
    if (absolute.isEmpty() && !reference.relativePath.isEmpty() && !previousProjectPath.isEmpty()) {
        // Missing files still have a location; never reinterpret it against Save As or cwd.
        absolute = referenceDirectory(previousProjectPath).absoluteFilePath(reference.relativePath);
    }
    if (absolute.isEmpty()) absolute = reference.absolutePath;
    if (absolute.isEmpty()) return ProjectSourceReferenceCodec::toJson(reference, targetProjectPath);
    // Drop the old relative spelling before deriving one in the new document directory.
    return ProjectSourceReferenceCodec::toJson({{}, absolute, reference.fingerprint}, targetProjectPath);
}

QStringList EventProjectCodec::referencedPaths(const QJsonObject &project, const QString &projectPath)
{
    QStringList paths;
    const auto append = [&paths, &projectPath](const QJsonObject &object) {
        const auto reference = sourceReference(object);
        if (!reference.absolutePath.isEmpty()) paths.append(reference.absolutePath);
        if (!reference.relativePath.isEmpty() && !projectPath.isEmpty()) {
            paths.append(referenceDirectory(projectPath).absoluteFilePath(reference.relativePath));
        }
    };
    const auto event = project.value(QStringLiteral("event")).toObject();
    for (const QJsonValue &value : event.value(QStringLiteral("runs")).toArray()) {
        const auto sources = value.toObject().value(QStringLiteral("sources")).toObject();
        append(sources.value(QStringLiteral("video")).toObject());
        for (const QJsonValue &source : sources.value(QStringLiteral("telemetry")).toArray()) {
            append(source.toObject().value(QStringLiteral("reference")).toObject());
        }
    }
    paths.removeDuplicates();
    return paths;
}

QJsonObject EventProjectCodec::withEditorState(
    const QJsonObject &eventProject, QJsonObject editorProject,
    const QString &previousProjectPath, const QString &targetProjectPath)
{
    QJsonObject event = eventProject.value(QStringLiteral("event")).toObject();
    QJsonArray runs;
    for (const QJsonValue &value : event.value(QStringLiteral("runs")).toArray()) {
        QJsonObject run = value.toObject();
        const bool active = run.value(QStringLiteral("id")) == event.value(QStringLiteral("activeRunId"));
        QJsonObject sources = run.value(QStringLiteral("sources")).toObject();
        QJsonArray telemetry;
        const QJsonObject editorSources = editorProject.value(QStringLiteral("sources")).toObject();
        for (const QJsonValue &sourceValue : sources.value(QStringLiteral("telemetry")).toArray()) {
            QJsonObject source = sourceValue.toObject();
            const bool primary = source.value(QStringLiteral("id")) == run.value(QStringLiteral("primaryTelemetrySourceId"));
            source.insert(QStringLiteral("reference"), active && primary
                ? editorSources.value(QStringLiteral("telemetry")).toObject()
                : rebaseReference(source.value(QStringLiteral("reference")).toObject(), previousProjectPath, targetProjectPath));
            telemetry.append(source);
        }
        sources.insert(QStringLiteral("telemetry"), telemetry);
        if (active) {
            if (editorSources.contains(QStringLiteral("video"))) sources.insert(QStringLiteral("video"), editorSources.value(QStringLiteral("video")));
            else sources.remove(QStringLiteral("video"));
            run.insert(QStringLiteral("sync"), editorProject.value(QStringLiteral("sync")));
        } else if (sources.contains(QStringLiteral("video"))) {
            sources.insert(QStringLiteral("video"), rebaseReference(sources.value(QStringLiteral("video")).toObject(), previousProjectPath, targetProjectPath));
        }
        const auto previousFingerprint = primaryFingerprint(run);
        run.insert(QStringLiteral("sources"), sources);
        if (primaryFingerprint(run) != previousFingerprint) {
            // A replacement source cannot inherit asserted layout/gate metadata.
            // A same-content relink/Save As changes paths only and retains it.
            run.insert("trackConfiguration", unknownTrackConfiguration(
                run.value("primaryTelemetrySourceId").toString(), primaryFingerprint(run)));
        }
        runs.append(run);
    }
    event.insert(QStringLiteral("runs"), runs);
    editorProject.insert(QStringLiteral("version"), 3);
    editorProject.insert(QStringLiteral("event"), event);
    editorProject.remove(QStringLiteral("sources"));
    editorProject.remove(QStringLiteral("sync"));
    editorProject.remove(QStringLiteral("videoPath"));
    editorProject.remove(QStringLiteral("vboPath"));
    return editorProject;
}

} // namespace FlappedEar
