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

// What a derived result records so it can be checked later (KAN-50). It is
// current only while the track configuration (layout, direction, timing gate),
// the approved segment set and the result's own calculation algorithm are all
// unchanged; a result without an approved revision is never current.
struct SegmentationResultStamp {
    QString trackConfigurationReference;
    QString revision;
    QString calculationAlgorithm;
};

[[nodiscard]] SegmentationResultStamp segmentationResultStamp(
    const ApprovedSegmentation &approved, const QString &calculationAlgorithm = {});
[[nodiscard]] bool segmentationResultCurrent(const SegmentationResultStamp &stamp,
    const ApprovedSegmentation &approved, const QString &calculationAlgorithm = {});

// Persisted form of a stamp, for results saved in a project or report.
[[nodiscard]] QJsonObject segmentationResultStampToJson(const SegmentationResultStamp &stamp);
[[nodiscard]] std::optional<SegmentationResultStamp> segmentationResultStampFromJson(const QJsonValue &value);

// Review decisions persisted per run in "trackSegmentReview" (KAN-50). Only
// rejections are stored: approval is the presence of a segment in
// "trackSegments". Decisions identify a proposal by type and exact bounds and
// apply only to the same track configuration and proposal algorithm;
// otherwise they are ignored rather than guessed onto new proposals.
inline constexpr qsizetype maximumSegmentReviewDecisions = 64;
[[nodiscard]] bool validTrackSegmentReview(const QJsonValue &value);
[[nodiscard]] QJsonObject makeTrackSegmentReview(
    const QString &trackConfigurationReference, const QVector<TrackSegmentProposal> &rejected);
[[nodiscard]] QSet<int> rejectedProposalIndexes(const QJsonValue &storedReview,
    const QString &trackConfigurationReference, const QVector<TrackSegmentProposal> &proposals);

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
