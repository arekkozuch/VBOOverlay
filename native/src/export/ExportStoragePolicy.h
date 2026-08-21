#pragma once

#include <QString>
#include <QSize>
#include <functional>

namespace FlappedEar {

struct ExportFilesystemInfo {
    QString rootPath;
    QString inspectedPath;
    qint64 availableBytes = -1;
    qint64 totalBytes = -1;
    [[nodiscard]] bool isUsable() const { return !rootPath.isEmpty() && availableBytes >= 0; }
};

struct ExportStorageEstimate {
    qint64 temporaryOverlayBytes = 0;
    qint64 finalOutputBytes = 0;
    qint64 safetyReserveBytes = 0;
};

struct ExportStoragePreflight {
    ExportFilesystemInfo temporaryFilesystem;
    ExportFilesystemInfo destinationFilesystem;
    ExportStorageEstimate estimate;
    bool sufficient = false;
    QString error;
};

class ExportStoragePolicy final {
public:
    using FilesystemProvider = std::function<ExportFilesystemInfo(const QString &path)>;

    [[nodiscard]] static ExportFilesystemInfo filesystemForPath(const QString &path);
    [[nodiscard]] static ExportStorageEstimate estimate(
        qsizetype frameCount, const QSize &size, double durationSeconds, const QString &quality);
    [[nodiscard]] static ExportStoragePreflight evaluate(
        const QString &temporaryPath, const QString &destinationPath,
        const ExportStorageEstimate &estimate, const FilesystemProvider &provider = {});
    [[nodiscard]] static bool criticallyLow(const ExportFilesystemInfo &filesystem,
                                            qint64 safetyReserveBytes);
    [[nodiscard]] static QString bytesText(qint64 bytes);
};

} // namespace FlappedEar
