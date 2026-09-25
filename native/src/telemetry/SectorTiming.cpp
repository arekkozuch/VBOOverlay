#include "telemetry/SectorTiming.h"

#include <QJsonArray>

#include <algorithm>
#include <cmath>

namespace FlappedEar {
namespace {

constexpr double boundaryEpsilon = 1e-6;

struct Range {
    double start = 0.0;
    double end = 0.0;
};

// Progress ranges the lap's projection covers, merged, with the ends snapped
// to the gate when they stop within the gate tolerance of it.
QVector<Range> coverage(const QVector<ProgressSegment> &lap, const double length)
{
    QVector<Range> ranges;
    for (const auto &segment : lap)
        if (!segment.samples.isEmpty())
            ranges.append({segment.samples.first().progressMeters, segment.samples.last().progressMeters});
    std::sort(ranges.begin(), ranges.end(), [](const Range &a, const Range &b) { return a.start < b.start; });
    QVector<Range> merged;
    for (const auto &range : ranges) {
        if (!merged.isEmpty() && range.start <= merged.last().end + boundaryEpsilon)
            merged.last().end = std::max(merged.last().end, range.end);
        else
            merged.append(range);
    }
    if (!merged.isEmpty()) {
        if (merged.first().start <= gateCoverageToleranceMeters) merged.first().start = 0.0;
        if (merged.last().end >= length - gateCoverageToleranceMeters) merged.last().end = length;
    }
    return merged;
}

double coveredWithin(const QVector<Range> &ranges, const double from, const double to)
{
    double covered = 0.0;
    for (const auto &range : ranges) covered += std::max(0.0, std::min(range.end, to) - std::max(range.start, from));
    return covered;
}

bool fullyCovered(const QVector<Range> &ranges, const double from, const double to)
{
    return std::any_of(ranges.cbegin(), ranges.cend(), [from, to](const Range &range) {
        return range.start <= from + boundaryEpsilon && range.end >= to - boundaryEpsilon;
    });
}

std::optional<double> crossingTime(const QVector<ProgressSegment> &lap, const double progress, const double length,
    const double lapStartTime, const double lapEndTime)
{
    if (progress <= boundaryEpsilon) return lapStartTime;
    if (progress >= length - boundaryEpsilon) return lapEndTime;
    return timeAtProgress(lap, progress);
}

} // namespace

LapSectorTimes computeLapSectorTimes(const ApprovedSegmentation &approved, const double axisLengthMeters,
    const QVector<ProgressSegment> &lapTrace, const double lapStartTime, const double lapEndTime,
    const QJsonObject &lapReference)
{
    LapSectorTimes result;
    result.lapReference = lapReference;
    if (!approved.valid || !std::isfinite(axisLengthMeters) || axisLengthMeters <= 0.0 || !std::isfinite(lapStartTime)
        || !std::isfinite(lapEndTime) || lapEndTime <= lapStartTime)
        return result;
    const double length = axisLengthMeters;
    result.stamp = segmentationResultStamp(approved, sectorTimingAlgorithm);
    result.lapSeconds = lapEndTime - lapStartTime;
    const auto ranges = coverage(lapTrace, length);

    bool complete = !approved.segments.isEmpty();
    bool allTimed = !approved.segments.isEmpty();
    double reached = 0.0;
    double sum = 0.0;
    for (const auto &value : approved.segments) {
        const auto segment = value.toObject();
        SectorTime sector;
        sector.segmentId = segment.value("id").toString();
        sector.name = segment.value("name").toString();
        sector.type = segment.value("type").toString();
        sector.startProgressMeters = segment.value("startProgressMeters").toDouble();
        sector.endProgressMeters = segment.value("endProgressMeters").toDouble();
        const double start = sector.startProgressMeters;
        const double end = sector.endProgressMeters;
        if (end < start) {
            // Its two halves lie at opposite ends of a gate-to-gate lap.
            sector.lengthMeters = end + length - start;
            sector.coveredMeters = coveredWithin(ranges, start, length) + coveredWithin(ranges, 0.0, end);
            sector.unavailableReason = sectorCrossesGate;
            complete = false;
            allTimed = false;
        } else {
            sector.lengthMeters = end - start;
            sector.coveredMeters = coveredWithin(ranges, start, end);
            sector.startTime = crossingTime(lapTrace, start, length, lapStartTime, lapEndTime);
            sector.endTime = crossingTime(lapTrace, end, length, lapStartTime, lapEndTime);
            if (fullyCovered(ranges, start, end) && sector.startTime && sector.endTime && *sector.endTime > *sector.startTime) {
                sector.seconds = *sector.endTime - *sector.startTime;
                sum += *sector.seconds;
            } else {
                sector.unavailableReason = sectorIncompleteCoverage;
                allTimed = false;
            }
            if (std::abs(start - reached) > boundaryEpsilon) complete = false;
            reached = end;
        }
        result.sectors.append(sector);
    }
    if (complete && std::abs(reached - length) > boundaryEpsilon) complete = false;
    result.completePartition = complete;
    if (complete && allTimed) {
        result.sumSeconds = sum;
        result.partitionErrorSeconds = std::abs(sum - result.lapSeconds);
    }
    result.valid = true;
    return result;
}

double projectedCoverageMeters(const QVector<ProgressSegment> &lapTrace, const double fromMeters, const double toMeters,
    const double axisLengthMeters)
{
    if (!std::isfinite(axisLengthMeters) || axisLengthMeters <= 0.0 || !(toMeters >= fromMeters)) return 0.0;
    return coveredWithin(coverage(lapTrace, axisLengthMeters), fromMeters, toMeters);
}

} // namespace FlappedEar
