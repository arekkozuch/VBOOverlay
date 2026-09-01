#pragma once

#include "export/MediaProbe.h"

#include <QRect>
#include <QSize>

#include <optional>

namespace FlappedEar {

class PreviewPlayback final {
public:
    [[nodiscard]] static QRect aspectFitViewport(const QSize &available, const QSize &source);
    [[nodiscard]] static std::optional<qint64> lastFrame(qsizetype sourceFrameCount);
    [[nodiscard]] static std::optional<qint64> framePositionMilliseconds(
        qint64 frame, const MediaRational &frameRate);
    // Qt Multimedia seeks in whole milliseconds. This returns the first seek position
    // whose displayed SMPTE frame is 1, rather than leaving the user at frame 0.
    [[nodiscard]] static std::optional<qint64> firstTimelineFramePositionMilliseconds(
        const MediaRational &frameRate);
    [[nodiscard]] static std::optional<qint64> clampPositionMilliseconds(
        qint64 requestedMilliseconds, qint64 lastFrame, const MediaRational &frameRate);
};

} // namespace FlappedEar
