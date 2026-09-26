#include "telemetry/Consistency.h"

#include <algorithm>
#include <cmath>

namespace FlappedEar {

ConsistencySummary summarizeConsistency(QVector<double> values, const qsizetype minimumSamples)
{
    ConsistencySummary summary;
    values.erase(std::remove_if(values.begin(), values.end(), [](const double value) { return !std::isfinite(value); }),
        values.end());
    summary.count = values.size();
    if (summary.count < std::max<qsizetype>(1, minimumSamples)) {
        summary.unavailableReason = consistencyTooFewSamples;
        return summary;
    }
    std::sort(values.begin(), values.end());
    const auto quantile = [&values](const double fraction) {
        const double position = fraction * static_cast<double>(values.size() - 1);
        const auto lower = static_cast<qsizetype>(std::floor(position));
        const auto upper = static_cast<qsizetype>(std::ceil(position));
        return values[lower] + (values[upper] - values[lower]) * (position - static_cast<double>(lower));
    };
    summary.minimum = values.first();
    summary.q1 = quantile(0.25);
    summary.median = quantile(0.5);
    summary.q3 = quantile(0.75);
    summary.maximum = values.last();
    summary.interquartileRange = *summary.q3 - *summary.q1;
    summary.available = true;
    return summary;
}

QVector<SectorConsistency> computeSectorConsistency(const ApprovedSegmentation &approved,
    const QVector<TimedLapSectors> &population, const qsizetype minimumSamples)
{
    QVector<SectorConsistency> result;
    if (!approved.valid || approved.revision.isEmpty()) return result;
    for (const auto &value : approved.segments) {
        const auto segment = value.toObject();
        SectorConsistency sector;
        sector.segmentId = segment.value("id").toString();
        sector.name = segment.value("name").toString();
        sector.type = segment.value("type").toString();
        QVector<double> times;
        for (const auto &lap : population) {
            if (!lap.times.valid || lap.times.stamp.revision != approved.revision
                || lap.times.stamp.trackConfigurationReference != approved.trackConfigurationReference)
                continue;
            for (const auto &candidate : lap.times.sectors) {
                if (candidate.segmentId != sector.segmentId || !candidate.seconds) continue;
                times.append(*candidate.seconds);
                sector.lapReferences.append(lap.times.lapReference);
            }
        }
        sector.summary = summarizeConsistency(times, minimumSamples);
        result.append(sector);
    }
    return result;
}

} // namespace FlappedEar
