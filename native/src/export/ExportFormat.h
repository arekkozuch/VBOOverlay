#pragma once

#include "export/MediaProbe.h"
#include <QSize>

namespace FlappedEar {

// Centralized, deliberately simple HEVC policy for high-motion onboard footage.
class ExportFormat final {
public:
    static constexpr qint64 audioBitrate = 192'000;
    [[nodiscard]] static QList<QSize> resolutionOptions(const QSize &source);
    [[nodiscard]] static QList<MediaRational> frameRateOptions(const MediaRational &source);
    [[nodiscard]] static qint64 recommendedVideoBitrate(const QSize &size, const MediaRational &rate);
    [[nodiscard]] static qint64 bitrateForQuality(const QString &quality, const QSize &size, const MediaRational &rate);
    [[nodiscard]] static bool validCustomBitrate(qint64 bitrate);
    [[nodiscard]] static qint64 estimatedBytes(qint64 videoBitrate, bool audioEnabled, double seconds);
};

} // namespace FlappedEar
