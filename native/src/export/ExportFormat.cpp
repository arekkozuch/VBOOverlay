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
    // HEVC baselines for high-motion onboard footage, at 30 fps.  The square-root
    // cadence adjustment retains extra temporal detail without treating 60 fps as
    // a full 2x bitrate requirement.
    const qint64 baseline = size.height() <= 720 ? 6'000'000
        : size.height() <= 1080 ? 12'500'000
        : size.height() <= 1440 ? 21'200'000
        : 36'500'000;
    const double fpsMultiplier = std::sqrt(rate.value() / 30.0);
    return qBound<qint64>(qint64{1'000'000}, qRound64(baseline * fpsMultiplier), qint64{120'000'000});
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

QString ExportFormat::formatEstimatedSize(const qint64 bytes)
{
    if (bytes <= 0) return QStringLiteral("~0 MiB");
    const double mebibytes = bytes / (1024.0 * 1024.0);
    if (mebibytes < 1024.0)
        return QStringLiteral("~%1 MiB").arg(QString::number(mebibytes, 'f', mebibytes < 100.0 ? 1 : 0));
    const double gibibytes = mebibytes / 1024.0;
    return QStringLiteral("~%1 GiB").arg(QString::number(gibibytes, 'f', gibibytes < 10.0 ? 2 : 1));
}
} // namespace FlappedEar
