#include "export/ExportArtifactManifest.h"
#include "project/BoundedJsonLoader.h"
#include "project/ProjectLimits.h"

#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QUuid>
#include <limits>

#ifdef Q_OS_UNIX
#include <cerrno>
#include <signal.h>
#endif
#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

namespace FlappedEar {
namespace {
QString normalized(const QString &path)
{
    return QDir::cleanPath(QFileInfo(path).absoluteFilePath());
}

bool isValidId(const QString &id)
{
    return QUuid(QStringLiteral("{%1}").arg(id)).isNull() == false;
}

bool validOwnedData(const QString &manifestPath, const ExportArtifactManifestData &data)
{
    if (!isValidId(data.exportId) || data.createdUtcMilliseconds <= 0 || data.temporaryOverlayPath.isEmpty()
        || data.outputStagingPath.isEmpty() || data.finalTargetPath.isEmpty()) return false;
    const QString tempRoot = normalized(QDir::tempPath());
    const QFileInfo overlay(data.temporaryOverlayPath);
    const QString expectedOverlay = normalized(QDir(tempRoot).filePath(
        QStringLiteral("flappedear-overlay-%1.mkv").arg(data.exportId)));
    if (normalized(overlay.absoluteFilePath()) != expectedOverlay) return false;
    const QString expectedManifest = normalized(ExportArtifactManifest::manifestPathFor(data.exportId));
    if (normalized(manifestPath) != expectedManifest) return false;
    const QFileInfo target(data.finalTargetPath);
    QString baseName = target.completeBaseName();
    if (baseName.isEmpty()) baseName = QStringLiteral("export");
    QString suffix = target.completeSuffix();
    if (suffix.isEmpty()) suffix = QStringLiteral("mp4");
    const QString expectedStaging = normalized(QDir(target.absolutePath()).filePath(
        QStringLiteral(".%1.flappedear-%2.part.%3").arg(baseName, data.exportId, suffix)));
    if (normalized(data.outputStagingPath) != expectedStaging
        || normalized(data.outputStagingPath) == normalized(data.finalTargetPath)) return false;
    return true;
}

QJsonObject asJson(const ExportArtifactManifestData &data)
{
    return {{"version", ExportArtifactManifest::version}, {"exportId", data.exportId},
            {"createdUtcMilliseconds", data.createdUtcMilliseconds},
            {"temporaryOverlayPath", data.temporaryOverlayPath}, {"outputStagingPath", data.outputStagingPath},
            {"finalTargetPath", data.finalTargetPath}, {"workerPid", data.workerPid}, {"state", data.state}};
}
}

QString ExportArtifactManifest::manifestPathFor(const QString &exportId)
{
    return QDir::temp().filePath(QStringLiteral("flappedear-export-%1.manifest.json").arg(exportId));
}

bool ExportArtifactManifest::create(const ExportArtifactManifestData &data, QString *error)
{
    if (!validOwnedData(manifestPathFor(data.exportId), data)) {
        if (error) *error = QStringLiteral("Export manifest ownership data is invalid.");
        return false;
    }
    return update(manifestPathFor(data.exportId), data, error);
}

bool ExportArtifactManifest::update(const QString &manifestPath, const ExportArtifactManifestData &data, QString *error)
{
    if (!validOwnedData(manifestPath, data)) {
        if (error) *error = QStringLiteral("Refusing to write an invalid export ownership manifest.");
        return false;
    }
    QSaveFile file(manifestPath);
    if (!file.open(QIODevice::WriteOnly)
        || file.write(QJsonDocument(asJson(data)).toJson(QJsonDocument::Compact)) < 0 || !file.commit()) {
        if (error) *error = QStringLiteral("Could not update export ownership manifest: %1").arg(file.errorString());
        return false;
    }
    return true;
}

bool ExportArtifactManifest::read(const QString &manifestPath, ExportArtifactManifestData *data, QString *error)
{
    const auto loaded = BoundedJsonLoader::loadFile(
        manifestPath, ProjectLimits::manifestBytes, QStringLiteral("Export ownership manifest"));
    if (!loaded.success() || !loaded.document.isObject()) { if (error) *error = loaded.error; return false; }
    const QJsonObject object = loaded.document.object();
    ExportArtifactManifestData parsed{object.value("exportId").toString(),
        object.value("createdUtcMilliseconds").toInteger(), object.value("temporaryOverlayPath").toString(),
        object.value("outputStagingPath").toString(), object.value("finalTargetPath").toString(),
        object.value("workerPid").toInteger(), object.value("state").toString()};
    if (object.value("version").toInt() != version || !validOwnedData(manifestPath, parsed)) {
        if (error) *error = QStringLiteral("Manifest is malformed or does not prove artifact ownership.");
        return false;
    }
    if (data) *data = parsed;
    return true;
}

bool ExportArtifactManifest::cleanupOwned(const QString &manifestPath, QString *error)
{
    if (!QFileInfo::exists(manifestPath)) {
        if (error) error->clear();
        return true;
    }
    ExportArtifactManifestData data;
    if (!read(manifestPath, &data, error)) return false;
    bool clean = true;
    for (const QString &path : {data.temporaryOverlayPath, data.outputStagingPath}) {
        const QFileInfo artifact(path);
        if ((artifact.exists() || artifact.isSymLink()) && !QFile::remove(path)) clean = false;
    }
    if (clean && QFileInfo::exists(manifestPath) && !QFile::remove(manifestPath)) clean = false;
    if (!clean && error) *error = QStringLiteral("Owned export artifacts could not be fully removed.");
    return clean;
}

QStringList ExportArtifactManifest::recoverStale(QStringList *diagnostics)
{
    QStringList cleaned;
    QDirIterator it(QDir::tempPath(), {QStringLiteral("flappedear-export-*.manifest.json")}, QDir::Files);
    while (it.hasNext()) {
        const QString path = it.next();
        ExportArtifactManifestData data;
        QString error;
        if (!read(path, &data, &error)) { if (diagnostics) diagnostics->append(QStringLiteral("Skipped %1: %2").arg(path, error)); continue; }
        if (processIsActive(data.workerPid)) { if (diagnostics) diagnostics->append(QStringLiteral("Kept active export manifest %1").arg(path)); continue; }
#ifdef Q_OS_UNIX
        // The worker PID is also its isolated process-group ID. A stale leader
        // does not authorize deleting files still owned by surviving writers.
        if (data.workerPid > 1 && data.workerPid <= std::numeric_limits<pid_t>::max()
            && (::kill(-static_cast<pid_t>(data.workerPid), 0) == 0 || errno != ESRCH)) {
            if (diagnostics) diagnostics->append(QStringLiteral("Kept active export group manifest %1").arg(path));
            continue;
        }
#endif
        if (cleanupOwned(path, &error)) cleaned.append(path);
        else if (diagnostics) diagnostics->append(QStringLiteral("Could not clean %1: %2").arg(path, error));
    }
    return cleaned;
}

bool ExportArtifactManifest::processIsActive(const qint64 pid)
{
    if (pid <= 0) return false;
#ifdef Q_OS_UNIX
    return ::kill(static_cast<pid_t>(pid), 0) == 0 || errno == EPERM;
#elif defined(Q_OS_WIN)
    HANDLE process = OpenProcess(SYNCHRONIZE, FALSE, static_cast<DWORD>(pid));
    if (!process) return false;
    const DWORD status = WaitForSingleObject(process, 0);
    CloseHandle(process);
    return status == WAIT_TIMEOUT;
#else
    return false;
#endif
}

} // namespace FlappedEar
