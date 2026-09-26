#include "telemetry/DrivingVariability.h"

#include <algorithm>
#include <cmath>

namespace FlappedEar {

CornerVariability summarizeCornerVariability(const QString &segmentId, const QString &name,
    const QVector<CornerLapObservation> &observations, const qsizetype minimumSamples)
{
    CornerVariability result;
    result.segmentId = segmentId;
    result.name = name;
    QVector<double> brakingMeasured, brakingInferred, apex, minimum, exit, pickupMeasured, pickupInferred, line, accuracy;
    for (const auto &lap : observations) {
        if (lap.brakingPointMeters) {
            if (lap.brakingProvenance == QLatin1String("measured")) brakingMeasured.append(*lap.brakingPointMeters);
            else if (lap.brakingProvenance == QLatin1String("inferred")) brakingInferred.append(*lap.brakingPointMeters);
        }
        if (lap.pickupMeters) {
            if (lap.pickupProvenance == QLatin1String("measured")) pickupMeasured.append(*lap.pickupMeters);
            else if (lap.pickupProvenance == QLatin1String("inferred")) pickupInferred.append(*lap.pickupMeters);
        }
        if (lap.apexSpeed) apex.append(*lap.apexSpeed);
        if (lap.minimumSpeed) minimum.append(*lap.minimumSpeed);
        if (lap.exitSpeed) exit.append(*lap.exitSpeed);
        if (lap.lineOffsetMeters) line.append(*lap.lineOffsetMeters);
        if (lap.gpsAccuracyMeters && std::isfinite(*lap.gpsAccuracyMeters) && *lap.gpsAccuracyMeters >= 0.0)
            accuracy.append(*lap.gpsAccuracyMeters);
    }
    result.brakingPointMeasured = summarizeConsistency(brakingMeasured, minimumSamples);
    result.brakingPointInferred = summarizeConsistency(brakingInferred, minimumSamples);
    result.apexSpeed = summarizeConsistency(apex, minimumSamples);
    result.minimumSpeed = summarizeConsistency(minimum, minimumSamples);
    result.exitSpeed = summarizeConsistency(exit, minimumSamples);
    result.pickupMeasured = summarizeConsistency(pickupMeasured, minimumSamples);
    result.pickupInferred = summarizeConsistency(pickupInferred, minimumSamples);
    result.lineOffset = summarizeConsistency(line, minimumSamples);
    const auto typicalAccuracy = summarizeConsistency(accuracy, 1);
    if (typicalAccuracy.available) result.typicalGpsAccuracyMeters = typicalAccuracy.median;
    // Without a stated accuracy there is nothing to compare against: the
    // spread is reported but never claimed resolvable.
    result.lineSpreadResolvable = result.lineOffset.available && result.typicalGpsAccuracyMeters
        && *result.lineOffset.interquartileRange > *result.typicalGpsAccuracyMeters;
    return result;
}

std::optional<double> lateralOffsetMeters(const ProgressAxis &axis, const double progressMeters, const QPointF &localPoint)
{
    if (!axis.valid || axis.points.size() < 2 || axis.cumulative.size() != axis.points.size()
        || !std::isfinite(progressMeters) || progressMeters < 0.0 || progressMeters > axis.lengthMeters)
        return std::nullopt;
    const auto upper = std::lower_bound(axis.cumulative.cbegin(), axis.cumulative.cend(), progressMeters);
    qsizetype index = std::clamp<qsizetype>(std::distance(axis.cumulative.cbegin(), upper), 1, axis.points.size() - 1);
    const QPointF a = axis.points[index - 1], b = axis.points[index];
    const double span = axis.cumulative[index] - axis.cumulative[index - 1];
    const double fraction = span > 0.0 ? std::clamp((progressMeters - axis.cumulative[index - 1]) / span, 0.0, 1.0) : 0.0;
    const QPointF onAxis = a + (b - a) * fraction;
    const QPointF direction = b - a;
    const double length = std::hypot(direction.x(), direction.y());
    if (length <= 0.0) return std::nullopt;
    const QPointF offset = localPoint - onAxis;
    return (direction.x() * offset.y() - direction.y() * offset.x()) / length;
}

} // namespace FlappedEar
