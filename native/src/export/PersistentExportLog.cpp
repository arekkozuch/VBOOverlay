#include "export/PersistentExportLog.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>

#include <algorithm>

namespace FlappedEar {
namespace {
const QRegularExpression kExportLogName(
    QStringLiteral(R"(^export-\d{8}-\d{6}-[0-9a-fA-F]{8,}\.log$)"));
} // namespace

PersistentExportLog::PersistentExportLog(std::unique_ptr<QFile> file) : m_file(std::move(file)) {}
PersistentExportLog::~PersistentExportLog() = default;

QString PersistentExportLog::fileName(const QDateTime &started, const QString &exportId)
{
    return QStringLiteral("export-%1-%2.log")
        .arg(started.toString(QStringLiteral("yyyyMMdd-hhmmss")), exportId);
}

std::unique_ptr<PersistentExportLog> PersistentExportLog::create(
    const QString &directory, const QString &exportId, const QString &header,
    QString *error, const QDateTime &started)
{
    QDir dir(directory);
    if (!dir.exists() && !QDir().mkpath(directory)) {
        if (error) *error = QStringLiteral("Could not create %1").arg(directory);
        return {};
    }
    auto file = std::make_unique<QFile>(dir.filePath(fileName(started, exportId)));
    if (!file->open(QIODevice::WriteOnly | QIODevice::NewOnly | QIODevice::Text)) {
        if (error) *error = file->errorString();
        return {};
    }
    auto log = std::unique_ptr<PersistentExportLog>(new PersistentExportLog(std::move(file)));
    if (!log->append(header)) {
        if (error) *error = log->m_file->errorString();
        return {};
    }
    return log;
}

QString PersistentExportLog::path() const { return m_file ? m_file->fileName() : QString(); }

bool PersistentExportLog::append(const QString &entry)
{
    if (!m_file || !m_file->isOpen()) return false;
    QString line = entry;
    if (!line.endsWith(QLatin1Char('\n'))) line.append(QLatin1Char('\n'));
    return m_file->write(line.toUtf8()) >= 0 && m_file->flush();
}

void PersistentExportLog::retainNewest(
    const QString &directory, const QString &activePath, const int maximumFiles)
{
    QDir dir(directory);
    QList<QFileInfo> logs;
    for (const QFileInfo &entry : dir.entryInfoList({QStringLiteral("export-*.log")}, QDir::Files)) {
        if (kExportLogName.match(entry.fileName()).hasMatch()) logs.append(entry);
    }
    std::sort(logs.begin(), logs.end(), [](const QFileInfo &left, const QFileInfo &right) {
        return left.fileName() > right.fileName();
    });
    int kept = 0;
    for (const QFileInfo &entry : logs) {
        if (entry.absoluteFilePath() == activePath || kept++ < maximumFiles) continue;
        QFile::remove(entry.absoluteFilePath());
    }
}

} // namespace FlappedEar
