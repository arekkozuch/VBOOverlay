#include "project/ProjectRecoveryStore.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>

namespace FlappedEar {

namespace {
constexpr int RecoveryFormatVersion = 1;
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
    QFile file(m_path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) *error = file.errorString();
        return false;
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    const QJsonObject root = document.object();
    if (parseError.error != QJsonParseError::NoError || !document.isObject()
        || root.value(QStringLiteral("recoveryVersion")).toInt() != RecoveryFormatVersion
        || root.value(QStringLiteral("dirty")).toBool() != true
        || !root.value(QStringLiteral("project")).isObject()) {
        if (error) *error = QStringLiteral("invalid or unsupported recovery snapshot");
        return false;
    }
    bool revisionOk = false;
    bool savedRevisionOk = false;
    const quint64 revision = root.value(QStringLiteral("revision")).toString().toULongLong(&revisionOk);
    const quint64 savedRevision = root.value(QStringLiteral("lastSavedRevision")).toString().toULongLong(&savedRevisionOk);
    if (!revisionOk || !savedRevisionOk || revision == savedRevision) {
        if (error) *error = QStringLiteral("recovery snapshot does not describe unsaved state");
        return false;
    }
    if (snapshot) {
        snapshot->originalProjectPath = root.value(QStringLiteral("originalProjectPath")).toString();
        snapshot->revision = revision;
        snapshot->lastSavedRevision = savedRevision;
        snapshot->timestamp = root.value(QStringLiteral("timestamp")).toString();
        snapshot->project = root.value(QStringLiteral("project")).toObject();
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
