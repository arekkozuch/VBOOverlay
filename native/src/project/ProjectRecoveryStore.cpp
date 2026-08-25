#include "project/ProjectRecoveryStore.h"
#include "project/BoundedJsonLoader.h"
#include "project/ProjectLimits.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>

namespace FlappedEar {

namespace {
constexpr int RecoveryFormatVersion = 2;
constexpr int LegacyRecoveryFormatVersion = 1;

bool unsignedValue(const QJsonValue &value, quint64 *result)
{
    if (!value.isString()) return false;
    bool ok = false;
    const quint64 parsed = value.toString().toULongLong(&ok);
    if (!ok) return false;
    if (result) *result = parsed;
    return true;
}
}

ProjectRecoveryStore::ProjectRecoveryStore(QString path)
    : m_path(path.isEmpty()
                 ? QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation))
                       .filePath(QStringLiteral("project-recovery.json"))
                 : std::move(path))
{
}

QString ProjectRecoveryStore::path() const { return m_path; }
bool ProjectRecoveryStore::exists() const { return QFileInfo(m_path).isFile(); }

bool ProjectRecoveryStore::load(ProjectRecoverySnapshot *snapshot, QString *error) const
{
    const auto loaded = BoundedJsonLoader::loadFile(
        m_path, ProjectLimits::recoveryBytes, QStringLiteral("Recovery snapshot"));
    if (!loaded.success() || !loaded.document.isObject()) {
        if (error) *error = loaded.error;
        return false;
    }
    const QJsonObject root = loaded.document.object();
    QString projectError;
    const QJsonValue versionValue = root.value(QStringLiteral("recoveryVersion"));
    const int version = versionValue.toInt(-1);
    if (!versionValue.isDouble() || versionValue.toDouble() != version
        || (version != RecoveryFormatVersion && version != LegacyRecoveryFormatVersion)
        || root.value(QStringLiteral("dirty")).toBool() != true
        || !root.value(QStringLiteral("project")).isObject()
        || !ProjectLimits::validateProject(root.value(QStringLiteral("project")).toObject(), &projectError)) {
        if (error) *error = QStringLiteral("invalid or unsupported recovery snapshot: %1").arg(projectError);
        return false;
    }
    quint64 revision = 0;
    quint64 savedRevision = 0;
    if (!unsignedValue(root.value(QStringLiteral("revision")), &revision)
        || !unsignedValue(root.value(QStringLiteral("lastSavedRevision")), &savedRevision)) {
        if (error) *error = QStringLiteral("recovery snapshot revision metadata is malformed");
        return false;
    }
    const bool hasLogicalMetadata = version == RecoveryFormatVersion;
    const QString documentId = root.value(QStringLiteral("documentId")).toString();
    if (hasLogicalMetadata && (documentId.isEmpty() || documentId.size() > 128)) {
        if (error) *error = QStringLiteral("recovery snapshot document identity is malformed");
        return false;
    }
    if (hasLogicalMetadata) {
        const QJsonObject documentState = root.value(QStringLiteral("project")).toObject()
                                             .value(QStringLiteral("documentState")).toObject();
        quint64 projectSavedRevision = 0;
        if (documentState.value(QStringLiteral("id")).toString() != documentId
            || !unsignedValue(documentState.value(QStringLiteral("savedRevision")), &projectSavedRevision)
            || projectSavedRevision != savedRevision) {
            if (error) *error = QStringLiteral("recovery snapshot document metadata does not match payload");
            return false;
        }
    }
    if (snapshot) {
        snapshot->originalProjectPath = root.value(QStringLiteral("originalProjectPath")).toString();
        snapshot->documentId = documentId;
        snapshot->revision = revision;
        snapshot->lastSavedRevision = savedRevision;
        snapshot->timestamp = root.value(QStringLiteral("timestamp")).toString();
        snapshot->project = root.value(QStringLiteral("project")).toObject();
        snapshot->hasLogicalMetadata = hasLogicalMetadata;
    }
    return true;
}

bool ProjectRecoveryStore::write(const ProjectRecoverySnapshot &snapshot, QString *error) const
{
    const QFileInfo info(m_path);
    if (!QDir().mkpath(info.absolutePath())) {
        if (error) *error = QStringLiteral("could not create recovery directory");
        return false;
    }
    const QJsonObject root{
        {QStringLiteral("recoveryVersion"), RecoveryFormatVersion},
        {QStringLiteral("dirty"), true},
        {QStringLiteral("timestamp"), snapshot.timestamp},
        {QStringLiteral("originalProjectPath"), snapshot.originalProjectPath},
        {QStringLiteral("documentId"), snapshot.documentId},
        {QStringLiteral("revision"), QString::number(snapshot.revision)},
        {QStringLiteral("lastSavedRevision"), QString::number(snapshot.lastSavedRevision)},
        {QStringLiteral("project"), snapshot.project},
    };
    QSaveFile file(m_path);
    file.setDirectWriteFallback(false);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error) *error = file.errorString();
        return false;
    }
    const QByteArray payload = QJsonDocument(root).toJson(QJsonDocument::Indented);
    if (file.write(payload) != payload.size() || !file.commit()) {
        if (error) *error = file.errorString();
        return false;
    }
    return true;
}

bool ProjectRecoveryStore::clear(QString *error) const
{
    if (!exists() || QFile::remove(m_path)) {
        return true;
    }
    if (error) *error = QStringLiteral("could not remove recovery snapshot");
    return false;
}

} // namespace FlappedEar
