#include "telemetry/CornerPhases.h"

#include <QJsonArray>

#include <algorithm>
#include <cmath>
#include <iterator>

namespace FlappedEar {
namespace {

bool inProgressRange(const double progress, const double start, const double end)
{
    return start <= end ? progress >= start && progress < end : progress >= start || progress < end;
}

// Axis sample indices inside the corner, in travel order (handles a corner
// that wraps across the gate).
QVector<int> cornerIndices(const TrackFeatures &features, const TrackSegmentProposal &corner)
{
    const int n = static_cast<int>(features.samples.size());
    const double start = corner.start.progressMeters;
    const double end = corner.end.progressMeters;
    int first = 0;
    for (int i = 0; i < n; ++i) {
        if (features.samples[i].progressMeters >= start) { first = i; break; }
    }
    QVector<int> indices;
    for (int walked = 0, index = first; walked < n; ++walked, index = (index + 1) % n) {
        if (!inProgressRange(features.samples[index].progressMeters, start, end)) break;
        indices.append(index);
    }
    return indices;
}

bool validCorner(const TrackSegmentProposal &corner, const double length)
{
    const auto inAxis = [length](const double value) { return std::isfinite(value) && value >= 0.0 && value <= length; };
    return corner.type == TrackSegmentType::Corner && inAxis(corner.start.progressMeters)
        && inAxis(corner.end.progressMeters) && corner.start.progressMeters != corner.end.progressMeters;
}

bool validOptions(const CornerPhaseOptions &options)
{
    return std::isfinite(options.apexRegionRatio) && std::isfinite(options.apexSeparationRatio)
        && std::isfinite(options.flatSpeedFraction) && options.apexSeparationRatio > 0.0
        && options.apexSeparationRatio < options.apexRegionRatio && options.apexRegionRatio <= 1.0
        && options.flatSpeedFraction > 0.0 && options.flatSpeedFraction < 1.0;
}

CornerPhasePoint boundaryPhase(const char *method, const SegmentProposalBoundary &boundary)
{
    CornerPhasePoint point;
    point.method = method;
    point.progressMeters = boundary.progressMeters;
    point.toleranceMeters = boundary.toleranceMeters;
    point.uncertaintyReasons = boundary.uncertaintyReasons;
    point.evidence = {{"source", trackSegmentProposalAlgorithm}};
    return point;
}

} // namespace

CornerGeometryPhases proposeCornerGeometryPhases(const ProgressAxis &axis, const TrackFeatures &features,
    const TrackSegmentProposal &corner, const CornerPhaseOptions &options)
{
    CornerGeometryPhases result;
    if (!axis.valid || !features.valid || features.samples.size() != axis.points.size()
        || axis.points.size() < 4 || !(axis.spacingMeters > 0.0) || !validCorner(corner, axis.lengthMeters)
        || !validOptions(options))
        return result;

    result.entry = boundaryPhase(cornerPhaseCurvatureOnset, corner.start);
    result.exit = boundaryPhase(cornerPhaseCurvatureRelease, corner.end);
    result.apex.method = cornerPhasePeakCurvature;
    result.valid = true;

    const auto indices = cornerIndices(features, corner);
    const double direction = corner.turnRadians != 0.0 ? corner.turnRadians : corner.peakCurvaturePerMeter;
    QVector<double> turning;
    turning.reserve(indices.size());
    double peak = 0.0;
    for (const int index : indices) {
        const double value = (direction >= 0.0 ? 1.0 : -1.0) * features.samples[index].curvaturePerMeter;
        turning.append(value);
        peak = std::max(peak, value);
    }
    if (indices.size() < 3 || !(peak > 0.0) || direction == 0.0) {
        result.apex.unresolvedReason = cornerPhaseInsufficientGeometry;
        return result;
    }

    // Hysteresis keeps small curvature ripple on a constant-radius arc from
    // splitting one region into several false apexes.
    struct Region { qsizetype first; qsizetype last; };
    QVector<Region> regions;
    bool open = false;
    for (qsizetype j = 0; j < turning.size(); ++j) {
        if (!open && turning[j] >= options.apexRegionRatio * peak) {
            regions.append({j, j});
            open = true;
        } else if (open && turning[j] < options.apexSeparationRatio * peak) {
            open = false;
        } else if (open && turning[j] >= options.apexRegionRatio * peak) {
            regions.last().last = j;
        }
    }

    const double spacing = axis.spacingMeters;
    const double length = axis.lengthMeters;
    const double baseTolerance = features.smoothingMeters + spacing;
    const auto regionStart = [&](const Region &region) { return features.samples[indices[region.first]].progressMeters; };
    const auto regionMidpoint = [&](const Region &region) {
        return std::fmod(regionStart(region) + (region.last - region.first) * spacing / 2.0, length);
    };
    QJsonArray candidates;
    for (const auto &region : regions) {
        result.apexCandidatesMeters.append(regionMidpoint(region));
        candidates.append(regionMidpoint(region));
    }
    result.apex.evidence = {{"peakCurvaturePerMeter", (direction >= 0.0 ? 1.0 : -1.0) * peak},
        {"regionRatio", options.apexRegionRatio}, {"separationRatio", options.apexSeparationRatio},
        {"smoothingMeters", features.smoothingMeters}, {"candidatesMeters", candidates}};
    if (regions.size() != 1) {
        result.apex.unresolvedReason = cornerPhaseMultipleApexes;
        return result;
    }

    const auto &region = regions.first();
    const double halfWidth = (region.last - region.first) * spacing / 2.0;
    result.apex.progressMeters = regionMidpoint(region);
    result.apex.toleranceMeters = std::max(baseTolerance, halfWidth + spacing);
    result.apex.evidence.insert("regionStartMeters", regionStart(region));
    result.apex.evidence.insert("regionEndMeters", features.samples[indices[region.last]].progressMeters);
    if (halfWidth > baseTolerance) result.apex.uncertaintyReasons.append(cornerPhaseBroadPeak);
    return result;
}

CornerPhasePoint locateMinimumSpeed(const ProgressAxis &axis, const TrackSegmentProposal &corner,
    const QVector<ProgressSegment> &lapTrace, const TelemetrySession &session, const double stepMeters,
    const CornerPhaseOptions &options)
{
    CornerPhasePoint point;
    point.method = cornerPhaseMinimumSpeed;
    if (!axis.valid || !validCorner(corner, axis.lengthMeters) || !validOptions(options)
        || !std::isfinite(stepMeters) || stepMeters <= 0.0) {
        point.unresolvedReason = cornerPhaseInvalidInput;
        return point;
    }
    point.evidence.insert("stepMeters", stepMeters);

    const QString channelName = session.aliases.value("speed");
    const auto channel = session.channels.constFind(channelName);
    if (channelName.isEmpty() || channel == session.channels.cend()) {
        point.unresolvedReason = cornerPhaseSpeedChannelMissing;
        return point;
    }
    point.evidence.insert("channel", channelName);
    point.evidence.insert("unit", channel->unit);
    // One lap is timed gate to gate, so the two halves of a gate-crossing
    // corner lie at opposite ends of the lap and are not one continuous pass.
    if (corner.end.progressMeters < corner.start.progressMeters) {
        point.unresolvedReason = cornerPhaseCrossesGate;
        return point;
    }

    const double start = corner.start.progressMeters;
    const double span = corner.end.progressMeters - start;
    const double sampleCountExact = std::ceil(span / stepMeters) + 1.0;
    if (!(sampleCountExact <= maximumMinimumSpeedSamples)) {
        point.unresolvedReason = cornerPhaseInvalidInput;
        return point;
    }
    const int count = static_cast<int>(sampleCountExact);
    QVector<double> progress(count), values(count), times(count);
    int missing = 0;
    for (int k = 0; k < count; ++k) {
        progress[k] = std::min(start + k * stepMeters, corner.end.progressMeters);
        const auto time = timeAtProgress(lapTrace, progress[k]);
        const auto value = time ? session.valueAt(channelName, *time) : std::nullopt;
        if (!value) { ++missing; continue; }
        times[k] = *time;
        values[k] = *value;
    }
    point.evidence.insert("samples", count);
    if (missing > 0) {
        point.evidence.insert("missingSamples", missing);
        point.unresolvedReason = cornerPhaseIncompleteCoverage;
        return point;
    }

    const auto [minimumIt, maximumIt] = std::minmax_element(values.cbegin(), values.cend());
    const double minimum = *minimumIt;
    const double maximum = *maximumIt;
    if (!(maximum - minimum > 1e-9 * std::max(1.0, std::abs(maximum)))) {
        point.unresolvedReason = cornerPhaseFlatSpeed;
        return point;
    }
    const int lowest = static_cast<int>(std::distance(values.cbegin(), minimumIt));
    const double nearMinimum = minimum + options.flatSpeedFraction * (maximum - minimum);
    int left = lowest;
    int right = lowest;
    while (left > 0 && values[left - 1] <= nearMinimum) --left;
    while (right + 1 < count && values[right + 1] <= nearMinimum) ++right;

    point.progressMeters = progress[lowest];
    point.toleranceMeters = stepMeters + std::max(progress[lowest] - progress[left], progress[right] - progress[lowest]);
    if (left == 0 || right == count - 1) point.uncertaintyReasons.append(cornerPhaseAtCornerBoundary);
    point.evidence.insert("minimumValue", minimum);
    point.evidence.insert("maximumValue", maximum);
    point.evidence.insert("telemetryTime", times[lowest]);
    point.evidence.insert("flatSpeedFraction", options.flatSpeedFraction);
    return point;
}

} // namespace FlappedEar
