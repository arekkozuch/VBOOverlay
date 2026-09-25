#pragma once

#include "telemetry/TrackSegmentProposals.h"
#include "telemetry/TrackSegments.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QSet>
#include <QString>
#include <QVector>
#include <optional>

namespace FlappedEar {

// Review of automatic segment proposals (KAN-48). Proposals are in-memory
// review input only; approval copies one into the run's persisted
// "trackSegments" array, which is the approved revision. Nothing downstream
// may consume a proposal directly.
inline constexpr auto trackSegmentReviewAlgorithm = "track-segment-review-v1";

// Heading/curvature smoothing radius used to build review proposals; each
// proposal boundary's tolerance is this plus the axis spacing.
inline constexpr double segmentReviewSmoothingMeters = 6.0;
// Coverage holes in the reviewed lap's projected trace shorter than this are
// sampling spacing at the lap ends, not GPS gaps.
inline constexpr double segmentReviewMinimumGapMeters = 15.0;

enum class SegmentReviewState {
    Proposed,   // awaiting a decision
    Approved,   // an approved segment has exactly these bounds and type
    Rejected,   // dismissed for this review session
    Superseded, // overlaps an approved segment that came from elsewhere
};

[[nodiscard]] QString segmentReviewStateName(SegmentReviewState state);

struct SegmentReviewItem {
    TrackSegmentProposal proposal; // the reviewer's edited copy when `edited`
    SegmentReviewState state = SegmentReviewState::Proposed;
    QString approvedSegmentId;     // set when Approved
    bool edited = false;
};

// The approved segments that apply to one track configuration, and the
// revision that identifies them. A result derived from segments (sector time,
// theoretical lap, report) records `revision` and is current only while it
// still matches; an empty revision means nothing is approved.
struct ApprovedSegmentation {
    QString trackConfigurationReference;
    QJsonArray segments;
    QString revision;
    // Stored segments approved for a different configuration (e.g. before a
    // layout change). They are never applied to this configuration.
    int otherConfigurationSegments = 0;
    bool valid = false; // false when the stored value itself is malformed
};

[[nodiscard]] ApprovedSegmentation approvedSegmentation(
    const QJsonValue &storedSegments, const QString &trackConfigurationReference);

struct SegmentationResultStamp {
    QString trackConfigurationReference;
    QString revision;
};

[[nodiscard]] SegmentationResultStamp segmentationResultStamp(const ApprovedSegmentation &approved);
[[nodiscard]] bool segmentationResultCurrent(
    const SegmentationResultStamp &stamp, const ApprovedSegmentation &approved);

// True when two progress intervals (end < start wraps the gate on a loop of
// `lengthMeters`) share more than a boundary point.
[[nodiscard]] bool progressRangesOverlap(const ProgressRange &a, const ProgressRange &b, double lengthMeters);

// Derives each proposal's state against the approved set. `proposals` may
// carry reviewer edits (listed in `edited`); `rejected` holds proposal indexes.
[[nodiscard]] QVector<SegmentReviewItem> reviewSegmentProposals(const QVector<TrackSegmentProposal> &proposals,
    const QSet<int> &edited, const QSet<int> &rejected, const ApprovedSegmentation &approved, double lengthMeters);

// Validates a reviewer's edit of one proposal before approval. Bounds lie in
// [0, lengthMeters], differ, and end < start wraps the gate. Overlap with
// approved segments is checked at approval, not here.
[[nodiscard]] bool validProposalEdit(const QString &name, double startMeters, double endMeters,
    double lengthMeters, QString *error = nullptr);

// Returns the stored array with `segment` inserted in start order, or nullopt
// with a reason when approval would overlap an approved segment, mix track
// configurations, exceed the bound or fail validation.
[[nodiscard]] std::optional<QJsonArray> withApprovedSegment(const QJsonValue &storedSegments,
    const QJsonObject &segment, double lengthMeters, QString *error = nullptr);

// Removes one approved segment by ID (revoking approval).
[[nodiscard]] std::optional<QJsonArray> withoutApprovedSegment(const QJsonValue &storedSegments, const QString &id);

// Drops stored segments approved for any configuration other than
// `trackConfigurationReference`, on explicit user request only.
[[nodiscard]] QJsonArray withoutOtherConfigurations(
    const QJsonValue &storedSegments, const QString &trackConfigurationReference);

} // namespace FlappedEar
