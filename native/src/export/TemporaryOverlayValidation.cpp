#include "export/TemporaryOverlayValidation.h"

#include <QtGlobal>
#include <cmath>

namespace FlappedEar {
namespace {

double timestampTolerance(const MediaRational &timeBase, const MediaRational &scheduledRate)
{
    // Matroska commonly stores this FFV1 stream at 1 ms precision. If a
    // container omits a usable time base, one scheduled frame is the narrowest
    // defensible fallback rather than an arbitrary rate epsilon.
    return timeBase.isValid() ? timeBase.value() : 1.0 / scheduledRate.value();
}

bool rateWithinTimestampTolerance(
    const MediaRational &reportedRate, const qsizetype expectedFrames,
    const double scheduledDuration, const double durationTolerance)
{
    if (!reportedRate.isValid() || expectedFrames == 0 || !std::isfinite(scheduledDuration)
        || scheduledDuration <= durationTolerance) {
        return false;
    }
    const double minimumPossibleRate = expectedFrames / (scheduledDuration + durationTolerance);
    const double maximumPossibleRate = expectedFrames / (scheduledDuration - durationTolerance);
    return reportedRate.value() >= minimumPossibleRate && reportedRate.value() <= maximumPossibleRate;
}

} // namespace

bool TemporaryOverlayValidationResult::passed() const
{
    return codecOk && dimensionsOk && nominalRateOk && averageRateOk && durationOk && startTimeOk;
}

TemporaryOverlayValidationResult TemporaryOverlayValidation::validate(
    const MediaInfo &overlay, const QSize &expectedSize, const MediaRational &scheduledRate,
    const qsizetype expectedFrames, const double scheduledDuration)
{
    TemporaryOverlayValidationResult result;
    if (!scheduledRate.isValid() || expectedFrames == 0 || !std::isfinite(scheduledDuration)
        || scheduledDuration <= 0.0) {
        return result;
    }
    const double durationTolerance = timestampTolerance(overlay.timeBase, scheduledRate);
    result.timestampToleranceMicroseconds = qRound64(durationTolerance * 1'000'000.0);
    result.codecOk = overlay.videoCodec == QStringLiteral("ffv1");
    result.dimensionsOk = overlay.videoSize == expectedSize;
    result.nominalRateExact = overlay.frameRate.isEquivalentTo(scheduledRate);
    result.nominalRateOk = result.nominalRateExact
        || rateWithinTimestampTolerance(
            overlay.frameRate, expectedFrames, scheduledDuration, durationTolerance);
    result.averageRateOk = rateWithinTimestampTolerance(
        overlay.averageFrameRate, expectedFrames, scheduledDuration, durationTolerance);
    result.durationOk = std::isfinite(overlay.duration)
        && qAbs(overlay.duration - scheduledDuration) <= durationTolerance;
    result.startTimeOk = std::isfinite(overlay.videoStartTime)
        && qAbs(overlay.videoStartTime) <= durationTolerance;
    const double lowerRate = expectedFrames / (scheduledDuration + durationTolerance);
    result.rateTolerance = scheduledRate.value() - lowerRate;
    return result;
}

} // namespace FlappedEar
