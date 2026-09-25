#include "telemetry/TrackSegmentProposals.h"

#include <QJsonObject>

#include <algorithm>
#include <cmath>
#include <iterator>

namespace FlappedEar {
namespace {

// A maximal circular run of samples sharing one label: 0 straight, +1 left
// corner, -1 right corner.
struct Run {
    int label = 0;
    int first = 0;
    int count = 0;
};

// Empty when every sample has the same label (no boundary exists).
QVector<Run> circularRuns(const QVector<int> &labels)
{
    const int n = static_cast<int>(labels.size());
    int startIndex = -1;
    for (int i = 0; i < n; ++i) {
        if (labels[i] != labels[(i - 1 + n) % n]) { startIndex = i; break; }
    }
    QVector<Run> runs;
    if (startIndex < 0) return runs;
    int index = startIndex;
    int walked = 0;
    while (walked < n) {
        Run run{labels[index], index, 0};
        while (walked < n && labels[index] == run.label) {
            ++run.count;
            index = (index + 1) % n;
            ++walked;
        }
        runs.append(run);
    }
    return runs;
}

double circularDistance(const double a, const double b, const double length)
{
    const double direct = std::abs(a - b);
    return std::min(direct, length - direct);
}

bool nearRange(const double progress, const ProgressRange &range, const double tolerance, const double length)
{
    const bool inside = range.startMeters <= range.endMeters
        ? progress >= range.startMeters && progress <= range.endMeters
        : progress >= range.startMeters || progress <= range.endMeters;
    return inside || circularDistance(progress, range.startMeters, length) <= tolerance
        || circularDistance(progress, range.endMeters, length) <= tolerance;
}

bool validOptions(const SegmentProposalOptions &options)
{
    const auto positive = [](const double value) { return std::isfinite(value) && value > 0.0; };
    return positive(options.cornerCurvaturePerMeter) && positive(options.minimumCornerTurnRadians)
        && positive(options.connectedStraightMeters) && positive(options.certainStraightMeters)
        && options.certainStraightMeters >= options.connectedStraightMeters;
}

void addReason(QStringList &reasons, const QString &reason)
{
    if (!reasons.contains(reason)) reasons.append(reason);
}

TrackSegmentProposals unresolved(const SegmentProposalOptions &options, const QString &reason)
{
    TrackSegmentProposals result;
    result.options = options;
    result.unresolvedReason = reason;
    result.valid = true;
    return result;
}

} // namespace

TrackSegmentProposals proposeTrackSegments(const ProgressAxis &axis, const TrackFeatures &features,
    const QVector<ProgressRange> &gpsGaps, const SegmentProposalOptions &options)
{
    TrackSegmentProposals invalid;
    invalid.options = options;
    if (!axis.valid || !features.valid || axis.points.size() < 4
        || features.samples.size() != axis.points.size() || !(axis.spacingMeters > 0.0)
        || !std::isfinite(axis.lengthMeters) || axis.lengthMeters <= 0.0 || !validOptions(options)
        || gpsGaps.size() > maximumProposalGpsGaps)
        return invalid;
    const double length = axis.lengthMeters;
    for (const auto &gap : gpsGaps) {
        if (!std::isfinite(gap.startMeters) || !std::isfinite(gap.endMeters) || gap.startMeters < 0.0
            || gap.endMeters < 0.0 || gap.startMeters > length || gap.endMeters > length)
            return invalid;
    }

    const int n = static_cast<int>(features.samples.size());
    const double spacing = axis.spacingMeters;
    QVector<int> labels(n, 0);
    for (int i = 0; i < n; ++i) {
        const double curvature = features.samples[i].curvaturePerMeter;
        if (!std::isfinite(curvature)) return invalid;
        if (std::abs(curvature) >= options.cornerCurvaturePerMeter) labels[i] = curvature > 0.0 ? 1 : -1;
    }
    const auto turnOf = [&](const Run &run) {
        double turn = 0.0;
        for (int k = 0; k < run.count; ++k) turn += features.samples[(run.first + k) % n].curvaturePerMeter * spacing;
        return turn;
    };
    const auto unsplittable = [&](const QVector<int> &current) {
        return unresolved(options, current.constFirst() == 0 ? "noCorners" : "continuousCorner");
    };

    auto runs = circularRuns(labels);
    if (runs.isEmpty()) return unsplittable(labels);
    // A turning run too small to be a corner is a kink: fold it into the straight.
    for (const auto &run : runs) {
        if (run.label != 0 && std::abs(turnOf(run)) < options.minimumCornerTurnRadians) {
            for (int k = 0; k < run.count; ++k) labels[(run.first + k) % n] = 0;
        }
    }
    runs = circularRuns(labels);
    if (runs.isEmpty()) return unsplittable(labels);

    const double tolerance = features.smoothingMeters + spacing;
    struct Piece {
        Run run;
        SegmentProposalBoundary start;
    };
    QVector<Piece> pieces;
    pieces.reserve(runs.size());
    for (const auto &run : runs)
        pieces.append({run, {features.samples[run.first].progressMeters, tolerance, {}}});
    const int m = static_cast<int>(pieces.size());
    for (int k = 0; k < m; ++k) {
        const auto &run = pieces[k].run;
        const double straightLength = run.count * spacing;
        if (run.label == 0 && straightLength >= options.connectedStraightMeters
            && straightLength < options.certainStraightMeters) {
            addReason(pieces[k].start.uncertaintyReasons, proposalUncertainShortStraight);
            addReason(pieces[(k + 1) % m].start.uncertaintyReasons, proposalUncertainShortStraight);
        }
    }

    // A straight too short to stand on its own is not proposed. Corners with
    // no proposed straight between them (an S-bend, or a straight shorter than
    // connectedStraightMeters) form one corner chain (KAN-116).
    int anchor = -1;
    for (int k = 0; k < m && anchor < 0; ++k) {
        const auto &run = pieces[k].run;
        if (run.label == 0 && run.count * spacing >= options.connectedStraightMeters) anchor = k;
    }
    if (anchor < 0) return unresolved(options, "continuousCorner");
    QVector<Piece> kept;
    QVector<int> chained;
    kept.reserve(m);
    for (int step = 0; step < m; ++step) {
        const auto &piece = pieces[(anchor + step) % m];
        const bool straightPiece = piece.run.label == 0;
        const bool proposedStraight = straightPiece && piece.run.count * spacing >= options.connectedStraightMeters;
        if (proposedStraight || kept.isEmpty() || kept.last().run.label == 0) {
            kept.append(piece);
            chained.append(straightPiece ? 0 : 1);
            continue;
        }
        // Extend the current chain (label stays non-zero: it is a corner).
        kept.last().run.count += piece.run.count;
        if (!straightPiece) ++chained.last();
    }
    if (kept.size() < 2) return unresolved(options, "continuousCorner");
    if (kept.size() > maximumTrackSegments) return unresolved(options, "tooManySegments");

    for (auto &piece : kept) {
        for (const auto &gap : gpsGaps) {
            if (nearRange(piece.start.progressMeters, gap, tolerance, length)) {
                addReason(piece.start.uncertaintyReasons, proposalUncertainGpsGap);
                break;
            }
        }
    }

    // Order by start progress so only the final proposal can wrap the gate.
    const auto firstIt = std::min_element(kept.cbegin(), kept.cend(),
        [](const Piece &a, const Piece &b) { return a.start.progressMeters < b.start.progressMeters; });
    const auto rotation = std::distance(kept.cbegin(), firstIt);
    std::rotate(kept.begin(), kept.begin() + rotation, kept.end());
    std::rotate(chained.begin(), chained.begin() + rotation, chained.end());

    TrackSegmentProposals result;
    result.options = options;
    const int count = static_cast<int>(kept.size());
    int cornerNumber = 0;
    int straightNumber = 0;
    for (int k = 0; k < count; ++k) {
        const auto &piece = kept[k];
        TrackSegmentProposal proposal;
        proposal.start = piece.start;
        proposal.end = kept[(k + 1) % count].start;
        // The loop closes at progress 0 only when the first boundary sits exactly on the gate.
        if (k == count - 1 && proposal.end.progressMeters == 0.0) proposal.end.progressMeters = length;
        const double span = proposal.end.progressMeters - proposal.start.progressMeters;
        proposal.lengthMeters = span > 0.0 ? span : span + length;
        if (piece.run.label == 0) {
            proposal.type = TrackSegmentType::Straight;
            proposal.name = QString("Straight %1").arg(++straightNumber);
        } else {
            proposal.type = TrackSegmentType::Corner;
            proposal.chainedCorners = chained[k];
            const int firstCorner = cornerNumber + 1;
            cornerNumber += chained[k];
            proposal.name = chained[k] > 1 ? QString("Corners %1–%2").arg(firstCorner).arg(cornerNumber)
                                           : QString("Corner %1").arg(firstCorner);
            proposal.turnRadians = turnOf(piece.run);
            for (int j = 0; j < piece.run.count; ++j) {
                const double curvature = features.samples[(piece.run.first + j) % n].curvaturePerMeter;
                if (std::abs(curvature) > std::abs(proposal.peakCurvaturePerMeter)) proposal.peakCurvaturePerMeter = curvature;
            }
        }
        result.proposals.append(proposal);
    }
    result.valid = true;
    return result;
}

QJsonArray proposalsToTrackSegments(const TrackSegmentProposals &proposals, const QString &trackConfigurationReference)
{
    if (!proposals.valid || proposals.proposals.isEmpty()) return {};
    QJsonArray segments;
    for (const auto &proposal : proposals.proposals) {
        const auto segment = makeTrackSegment(proposal.type, proposal.name,
            proposal.start.progressMeters, proposal.end.progressMeters, trackConfigurationReference);
        if (segment.isEmpty()) return {};
        segments.append(segment);
    }
    return validTrackSegments(segments) ? segments : QJsonArray{};
}

} // namespace FlappedEar
