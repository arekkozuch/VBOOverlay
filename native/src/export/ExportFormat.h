#pragma once

#include "export/MediaProbe.h"
#include <QSize>
#include <optional>

namespace FlappedEar {

// Centralized, deliberately simple HEVC policy for high-motion onboard footage.
class ExportFormat final {
public:
    static constexpr qint64 audioBitrate = 192'000;
    static constexpr qint64 maximumCustomVideoBitrate = 500'000'000;
    [[nodiscard]] static QList<QSize> resolutionOptions(const QSize &source);
    [[nodiscard]] static QList<MediaRational> frameRateOptions(const MediaRational &source);
    [[nodiscard]] static qint64 recommendedVideoBitrate(
        const QSize &size, const MediaRational &rate, int bitDepth = 8);
    [[nodiscard]] static qint64 bitrateForQuality(
        const QString &quality, const QSize &size, const MediaRational &rate,
        int bitDepth = 8);
    [[nodiscard]] static bool validCustomBitrate(qint64 bitrate);
    [[nodiscard]] static std::optional<qint64> rgbaFrameBytes(const QSize &size);
    [[nodiscard]] static qint64 estimatedBytes(qint64 videoBitrate, bool audioEnabled, double seconds);
    [[nodiscard]] static QString formatEstimatedSize(qint64 bytes);
};

} // namespace FlappedEar
