#pragma once

#include "telemetry/TrackProgress.h"
#include "telemetry/TrackSegments.h"

#include <QJsonArray>
#include <QString>
#include <QStringList>
#include <QVector>

namespace FlappedEar {

// v2 (KAN-116): corners with no proposed straight between them form one
// corner chain. Persisted review decisions are keyed by this tag.
inline constexpr auto trackSegmentProposalAlgorithm = "track-segment-proposal-v2";

// Uncertainty reasons attached to a proposal boundary. A boundary with no
// reason is geometrically well separated; it still carries a finite
// tolerance and remains a proposal until reviewed.
inline constexpr auto proposalUncertainConnectedCorners = "connectedCorners";
inline constexpr auto proposalUncertainShortStraight = "shortStraight";
inline constexpr auto proposalUncertainGpsGap = "gpsGap";

// Progress interval on the axis; endMeters < startMeters wraps across the gate.
struct ProgressRange {
    double startMeters = 0.0;
    double endMeters = 0.0;
};

struct SegmentProposalOptions {
    double cornerCurvaturePerMeter = 1.0 / 250.0; // |curvature| at or above this is turning
    double minimumCornerTurnRadians = 0.35;        // smaller turning runs are kinks, kept in the straight
    double connectedStraightMeters = 20.0;         // shorter straights are not proposed; corners join
    double certainStraightMeters = 40.0;           // shorter straights get uncertain boundaries
};

struct SegmentProposalBoundary {
    double progressMeters = 0.0;
    double toleranceMeters = 0.0; // smoothing radius plus axis spacing
    QStringList uncertaintyReasons;
    [[nodiscard]] bool certain() const { return uncertaintyReasons.isEmpty(); }
};

struct TrackSegmentProposal {
    TrackSegmentType type = TrackSegmentType::Straight;
    QString name;
    SegmentProposalBoundary start;
    SegmentProposalBoundary end; // end.progressMeters < start.progressMeters wraps across the gate
    double lengthMeters = 0.0;
    double turnRadians = 0.0;           // signed total heading change; positive turns left
    double peakCurvaturePerMeter = 0.0; // signed curvature with the largest magnitude
    int chainedCorners = 0;             // corners joined into this proposal (0 for a straight)
};

struct TrackSegmentProposals {
    // Non-decreasing start progress; only the final proposal may wrap.
    QVector<TrackSegmentProposal> proposals;
    // Set (with no proposals) when the geometry cannot be split without
    // inventing boundaries: "continuousCorner", "noCorners", "tooManySegments".
    QString unresolvedReason;
    SegmentProposalOptions options;
    bool valid = false;
};

// Classifies an axis's smoothed curvature into alternating corner and
// straight proposals. `features` must come from computeTrackFeatures(axis, ...).
// `gpsGaps` are progress ranges where the analysed lap(s) lack GPS coverage;
// boundaries within tolerance of a gap are marked uncertain. The axis itself
// is built from a gap-free reference lap, so no gap originates in its geometry.
// Returns valid=false for invalid inputs. Proposals carry no IDs or approval
// state; they are review input only.
[[nodiscard]] TrackSegmentProposals proposeTrackSegments(const ProgressAxis &axis,
    const TrackFeatures &features, const QVector<ProgressRange> &gpsGaps = {},
    const SegmentProposalOptions &options = {});

// Converts proposals into ordinary editable track-segment objects with fresh
// IDs. Uncertainty is not persisted in the segment. Returns an empty array for
// invalid/unresolved proposals or when the result would not validate.
[[nodiscard]] QJsonArray proposalsToTrackSegments(
    const TrackSegmentProposals &proposals, const QString &trackConfigurationReference);

inline constexpr qsizetype maximumProposalGpsGaps = 4096;

} // namespace FlappedEar
