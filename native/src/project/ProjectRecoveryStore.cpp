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
constexpr int DiscardTombstoneFormatVersion = 1;

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

ProjectRecoveryStore::ProjectRecoveryStore(QString path, Operations operations)
    : m_path(path.isEmpty()
                 ? QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation))
                       .filePath(QStringLiteral("project-recovery.json"))
                 : std::move(path))
    , m_operations(std::move(operations))
{
}

QString ProjectRecoveryStore::path() const { return m_path; }
QString ProjectRecoveryStore::discardTombstonePath() const
{
    return m_path + QStringLiteral(".discard");
}
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
    QString projectError;
    if (!ProjectLimits::validateProject(snapshot.project, &projectError)) {
        if (error) *error = QStringLiteral("invalid recovery project: %1").arg(projectError);
        return false;
    }
    const QJsonObject documentState = snapshot.project.value(QStringLiteral("documentState")).toObject();
    quint64 projectSavedRevision = 0;
    if (snapshot.documentId.isEmpty() || snapshot.documentId.size() > 128
        || documentState.value(QStringLiteral("id")).toString() != snapshot.documentId
        || !unsignedValue(documentState.value(QStringLiteral("savedRevision")), &projectSavedRevision)
        || projectSavedRevision != snapshot.lastSavedRevision) {
        if (error) *error = QStringLiteral("recovery snapshot document metadata does not match payload");
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
    const QByteArray payload = QJsonDocument(root).toJson(QJsonDocument::Indented);
    if (payload.size() > ProjectLimits::recoveryBytes) {
        if (error) {
            *error = QStringLiteral("recovery snapshot is %1 bytes; the limit is %2 bytes")
                         .arg(payload.size())
                         .arg(ProjectLimits::recoveryBytes);
        }
        return false;
    }
    const QFileInfo info(m_path);
    if (!QDir().mkpath(info.absolutePath())) {
        if (error) *error = QStringLiteral("could not create recovery directory");
        return false;
    }
    QSaveFile file(m_path);
    file.setDirectWriteFallback(false);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error) *error = file.errorString();
        return false;
    }
    if (file.write(payload) != payload.size() || !file.commit()) {
        if (error) *error = file.errorString();
        return false;
    }
    return true;
}

bool ProjectRecoveryStore::clear(QString *error) const
{
    if (m_operations.clearSnapshot) {
        return m_operations.clearSnapshot(error);
    }
    if (!exists() || QFile::remove(m_path)) {
        return true;
    }
    if (error) *error = QStringLiteral("could not remove recovery snapshot");
    return false;
}

bool ProjectRecoveryStore::loadDiscardTombstone(
    ProjectRecoveryDiscardTombstone *tombstone, QString *error) const
{
    const auto loaded = BoundedJsonLoader::loadFile(
        discardTombstonePath(), ProjectLimits::recoveryBytes,
        QStringLiteral("Recovery discard tombstone"));
    if (!loaded.success() || !loaded.document.isObject()) {
        if (error) *error = loaded.error;
        return false;
    }
    const QJsonObject root = loaded.document.object();
    quint64 revision = 0;
    const QJsonValue versionValue = root.value(QStringLiteral("discardVersion"));
    const int version = versionValue.toInt(-1);
    const QString documentId = root.value(QStringLiteral("documentId")).toString();
    if (!versionValue.isDouble() || versionValue.toDouble() != version
        || version != DiscardTombstoneFormatVersion
        || documentId.isEmpty() || documentId.size() > 128
        || !unsignedValue(root.value(QStringLiteral("discardedThroughRevision")), &revision)) {
        if (error) *error = QStringLiteral("invalid recovery discard tombstone");
        return false;
    }
    if (tombstone) {
        tombstone->documentId = documentId;
        tombstone->discardedThroughRevision = revision;
    }
    return true;
}

bool ProjectRecoveryStore::writeDiscardTombstone(
    const ProjectRecoveryDiscardTombstone &tombstone, QString *error) const
{
    if (m_operations.writeDiscardTombstone) {
        return m_operations.writeDiscardTombstone(error);
    }
    if (tombstone.documentId.isEmpty() || tombstone.documentId.size() > 128) {
        if (error) *error = QStringLiteral("recovery discard tombstone document identity is malformed");
        return false;
    }
    const QFileInfo info(discardTombstonePath());
    if (!QDir().mkpath(info.absolutePath())) {
        if (error) *error = QStringLiteral("could not create recovery discard directory");
        return false;
    }
    const QJsonObject root{
        {QStringLiteral("discardVersion"), DiscardTombstoneFormatVersion},
        {QStringLiteral("documentId"), tombstone.documentId},
        {QStringLiteral("discardedThroughRevision"),
         QString::number(tombstone.discardedThroughRevision)},
    };
    QSaveFile file(discardTombstonePath());
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

bool ProjectRecoveryStore::clearDiscardTombstone(QString *error) const
{
    const QString tombstonePath = discardTombstonePath();
    if (!QFileInfo(tombstonePath).exists() || QFile::remove(tombstonePath)) {
        return true;
    }
    if (error) *error = QStringLiteral("could not remove recovery discard tombstone");
    return false;
}

} // namespace FlappedEar
