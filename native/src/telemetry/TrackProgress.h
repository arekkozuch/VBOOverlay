#pragma once

#include "telemetry/LapTiming.h"
#include "telemetry/SourceOperation.h"
#include "telemetry/TelemetryGeometry.h"
#include "telemetry/TelemetrySession.h"
#include "telemetry/TimingGate.h"

#include <QPointF>
#include <QVector>
#include <optional>

namespace FlappedEar {

// A dense (~2m spacing), gate-anchored polyline built once per compatible
// track group from one representative lap. progress=0 is rotated to sit at
// the timing-gate crossing. Points are local east/north meters relative to
// `origin` -- the same convention TrackInference/LapTiming already use for a
// lap trace, deliberately ignoring the west-positive display flip: that only
// matters for map presentation, not for this internal distance math.
struct ProgressAxis {
    QVector<QPointF> points;
    QVector<double> cumulative; // cumulative[i] = arc length from points[0] to points[i]
    double lengthMeters = 0.0;
    double spacingMeters = 0.0; // ~ lengthMeters / points.size(), points are evenly arc-spaced by construction
    GeoCoordinate origin;
    bool valid = false;
};

[[nodiscard]] ProgressAxis buildProgressAxis(
    const LapTrace &referenceTrace, const GeoCoordinate &origin, const TimingGate &gate,
    const CancellationCheck &cancelled = {});

struct TrackFeatureSample {
    double progressMeters = 0.0;
    double headingRadians = 0.0;    // smoothed direction of travel, atan2(north,east) convention
    double curvaturePerMeter = 0.0; // signed rate of heading change per meter; positive turns left
};

// Heading and curvature computed once from the axis's own geometry, never
// from a per-lap projection. buildProgressAxis's input is always a single
// referenceEligible (gap-free, continuous) lap trace, so there is no per-lap
// gap concept to preserve here; "preserved gaps" is satisfied by this being a
// pure function of `axis` that never modifies axis.points or any telemetry
// channel -- only ever reads them.
struct TrackFeatures {
    QVector<TrackFeatureSample> samples; // one entry per axis.points index, same order/count
    double smoothingMeters = 0.0;
    bool valid = false;
};

// `smoothingMeters` is the averaging window's radius: heading at each axis
// point is the circular mean (mean of unit tangent vectors, never a naive
// mean of raw angles, which breaks across the +-pi wrap) of the axis's
// direction of travel within that many meters on each side, wrapping around
// the closed loop. Curvature is the signed angular change between adjacent
// smoothed headings divided by the axis's uniform point spacing. Requires a
// positive, finite smoothingMeters and a valid axis with at least 4 points.
[[nodiscard]] TrackFeatures computeTrackFeatures(const ProgressAxis &axis, double smoothingMeters);

// Rolling state carried between successive projectSample calls for one lap's
// trace. Re-create (default-construct) after a real gap so the next sample
// is treated as a cold start rather than assuming continuity across it.
struct ProjectionContext {
    bool hasLock = false;
    double lastProgressMeters = 0.0;
    double lastTelemetryTime = 0.0;
};

struct ProjectedSample {
    double telemetryTime = 0.0;
    double progressMeters = 0.0;
    bool valid = false;
};

// Bounded local-search projection of one GPS fix onto the axis. Uses the
// previous lock plus dt*speed to size a forward-biased search window rather
// than searching the whole track, and requires the car's actual heading
// (movementDirection: the raw, not-necessarily-normalized displacement since
// the previous fix) to roughly agree with the axis's direction of travel at
// the candidate match. Nearest-point distance alone cannot tell a hairpin
// apex or a nearby parallel straight (running the opposite way) from the
// correct branch; the window plus heading plus a runner-up ambiguity gap
// together can. Returns valid=false -- never a guess -- whenever no
// candidate is unambiguous, continuous, heading-consistent and close enough.
// `context` is updated only on success. Pass a zero vector for
// movementDirection when none is available yet (e.g. the very first sample);
// the heading check is then skipped for that one sample.
[[nodiscard]] ProjectedSample projectSample(
    const ProgressAxis &axis, const QPointF &localPoint, double telemetryTime,
    double speedMetersPerSecond, const QPointF &movementDirection, ProjectionContext &context);

struct ProgressSegment {
    QVector<ProjectedSample> samples; // strictly increasing telemetryTime and progressMeters
};

// Projects every valid latitude/longitude sample of the session in
// [startTime, endTime] onto the axis, in telemetry order. Mirrors
// TelemetrySession::sampledSegments's gap-preserving shape: an actual GPS
// gap, or a stretch projectSample can't lock onto, ends the current segment
// rather than bridging it.
[[nodiscard]] QVector<ProgressSegment> projectLapTrace(
    const ProgressAxis &axis, const TelemetrySession &session, double startTime, double endTime,
    const CancellationCheck &cancelled = {});

// Interpolated telemetry time at a given shared progress value, searching
// every segment (a lap trace may have several after gaps). Returns nullopt
// when no segment's locked coverage reaches that progress value -- this is
// never bridged/guessed, matching how computeDeltaSeries treats coverage.
[[nodiscard]] std::optional<double> timeAtProgress(const QVector<ProgressSegment> &lap, double progressMeters);

// The inverse: interpolated progress at a telemetry time, or nullopt when no
// segment's locked coverage spans that time (never bridged).
[[nodiscard]] std::optional<double> progressAtTime(const QVector<ProgressSegment> &lap, double telemetryTime);

struct DeltaPoint {
    double progressMeters = 0.0;
    double deltaSeconds = 0.0; // A minus B; positive means A is behind at this point
};

// Resamples only where both laps have a projected (valid, locked) sample
// covering the same progress value -- never across a gap in either lap, and
// never by inventing a value between A's and B's differing valid ranges.
[[nodiscard]] QVector<QVector<DeltaPoint>> computeDeltaSeries(
    const QVector<ProgressSegment> &lapA, const QVector<ProgressSegment> &lapB,
    double progressStepMeters, const CancellationCheck &cancelled = {});

} // namespace FlappedEar
