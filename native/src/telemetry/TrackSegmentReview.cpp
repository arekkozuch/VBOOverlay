#include "telemetry/TrackSegmentReview.h"

#include <algorithm>
#include <cmath>

namespace FlappedEar {
namespace {

// Bounds survive a JSON round trip exactly; this only absorbs arithmetic noise.
constexpr double boundaryMatchMeters = 1e-6;

struct Interval {
    double start = 0.0;
    double end = 0.0;
};

// Splits a wrapping range into its two pieces on [0, length].
QVector<Interval> unwrap(const ProgressRange &range, const double length)
{
    if (range.endMeters >= range.startMeters) return {{range.startMeters, range.endMeters}};
    return {{range.startMeters, std::max(range.startMeters, length)}, {0.0, range.endMeters}};
}

ProgressRange rangeOf(const QJsonObject &segment)
{
    return {segment.value("startProgressMeters").toDouble(), segment.value("endProgressMeters").toDouble()};
}

ProgressRange rangeOf(const TrackSegmentProposal &proposal)
{
    return {proposal.start.progressMeters, proposal.end.progressMeters};
}

bool sameBounds(const QJsonObject &segment, const TrackSegmentProposal &proposal)
{
    return segment.value("type").toString() == trackSegmentTypeName(proposal.type)
        && std::abs(segment.value("startProgressMeters").toDouble() - proposal.start.progressMeters) <= boundaryMatchMeters
        && std::abs(segment.value("endProgressMeters").toDouble() - proposal.end.progressMeters) <= boundaryMatchMeters;
}

bool fail(QString *error, const QString &reason)
{
    if (error) *error = reason;
    return false;
}

} // namespace

QString segmentReviewStateName(const SegmentReviewState state)
{
    switch (state) {
    case SegmentReviewState::Proposed: return "proposed";
    case SegmentReviewState::Approved: return "approved";
    case SegmentReviewState::Rejected: return "rejected";
    case SegmentReviewState::Superseded: return "superseded";
    }
    return {};
}

ApprovedSegmentation approvedSegmentation(const QJsonValue &storedSegments, const QString &trackConfigurationReference)
{
    ApprovedSegmentation result;
    result.trackConfigurationReference = trackConfigurationReference;
    if (!validTrackSegments(storedSegments)) return result;
    for (const auto &value : storedSegments.toArray()) {
        const auto segment = value.toObject();
        if (segment.value("trackConfigurationReference").toString() == trackConfigurationReference)
            result.segments.append(segment);
        else
            ++result.otherConfigurationSegments;
    }
    if (!result.segments.isEmpty()) result.revision = trackSegmentSetRevision(result.segments);
    result.valid = true;
    return result;
}

SegmentationResultStamp segmentationResultStamp(const ApprovedSegmentation &approved)
{
    return {approved.trackConfigurationReference, approved.revision};
}

bool segmentationResultCurrent(const SegmentationResultStamp &stamp, const ApprovedSegmentation &approved)
{
    return approved.valid && !stamp.revision.isEmpty() && stamp.revision == approved.revision
        && stamp.trackConfigurationReference == approved.trackConfigurationReference;
}

bool progressRangesOverlap(const ProgressRange &a, const ProgressRange &b, const double lengthMeters)
{
    for (const auto &x : unwrap(a, lengthMeters)) {
        for (const auto &y : unwrap(b, lengthMeters)) {
            if (std::min(x.end, y.end) - std::max(x.start, y.start) > boundaryMatchMeters) return true;
        }
    }
    return false;
}

QVector<SegmentReviewItem> reviewSegmentProposals(const QVector<TrackSegmentProposal> &proposals,
    const QSet<int> &edited, const QSet<int> &rejected, const ApprovedSegmentation &approved, const double lengthMeters)
{
    QVector<SegmentReviewItem> items;
    items.reserve(proposals.size());
    for (int index = 0; index < proposals.size(); ++index) {
        SegmentReviewItem item;
        item.proposal = proposals[index];
        item.edited = edited.contains(index);
        bool overlapsApproved = false;
        for (const auto &value : approved.segments) {
            const auto segment = value.toObject();
            if (item.approvedSegmentId.isEmpty() && sameBounds(segment, item.proposal)) {
                item.approvedSegmentId = segment.value("id").toString();
                continue;
            }
            overlapsApproved |= progressRangesOverlap(rangeOf(segment), rangeOf(item.proposal), lengthMeters);
        }
        if (!item.approvedSegmentId.isEmpty()) item.state = SegmentReviewState::Approved;
        else if (overlapsApproved) item.state = SegmentReviewState::Superseded;
        else if (rejected.contains(index)) item.state = SegmentReviewState::Rejected;
        items.append(item);
    }
    return items;
}

bool validProposalEdit(const QString &name, const double startMeters, const double endMeters,
    const double lengthMeters, QString *error)
{
    if (!std::isfinite(lengthMeters) || lengthMeters <= 0.0) return fail(error, "The track axis is unavailable.");
    const auto trimmed = name.trimmed();
    if (trimmed.isEmpty() || trimmed.size() > 160 || trimmed.contains(QChar::Null))
        return fail(error, "Enter a name of 1–160 characters.");
    if (!std::isfinite(startMeters) || !std::isfinite(endMeters) || startMeters < 0.0 || endMeters < 0.0
        || startMeters > lengthMeters || endMeters > lengthMeters)
        return fail(error, QString("Bounds must lie between 0 and %1 m.").arg(lengthMeters, 0, 'f', 1));
    if (std::abs(startMeters - endMeters) <= boundaryMatchMeters)
        return fail(error, "A segment cannot be empty.");
    return true;
}

std::optional<QJsonArray> withApprovedSegment(const QJsonValue &storedSegments, const QJsonObject &segment,
    const double lengthMeters, QString *error)
{
    if (!validTrackSegments(storedSegments) || !validTrackSegment(segment)) {
        fail(error, "The segment or the stored approved segments are invalid.");
        return std::nullopt;
    }
    const auto stored = storedSegments.toArray();
    const auto reference = segment.value("trackConfigurationReference").toString();
    for (const auto &value : stored) {
        const auto existing = value.toObject();
        if (existing.value("trackConfigurationReference").toString() != reference) {
            fail(error, "Segments approved for a different track configuration must be discarded first.");
            return std::nullopt;
        }
        if (existing.value("id") == segment.value("id")) {
            fail(error, "This segment is already approved.");
            return std::nullopt;
        }
        if (progressRangesOverlap(rangeOf(existing), rangeOf(segment), lengthMeters)) {
            fail(error, QString("Overlaps approved segment “%1”.").arg(existing.value("name").toString()));
            return std::nullopt;
        }
    }
    if (stored.size() >= maximumTrackSegments) {
        fail(error, QString("At most %1 segments can be approved.").arg(maximumTrackSegments));
        return std::nullopt;
    }
    QJsonArray result;
    bool inserted = false;
    const double start = segment.value("startProgressMeters").toDouble();
    for (const auto &value : stored) {
        if (!inserted && start < value.toObject().value("startProgressMeters").toDouble()) {
            result.append(segment);
            inserted = true;
        }
        result.append(value);
    }
    if (!inserted) result.append(segment);
    if (!validTrackSegments(result)) {
        fail(error, "Only the last segment on the lap may cross the start/finish line.");
        return std::nullopt;
    }
    return result;
}

std::optional<QJsonArray> withoutApprovedSegment(const QJsonValue &storedSegments, const QString &id)
{
    if (!validTrackSegments(storedSegments)) return std::nullopt;
    QJsonArray result;
    bool removed = false;
    for (const auto &value : storedSegments.toArray()) {
        if (value.toObject().value("id").toString() == id) removed = true;
        else result.append(value);
    }
    return removed ? std::optional<QJsonArray>(result) : std::nullopt;
}

QJsonArray withoutOtherConfigurations(const QJsonValue &storedSegments, const QString &trackConfigurationReference)
{
    QJsonArray result;
    for (const auto &value : storedSegments.toArray()) {
        if (value.toObject().value("trackConfigurationReference").toString() == trackConfigurationReference)
            result.append(value);
    }
    return result;
}

} // namespace FlappedEar
