#include "app/PreviewPlayback.h"

#include <limits>
#include <numeric>

namespace FlappedEar {
namespace {

std::optional<qint64> multiplyDivideFloor(qint64 left, qint64 middle, qint64 right, qint64 denominator)
{
    if (left < 0 || middle < 0 || right < 0 || denominator <= 0) return std::nullopt;
    qint64 factors[] = {left, middle, right};
    for (qint64 &factor : factors) {
        const qint64 divisor = std::gcd(factor, denominator);
        factor /= divisor;
        denominator /= divisor;
    }
    qint64 product = 1;
    for (const qint64 factor : factors) {
        if (factor != 0 && product > std::numeric_limits<qint64>::max() / factor) return std::nullopt;
        product *= factor;
    }
    return product / denominator;
}

} // namespace

QRect PreviewPlayback::aspectFitViewport(const QSize &available, const QSize &source)
{
    if (!available.isValid() || !source.isValid()) return {};
    const qint64 availableWidth = available.width();
    const qint64 availableHeight = available.height();
    const qint64 sourceWidth = source.width();
    const qint64 sourceHeight = source.height();
    qint64 width = availableWidth;
    qint64 height = availableHeight;
    if (availableWidth * sourceHeight <= availableHeight * sourceWidth) {
        height = availableWidth * sourceHeight / sourceWidth;
    } else {
        width = availableHeight * sourceWidth / sourceHeight;
    }
    if (width <= 0 || height <= 0 || width > std::numeric_limits<int>::max()
        || height > std::numeric_limits<int>::max()) return {};
    return {static_cast<int>((availableWidth - width) / 2),
            static_cast<int>((availableHeight - height) / 2),
            static_cast<int>(width), static_cast<int>(height)};
}

std::optional<qint64> PreviewPlayback::lastFrame(const qsizetype sourceFrameCount)
{
    if (sourceFrameCount == 0 || sourceFrameCount > static_cast<qsizetype>(std::numeric_limits<qint64>::max())) return std::nullopt;
    return static_cast<qint64>(sourceFrameCount) - 1;
}

std::optional<qint64> PreviewPlayback::framePositionMilliseconds(
    const qint64 frame, const MediaRational &frameRate)
{
    if (frame < 0 || !frameRate.isValid()) return std::nullopt;
    return multiplyDivideFloor(frame, frameRate.denominator, 1'000, frameRate.numerator);
}

std::optional<qint64> PreviewPlayback::firstTimelineFramePositionMilliseconds(
    const MediaRational &frameRate)
{
    if (!frameRate.isValid()
        || frameRate.denominator > std::numeric_limits<qint64>::max() / 1'000) {
        return std::nullopt;
    }
    const qint64 scaledDenominator = frameRate.denominator * 1'000;
    if (scaledDenominator > std::numeric_limits<qint64>::max() - frameRate.numerator + 1) {
        return std::nullopt;
    }
    // Presentation-only inverse mapping from the millisecond player position to
    // frame index one. Export frame boundaries remain exact integer/rational values.
    return (scaledDenominator + frameRate.numerator - 1) / frameRate.numerator;
}

std::optional<qint64> PreviewPlayback::clampPositionMilliseconds(
    const qint64 requestedMilliseconds, const qint64 lastVideoFrame, const MediaRational &frameRate)
{
    const auto lastPosition = framePositionMilliseconds(lastVideoFrame, frameRate);
    if (!lastPosition) return std::nullopt;
    return qBound<qint64>(0, requestedMilliseconds, *lastPosition);
}

} // namespace FlappedEar
