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
    enum class Basis { MeasuredSample, ConservativeFallback };
    qint64 temporaryOverlayBytes = 0;
    qint64 finalOutputBytes = 0;
    qint64 safetyReserveBytes = 0;
    qint64 bytesPerFrame = 0;
    qsizetype sampleFrames = 0;
    qint64 sampleBytes = 0;
    double safetyMargin = 1.0;
    Basis basis = Basis::ConservativeFallback;
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
    [[nodiscard]] static ExportStorageEstimate estimateFromSample(
        qint64 sampleBytes, qsizetype sampleFrames, qsizetype expectedFrames,
        double durationSeconds, const QString &quality);
    [[nodiscard]] static QString estimateBasisText(ExportStorageEstimate::Basis basis);
    [[nodiscard]] static ExportStoragePreflight evaluate(
        const QString &temporaryPath, const QString &destinationPath,
        const ExportStorageEstimate &estimate, const FilesystemProvider &provider = {});
    [[nodiscard]] static bool criticallyLow(const ExportFilesystemInfo &filesystem,
                                            qint64 safetyReserveBytes);
    [[nodiscard]] static QString bytesText(qint64 bytes);
};

} // namespace FlappedEar
