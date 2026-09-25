#pragma once

#include "telemetry/BrakingOnset.h"
#include "telemetry/TelemetrySession.h"
#include "telemetry/TrackProgress.h"
#include "telemetry/TrackSegmentReview.h"

#include <QString>
#include <QStringList>
#include <optional>

namespace FlappedEar {

// Braking metrics for one approved segment on one lap (KAN-53).
//
// Spatial reference: shared-axis progress. The search interval runs from
// `approachMeters` before the segment's start boundary (clipped at the gate)
// to its end boundary. The braking point is the first braking-onset candidate
// (KAN-47) inside that interval; its time and distance run from the onset to
// the end of that braking episode. Deceleration is read only from the recorded
// longitudinal-acceleration channel over the same episode. An interval bound
// that lies on the gate (a clipped approach, or a segment ending at the lap
// length) takes the lap's timed start or end: a lap's projection never lands
// on the gate exactly.
inline constexpr auto brakingMetricsAlgorithm = "braking-metrics-v1";

inline constexpr auto brakingNoneDetected = "noBrakingDetected";
inline constexpr auto brakingIncompleteCoverage = "incompleteCoverage";
inline constexpr auto brakingSegmentCrossesGate = "crossesGate";
inline constexpr auto brakingDecelerationChannelMissing = "decelerationChannelMissing";
inline constexpr auto brakingApproachClipped = "approachClippedAtGate";
inline constexpr auto brakingMixedProvenance = "mixedProvenance";

struct BrakingMetricsOptions {
    double approachMeters = 200.0;
    BrakingOnsetOptions onset;
};

struct BrakingMetrics {
    QString segmentId;
    double intervalStartMeters = 0.0;
    double intervalEndMeters = 0.0;
    double entryMeters = 0.0;
    QString unavailableReason; // no braking point at all

    // Braking point, from the onset detector.
    QString method;     // measuredBrake / inferredDeceleration
    QString provenance; // measured / inferred
    QString channel;
    QString thresholdUnit;
    double onThreshold = 0.0;
    std::optional<double> brakingPointMeters;
    std::optional<double> brakingPointTime;
    std::optional<double> distanceBeforeEntryMeters; // entry - braking point; negative inside the segment
    std::optional<double> brakingSeconds;
    std::optional<double> brakingDistanceMeters;     // only with projected coverage to the episode end
    QStringList limitations;                         // onset uncertainty, clipped approach

    // Deceleration over the braking episode (positive magnitudes, channel unit).
    QString decelerationChannel;
    QString decelerationUnit;
    std::optional<double> peakDeceleration;
    std::optional<double> meanDeceleration;
    QString decelerationUnavailableReason;

    SegmentationResultStamp stamp;
    bool valid = false;
};

[[nodiscard]] BrakingMetrics computeBrakingMetrics(double axisLengthMeters, const ApprovedSegmentation &approved,
    const QString &segmentId, const QVector<ProgressSegment> &lapTrace, const TelemetrySession &session,
    std::optional<double> lapStartTime, std::optional<double> lapEndTime, const BrakingMetricsOptions &options = {});

// A minus B for the same segment and approved revision. A braking-point delta
// is positive when A starts braking further along the lap (later). Values
// measured by different methods are never compared (`mixedProvenance`).
struct BrakingComparison {
    std::optional<double> brakingPointDeltaMeters;
    std::optional<double> brakingSecondsDelta;
    std::optional<double> brakingDistanceDeltaMeters;
    std::optional<double> peakDecelerationDelta;
    std::optional<double> meanDecelerationDelta;
    QString unavailableReason;
    bool valid = false;
};

[[nodiscard]] BrakingComparison compareBrakingMetrics(const BrakingMetrics &a, const BrakingMetrics &b);

} // namespace FlappedEar
