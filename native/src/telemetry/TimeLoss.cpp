#include "telemetry/TimeLoss.h"
#include "telemetry/TrackSegments.h"

#include <algorithm>
#include <cmath>

namespace FlappedEar {

namespace {
bool sameBoundary(const double first, const double second, const double axisLengthMeters)
{
    if (std::abs(first - second) <= timeLossAdjacencyMeters) return true;
    // A corner ending at the gate continues onto a straight starting at it.
    return axisLengthMeters > 0.0 && std::abs(std::abs(first - second) - axisLengthMeters) <= timeLossAdjacencyMeters;
}

const SectorTime *sectorById(const LapSectorTimes &times, const QString &segmentId)
{
    for (const auto &sector : times.sectors)
        if (sector.segmentId == segmentId) return &sector;
    return nullptr;
}

bool stampMatches(const LapSectorTimes &times, const ApprovedSegmentation &approved)
{
    return times.valid && times.stamp.revision == approved.revision
        && times.stamp.trackConfigurationReference == approved.trackConfigurationReference;
}
} // namespace

TimeLossObservations computeTimeLossObservations(const ApprovedSegmentation &approved, const double axisLengthMeters,
    const LapSectorTimes &a, const double lapStartA, const LapSectorTimes &b, const double lapStartB)
{
    TimeLossObservations result;
    if (!approved.valid || approved.revision.isEmpty() || approved.segments.isEmpty()) {
        result.unavailableReason = QStringLiteral("noApprovedSegmentation");
        return result;
    }
    if (!stampMatches(a, approved) || !stampMatches(b, approved)) {
        result.unavailableReason = timeLossDifferentSegmentOrRevision;
        return result;
    }
    result.stamp = segmentationResultStamp(approved, timeLossAlgorithm);
    result.valid = true;

    QVector<const SectorTime *> ordered;
    for (const auto &sector : a.sectors) ordered.append(&sector);
    std::sort(ordered.begin(), ordered.end(), [](const SectorTime *left, const SectorTime *right) {
        return left->startProgressMeters < right->startProgressMeters;
    });

    const auto cumulative = [](const std::optional<double> timeA, const double startA,
                                const std::optional<double> timeB, const double startB) -> std::optional<double> {
        if (!timeA || !timeB) return std::nullopt;
        return (*timeA - startA) - (*timeB - startB);
    };

    result.allWindowsTimed = true;
    for (const auto *sectorA : ordered) {
        TimeLossWindow window;
        window.segmentId = sectorA->segmentId;
        window.name = sectorA->name;
        window.type = sectorA->type;
        window.startProgressMeters = sectorA->startProgressMeters;
        window.endProgressMeters = sectorA->endProgressMeters;
        if (window.type == trackSegmentTypeName(TrackSegmentType::Corner)) {
            window.role = timeLossRoleCorner;
        } else if (window.type == trackSegmentTypeName(TrackSegmentType::Straight)) {
            window.role = timeLossRoleStraight;
            for (const auto *candidate : ordered) {
                if (candidate->type == trackSegmentTypeName(TrackSegmentType::Corner)
                    && sameBoundary(candidate->endProgressMeters, window.startProgressMeters, axisLengthMeters)) {
                    window.role = timeLossRoleContinuation;
                    window.cornerSegmentId = candidate->segmentId;
                    break;
                }
            }
        } else {
            window.role = timeLossRoleSector;
        }
        const auto *sectorB = sectorById(b, window.segmentId);
        if (sectorB) {
            window.cumulativeAtStartSeconds = cumulative(sectorA->startTime, lapStartA, sectorB->startTime, lapStartB);
            window.cumulativeAtEndSeconds = cumulative(sectorA->endTime, lapStartA, sectorB->endTime, lapStartB);
        }
        if (sectorB && sectorA->seconds && sectorB->seconds) {
            window.incrementSeconds = *sectorA->seconds - *sectorB->seconds;
            result.timedIncrementSumSeconds += *window.incrementSeconds;
        } else {
            window.unavailableReason = timeLossUntimed;
            result.allWindowsTimed = false;
        }
        result.windows.append(window);
    }
    return result;
}

} // namespace FlappedEar
