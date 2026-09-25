#pragma once

#include "telemetry/TelemetrySession.h"
#include "telemetry/TrackProgress.h"
#include "telemetry/TrackSegmentReview.h"

#include <QString>
#include <QStringList>
#include <optional>

namespace FlappedEar {

// Throttle pickup and downstream exit effects for one approved segment on one
// lap (KAN-54).
//
// Pickup is searched inside the segment ([start, end] on shared-axis
// progress): the first sustained rise after a lift. It is measured from the
// recorded throttle channel when one exists; only without it is a positive
// longitudinal-acceleration onset reported, labelled inferred.
//
// The downstream interval starts at the segment's end boundary and runs to the
// end of the adjoining approved straight ("followingStraight") or, without
// one, `followMeters` further ("fixedDistance"). Exit speed, speed at the
// interval end and elapsed time over it are reported and compared; no cause is
// assigned to any difference.
inline constexpr auto exitMetricsAlgorithm = "exit-metrics-v1";

inline constexpr auto pickupMethodMeasured = "measuredThrottle";
inline constexpr auto pickupMethodInferred = "inferredAcceleration";
inline constexpr auto intervalFollowingStraight = "followingStraight";
inline constexpr auto intervalFixedDistance = "fixedDistance";

inline constexpr auto exitNoLift = "noLift";
inline constexpr auto exitNoPickup = "noPickupDetected";
inline constexpr auto exitNoChannel = "noThrottleOrAccelerationChannel";
inline constexpr auto exitUnitMismatch = "unitMismatch";
inline constexpr auto exitIncompleteCoverage = "incompleteCoverage";
inline constexpr auto exitCrossesGate = "crossesGate";
inline constexpr auto exitSpeedChannelMissing = "speedChannelMissing";
inline constexpr auto exitFollowsGap = "followsGap";
inline constexpr auto exitTruncated = "truncatedAtWindowEnd";
inline constexpr auto exitUnitUndeclared = "channelUnitUndeclared";
inline constexpr auto exitMixedProvenance = "mixedProvenance";

struct ExitThreshold {
    double on = 0.0;
    double off = 0.0;
    QString unit;
};

struct ExitMetricsOptions {
    ExitThreshold throttle{20.0, 10.0, "%"};
    ExitThreshold acceleration{0.10, 0.05, "g"};
    double minimumDurationSeconds = 0.2;
    double followMeters = 200.0;
};

struct ThrottlePickup {
    QString method;     // measuredThrottle / inferredAcceleration
    QString provenance; // measured / inferred
    QString channel;
    QString unit;
    ExitThreshold threshold;
    std::optional<double> progressMeters;
    std::optional<double> telemetryTime;
    QString unavailableReason;
    QStringList limitations;
};

struct ExitMetrics {
    QString segmentId;
    ThrottlePickup pickup;
    QString intervalSource; // followingStraight / fixedDistance
    double intervalStartMeters = 0.0;
    double intervalEndMeters = 0.0;
    QString speedChannel;
    QString speedUnit;
    std::optional<double> exitSpeed;         // at the segment end boundary
    std::optional<double> intervalEndSpeed;
    std::optional<double> elapsedSeconds;    // over the whole interval, only with continuous coverage
    QString downstreamUnavailableReason;
    SegmentationResultStamp stamp;
    bool valid = false;
};

// `lapEndTime` is the lap's timed gate crossing, used when the interval ends at the gate.
[[nodiscard]] ExitMetrics computeExitMetrics(double axisLengthMeters, const ApprovedSegmentation &approved,
    const QString &segmentId, const QVector<ProgressSegment> &lapTrace, const TelemetrySession &session,
    std::optional<double> lapEndTime = std::nullopt, const ExitMetricsOptions &options = {});

// A minus B for the same segment, revision and interval. The pickup delta is
// positive when A picks up later; it is withheld when methods differ.
struct ExitComparison {
    std::optional<double> pickupDeltaMeters;
    std::optional<double> exitSpeedDelta;
    std::optional<double> intervalEndSpeedDelta;
    std::optional<double> elapsedSecondsDelta;
    QString pickupUnavailableReason;
    bool valid = false;
};

[[nodiscard]] ExitComparison compareExitMetrics(const ExitMetrics &a, const ExitMetrics &b);

} // namespace FlappedEar
