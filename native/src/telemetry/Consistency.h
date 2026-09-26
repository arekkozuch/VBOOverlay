#pragma once

#include "telemetry/TimeLoss.h"

#include <QJsonObject>
#include <QString>
#include <QVector>
#include <optional>

namespace FlappedEar {

// Timing consistency (KAN-62). "Typical" is the median; the spread is the
// interquartile range (IQR): the time between the 25th and 75th percentile,
// i.e. the width of the middle half of the laps, in seconds. The IQR ignores
// the quickest and slowest quarter, so a warm-up or traffic lap does not
// dominate it. Quantiles interpolate linearly between ordered samples, the
// same definition as the ranking's lap distributions (rankOutingLaps).
inline constexpr auto consistencyAlgorithm = "consistency-iqr-v1";
// Fewer samples than this produce no statistics (unavailable), not a
// misleading spread from one or two laps.
inline constexpr qsizetype minimumConsistencySamples = 3;
inline constexpr auto consistencyTooFewSamples = "tooFewSamples";

struct ConsistencySummary {
    qsizetype count = 0; // finite samples considered
    std::optional<double> minimum, q1, median, q3, maximum;
    std::optional<double> interquartileRange; // q3 - q1, seconds
    QString unavailableReason;                // set when count < the minimum
    bool available = false;
};

[[nodiscard]] ConsistencySummary summarizeConsistency(QVector<double> values,
    qsizetype minimumSamples = minimumConsistencySamples);

struct SectorConsistency {
    QString segmentId;
    QString name;
    QString type;
    ConsistencySummary summary;
    QVector<QJsonObject> lapReferences; // laps with a time in this sector
};

// Per approved sector across a population of laps timed on one shared axis
// (the theoretical-best population). Laps from another revision are ignored.
[[nodiscard]] QVector<SectorConsistency> computeSectorConsistency(
    const ApprovedSegmentation &approved, const QVector<TimedLapSectors> &population,
    qsizetype minimumSamples = minimumConsistencySamples);

} // namespace FlappedEar
