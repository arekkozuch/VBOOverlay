#include "export/ExportFormat.h"

#include <algorithm>
#include <cmath>

namespace FlappedEar {
namespace {
QSize evenFit(const QSize &source, int targetWidth)
{
    if (targetWidth > source.width()) return {};
    const int width = targetWidth - targetWidth % 2;
    const int height = qMax(2, static_cast<int>(std::lround(double(source.height()) * width / source.width())) & ~1);
    return {width, height};
}
}

QList<QSize> ExportFormat::resolutionOptions(const QSize &source)
{
    QList<QSize> result;
    if (!source.isValid()) return result;
    const QSize normalized{source.width() & ~1, source.height() & ~1};
    result.append(normalized);
    for (const int width : {3840, 2560, 1920, 1280}) {
        const QSize candidate = evenFit(source, width);
        if (candidate.isValid() && candidate != normalized && !result.contains(candidate)) result.append(candidate);
    }
    return result;
}

QList<MediaRational> ExportFormat::frameRateOptions(const MediaRational &source)
{
    QList<MediaRational> result;
    if (!source.isValid()) return result;
    result.append(source);
    if (source.numerator % 2 == 0) {
        const MediaRational half{source.numerator / 2, source.denominator};
        if (half.isValid()) result.append(half);
    }
    return result;
}

qint64 ExportFormat::recommendedVideoBitrate(const QSize &size, const MediaRational &rate)
{
    if (!size.isValid() || !rate.isValid()) return 0;
    // 0.50 bits/pixel/frame: intentionally conservative for fast-moving motorsport footage.
    return qBound<qint64>(qint64{1'000'000}, qRound64(size.width() * double(size.height()) * rate.value() * 0.50), qint64{120'000'000});
}

qint64 ExportFormat::bitrateForQuality(const QString &quality, const QSize &size, const MediaRational &rate)
{
    const double multiplier = quality == QStringLiteral("smaller") ? 0.70
        : quality == QStringLiteral("high") ? 1.30 : 1.0;
    return qRound64(recommendedVideoBitrate(size, rate) * multiplier);
}

bool ExportFormat::validCustomBitrate(const qint64 bitrate)
{
    return bitrate >= 500'000 && bitrate <= 120'000'000;
}

qint64 ExportFormat::estimatedBytes(const qint64 videoBitrate, const bool audioEnabled, const double seconds)
{
    if (!validCustomBitrate(videoBitrate) || !std::isfinite(seconds) || seconds < 0) return 0;
    return qRound64((videoBitrate + (audioEnabled ? audioBitrate : 0)) * seconds / 8.0 * 1.03);
}
} // namespace FlappedEar
