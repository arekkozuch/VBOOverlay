#pragma once

#include "telemetry/Consistency.h"
#include "telemetry/TrackProgress.h"

#include <QJsonObject>
#include <QString>
#include <QVector>
#include <optional>

namespace FlappedEar {

// Braking, apex, exit and racing-line variability for one corner across a
// population of laps (KAN-63). Each lap contributes the same Corner Analyzer
// metrics (KAN-52/53/54) measured on one shared axis; each metric is
// summarized with the KAN-62 statistics (median, interquartile range, count).
// Measured and inferred braking points and throttle pickups are summarized
// separately and never mixed. The racing-line spread is the lateral offset
// of each lap from the reference line at the corner's geometric apex; it is
// reported next to the recording's typical GPS accuracy, and flagged as not
// resolvable when it is no larger than that accuracy.
inline constexpr auto drivingVariabilityAlgorithm = "driving-variability-v1";

struct CornerLapObservation {
    QJsonObject lapReference;
    std::optional<double> brakingPointMeters;
    QString brakingProvenance; // measured / inferred
    std::optional<double> apexSpeed;
    std::optional<double> minimumSpeed;
    std::optional<double> exitSpeed;
    std::optional<double> pickupMeters;
    QString pickupProvenance; // measured / inferred
    std::optional<double> lineOffsetMeters;  // + left of the reference line
    std::optional<double> gpsAccuracyMeters; // the recording's own accuracy at that point
};

struct CornerVariability {
    QString segmentId;
    QString name;
    ConsistencySummary brakingPointMeasured;
    ConsistencySummary brakingPointInferred;
    ConsistencySummary apexSpeed;
    ConsistencySummary minimumSpeed;
    ConsistencySummary exitSpeed;
    ConsistencySummary pickupMeasured;
    ConsistencySummary pickupInferred;
    ConsistencySummary lineOffset;
    std::optional<double> typicalGpsAccuracyMeters; // median accuracy of the observations
    // True when the line spread (IQR) is larger than the typical GPS accuracy,
    // i.e. the difference in line can be told apart from GPS noise.
    bool lineSpreadResolvable = false;
};

[[nodiscard]] CornerVariability summarizeCornerVariability(const QString &segmentId, const QString &name,
    const QVector<CornerLapObservation> &observations, qsizetype minimumSamples = minimumConsistencySamples);

// Signed lateral distance (metres, + to the left of travel) of `localPoint`
// from the axis at `progressMeters`; nullopt outside the axis.
[[nodiscard]] std::optional<double> lateralOffsetMeters(
    const ProgressAxis &axis, double progressMeters, const QPointF &localPoint);

} // namespace FlappedEar
