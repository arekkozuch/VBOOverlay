#pragma once

#include <QDateTime>
#include <QString>

#include <memory>

class QFile;

namespace FlappedEar {

class PersistentExportLog final {
public:
    static std::unique_ptr<PersistentExportLog> create(
        const QString &directory, const QString &exportId, const QString &header,
        QString *error = nullptr, const QDateTime &started = QDateTime::currentDateTime());
    static QString fileName(const QDateTime &started, const QString &exportId);
    static void retainNewest(const QString &directory, const QString &activePath, int maximumFiles = 10);

    ~PersistentExportLog();
    PersistentExportLog(const PersistentExportLog &) = delete;
    PersistentExportLog &operator=(const PersistentExportLog &) = delete;

    [[nodiscard]] QString path() const;
    bool append(const QString &entry);

private:
    explicit PersistentExportLog(std::unique_ptr<QFile> file);
    std::unique_ptr<QFile> m_file;
};

} // namespace FlappedEar
