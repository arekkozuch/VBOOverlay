#include "export/ExportStoragePolicy.h"

#include <QDir>
#include <QFileInfo>
#include <QStorageInfo>

#include <algorithm>
#include <cmath>
#include <limits>

namespace FlappedEar {
namespace {
constexpr qint64 kGiB = 1024LL * 1024LL * 1024LL;
// The fallback is intentionally still far above the observed 4K telemetry
// overlay (~71 KiB/frame), while avoiding a false 50+ GiB prediction for a
// mostly transparent QML scene when measurement is unavailable.
constexpr qint64 kFallbackOverlayBytesPerFrame = 512LL * 1024LL;
constexpr double kFallbackOverlaySafetyMargin = 1.75;
constexpr double kMeasuredOverlaySafetyMargin = 1.50;
constexpr double kOutputSafetyMargin = 1.25;
constexpr qint64 kMinimumReserve = 2LL * kGiB;

qint64 saturatedMultiply(const qint64 lhs, const qint64 rhs)
{
    if (lhs <= 0 || rhs <= 0) return 0;
    if (lhs > std::numeric_limits<qint64>::max() / rhs) return std::numeric_limits<qint64>::max();
    return lhs * rhs;
}

qint64 withMargin(const qint64 value, const double margin)
{
    if (value <= 0) return 0;
    const long double expanded = static_cast<long double>(value) * margin;
    return expanded >= static_cast<long double>(std::numeric_limits<qint64>::max())
        ? std::numeric_limits<qint64>::max() : static_cast<qint64>(std::ceil(expanded));
}

}

ExportFilesystemInfo ExportStoragePolicy::filesystemForPath(const QString &path)
{
    ExportFilesystemInfo result;
    result.inspectedPath = path;
    if (path.trimmed().isEmpty()) return result;

    QString candidate = QDir::cleanPath(QFileInfo(path).absoluteFilePath());
    while (!QFileInfo::exists(candidate)) {
        const QString parent = QFileInfo(candidate).absolutePath();
        if (parent == candidate || parent.isEmpty()) return result;
        candidate = parent;
    }

    result.probePath = candidate;
    const QStorageInfo storage(candidate);
    if (!storage.isValid() || !storage.isReady()
        || storage.bytesAvailable() < 0 || storage.bytesTotal() < 0) {
        return result;
    }
    result.rootPath = storage.rootPath();
    result.availableBytes = storage.bytesAvailable();
    result.totalBytes = storage.bytesTotal();
    return result;
}

ExportStorageEstimate ExportStoragePolicy::estimate(
    const qsizetype frameCount, const QSize &size, const double durationSeconds, const qint64 videoBitrate)
{
    // FFV1 content size is intentionally not presented as an exact prediction.
    // Until this export has a measured sample, use a deliberately conservative
    // six MiB/frame fallback (well below raw RGBA but above typical UI overlays).
    const qint64 overlayRaw = saturatedMultiply(static_cast<qint64>(frameCount), kFallbackOverlayBytesPerFrame);
    const qint64 finalRaw = static_cast<qint64>(std::ceil(
        qMax(0.0, durationSeconds) * static_cast<double>(videoBitrate) / 8.0));
    Q_UNUSED(size);
    return {withMargin(overlayRaw, kFallbackOverlaySafetyMargin), withMargin(finalRaw, kOutputSafetyMargin),
            kMinimumReserve, kFallbackOverlayBytesPerFrame, 0, 0, kFallbackOverlaySafetyMargin,
            ExportStorageEstimate::Basis::ConservativeFallback};
}

ExportStorageEstimate ExportStoragePolicy::estimateFromSample(
    const qint64 sampleBytes, const qsizetype sampleFrames, const qsizetype expectedFrames,
    const double durationSeconds, const qint64 videoBitrate)
{
    if (sampleBytes <= 0 || sampleFrames <= 0) {
        return estimate(expectedFrames, {}, durationSeconds, videoBitrate);
    }
    const qint64 frameCount = static_cast<qint64>(sampleFrames);
    const qint64 bytesPerFrame = sampleBytes / frameCount + (sampleBytes % frameCount == 0 ? 0 : 1);
    const qint64 overlayRaw = saturatedMultiply(bytesPerFrame, static_cast<qint64>(expectedFrames));
    const qint64 finalRaw = static_cast<qint64>(std::ceil(
        qMax(0.0, durationSeconds) * static_cast<double>(videoBitrate) / 8.0));
    return {withMargin(overlayRaw, kMeasuredOverlaySafetyMargin), withMargin(finalRaw, kOutputSafetyMargin),
            kMinimumReserve, bytesPerFrame, sampleFrames, sampleBytes, kMeasuredOverlaySafetyMargin,
            ExportStorageEstimate::Basis::MeasuredSample};
}

QString ExportStoragePolicy::estimateBasisText(const ExportStorageEstimate::Basis basis)
{
    return basis == ExportStorageEstimate::Basis::MeasuredSample
        ? QStringLiteral("Measured sample") : QStringLiteral("Conservative fallback");
}

ExportStoragePreflight ExportStoragePolicy::evaluate(
    const QString &temporaryPath, const QString &destinationPath,
    const ExportStorageEstimate &storageEstimate, const FilesystemProvider &provider)
{
    const FilesystemProvider effectiveProvider = provider ? provider : filesystemForPath;
    ExportStoragePreflight result;
    result.temporaryFilesystem = effectiveProvider(temporaryPath);
    result.destinationFilesystem = effectiveProvider(destinationPath);
    result.estimate = storageEstimate;
    if (!result.temporaryFilesystem.isUsable() || !result.destinationFilesystem.isUsable()) {
        result.error = QStringLiteral("Could not determine free space for export.");
        return result;
    }
    const bool sameFilesystem = result.temporaryFilesystem.rootPath == result.destinationFilesystem.rootPath;
    const qint64 temporaryRequired = storageEstimate.temporaryOverlayBytes + storageEstimate.safetyReserveBytes;
    const qint64 outputRequired = storageEstimate.finalOutputBytes + storageEstimate.safetyReserveBytes;
    const qint64 combinedRequired = storageEstimate.temporaryOverlayBytes + storageEstimate.finalOutputBytes
        + storageEstimate.safetyReserveBytes;
    const bool enough = sameFilesystem
        ? result.temporaryFilesystem.availableBytes >= combinedRequired
        : result.temporaryFilesystem.availableBytes >= temporaryRequired
            && result.destinationFilesystem.availableBytes >= outputRequired;
    result.sufficient = enough;
    if (!enough) {
        const ExportFilesystemInfo &affected = sameFilesystem || result.temporaryFilesystem.availableBytes < temporaryRequired
            ? result.temporaryFilesystem : result.destinationFilesystem;
        const qint64 required = sameFilesystem ? combinedRequired
            : (&affected == &result.temporaryFilesystem ? temporaryRequired : outputRequired);
        result.error = QStringLiteral("Not enough free space for export. Required estimate: %1; available: %2; filesystem: %3 (path: %4).")
                           .arg(bytesText(required), bytesText(affected.availableBytes), affected.rootPath,
                                affected.inspectedPath);
    }
    return result;
}

bool ExportStoragePolicy::criticallyLow(const ExportFilesystemInfo &filesystem,
                                        const qint64 safetyReserveBytes)
{
    return filesystem.isUsable() && filesystem.availableBytes < safetyReserveBytes;
}

QString ExportStoragePolicy::bytesText(const qint64 bytes)
{
    if (bytes < 0) return QStringLiteral("unavailable");
    return QStringLiteral("%1 GiB").arg(static_cast<double>(bytes) / static_cast<double>(kGiB), 0, 'f', 2);
}

} // namespace FlappedEar
