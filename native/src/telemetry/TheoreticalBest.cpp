#include "telemetry/TheoreticalBest.h"

namespace FlappedEar {

TheoreticalBestLap computeTheoreticalBest(const ApprovedSegmentation &approved, const QVector<LapSectorTimes> &population)
{
    TheoreticalBestLap result;
    if (!approved.valid || approved.revision.isEmpty() || approved.segments.isEmpty()) {
        result.unavailableReason = theoreticalBestNoApprovedSegmentation;
        return result;
    }
    result.stamp = segmentationResultStamp(approved, theoreticalBestAlgorithm);
    result.valid = true;
    double total = 0.0;
    bool complete = true;
    for (const auto &value : approved.segments) {
        const auto segment = value.toObject();
        TheoreticalBestSector sector;
        sector.segmentId = segment.value("id").toString();
        sector.name = segment.value("name").toString();
        sector.type = segment.value("type").toString();
        for (const auto &lapTimes : population) {
            if (!lapTimes.valid || lapTimes.stamp.revision != approved.revision
                || lapTimes.stamp.trackConfigurationReference != approved.trackConfigurationReference)
                continue;
            for (const auto &candidate : lapTimes.sectors) {
                if (candidate.segmentId != sector.segmentId || !candidate.seconds) continue;
                if (!sector.seconds || *candidate.seconds < *sector.seconds) {
                    sector.seconds = candidate.seconds;
                    sector.sourceLapReference = lapTimes.lapReference;
                }
            }
        }
        if (!sector.seconds) { sector.unavailableReason = theoreticalBestIncompleteCoverage; complete = false; }
        else total += *sector.seconds;
        result.sectors.append(sector);
    }
    if (complete) result.totalSeconds = total;
    else result.unavailableReason = theoreticalBestIncompleteCoverage;
    return result;
}

} // namespace FlappedEar
