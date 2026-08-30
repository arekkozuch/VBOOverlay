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
    [[nodiscard]] static std::optional<qint64> clampPositionMilliseconds(
        qint64 requestedMilliseconds, qint64 lastFrame, const MediaRational &frameRate);
};

} // namespace FlappedEar
