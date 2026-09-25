#pragma once

#include "telemetry/TelemetrySession.h"
#include "telemetry/TrackProgress.h"
#include "telemetry/TrackSegmentProposals.h"

#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVector>

namespace FlappedEar {

inline constexpr auto cornerPhaseAlgorithm = "corner-phase-v1";

// Phase methods. Entry, apex and exit are track geometry, shared by every lap
// on the axis. The minimum-speed location is a per-lap driving measurement and
// is never used as, or derived from, the apex.
inline constexpr auto cornerPhaseCurvatureOnset = "curvatureOnset";
inline constexpr auto cornerPhasePeakCurvature = "peakCurvatureRegion";
inline constexpr auto cornerPhaseCurvatureRelease = "curvatureRelease";
inline constexpr auto cornerPhaseMinimumSpeed = "minimumSpeed";

// Unresolved reasons: the phase has no location.
inline constexpr auto cornerPhaseMultipleApexes = "multipleApexes";
inline constexpr auto cornerPhaseSpeedChannelMissing = "speedChannelMissing";
inline constexpr auto cornerPhaseIncompleteCoverage = "incompleteCoverage";
inline constexpr auto cornerPhaseFlatSpeed = "flatSpeed";
inline constexpr auto cornerPhaseCrossesGate = "crossesGate";
inline constexpr auto cornerPhaseInsufficientGeometry = "insufficientGeometry";
inline constexpr auto cornerPhaseInvalidInput = "invalidInput";

// Uncertainty reasons: located, but not a sharp single point.
inline constexpr auto cornerPhaseBroadPeak = "broadPeak";
inline constexpr auto cornerPhaseAtCornerBoundary = "atCornerBoundary";

struct CornerPhasePoint {
    QString method;
    double progressMeters = 0.0;
    double toleranceMeters = 0.0;
    QStringList uncertaintyReasons;
    QString unresolvedReason; // non-empty: no location is proposed
    QJsonObject evidence;     // method inputs and measured values, for review
    [[nodiscard]] bool resolved() const { return unresolvedReason.isEmpty(); }
};

struct CornerGeometryPhases {
    CornerPhasePoint entry;
    CornerPhasePoint apex;
    CornerPhasePoint exit;
    // Progress of every separate high-curvature region, in travel order. More
    // than one leaves the apex unresolved for review.
    QVector<double> apexCandidatesMeters;
    bool valid = false;
};

struct CornerPhaseOptions {
    double apexRegionRatio = 0.8;     // curvature at or above this fraction of the corner peak starts a region
    double apexSeparationRatio = 0.6; // a region ends only once curvature falls below this fraction
    double flatSpeedFraction = 0.02;  // speeds within this fraction of the corner's speed range count as the minimum
};

// Entry and exit reuse the corner proposal's boundaries (and their
// uncertainty); the apex is the midpoint of the corner's high-curvature region.
// `corner` must be a Corner proposal from proposeTrackSegments on this axis.
[[nodiscard]] CornerGeometryPhases proposeCornerGeometryPhases(const ProgressAxis &axis,
    const TrackFeatures &features, const TrackSegmentProposal &corner, const CornerPhaseOptions &options = {});

// Samples one lap's "speed" channel every `stepMeters` across the corner,
// using that lap's projected trace (projectLapTrace on the same axis). Any
// sample without projected coverage or a speed value leaves the result
// unresolved rather than searching around the hole.
[[nodiscard]] CornerPhasePoint locateMinimumSpeed(const ProgressAxis &axis, const TrackSegmentProposal &corner,
    const QVector<ProgressSegment> &lapTrace, const TelemetrySession &session, double stepMeters,
    const CornerPhaseOptions &options = {});

inline constexpr int maximumMinimumSpeedSamples = 100'000;

} // namespace FlappedEar
