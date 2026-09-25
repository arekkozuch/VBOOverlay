#pragma once

#include "telemetry/SectorTiming.h"

#include <QJsonObject>
#include <QString>
#include <QVector>
#include <optional>

namespace FlappedEar {

// A theoretical best lap (KAN-56): for each approved sector, the fastest
// valid time any lap in a compatible population actually recorded. This is
// not a sum-of-personal-bests fiction across incompatible segmentations --
// every contributing lap's sector times were computed against the same
// approved revision (see computeLapSectorTimes), so a sector's winner is a
// real, attributable result, not a guess.
inline constexpr auto theoreticalBestAlgorithm = "theoretical-best-v1";

// No run in the population has an approved segmentation to measure sectors
// against at all.
inline constexpr auto theoreticalBestNoApprovedSegmentation = "noApprovedSegmentation";
// At least one approved sector has no valid time from any lap in the
// population; the aggregate total is withheld, but other sectors still show.
inline constexpr auto theoreticalBestIncompleteCoverage = "incompleteCoverage";

struct TheoreticalBestSector {
    QString segmentId;
    QString name;
    QString type;
    std::optional<double> seconds;   // the fastest valid time across the population
    QJsonObject sourceLapReference;  // which lap contributed it
    QString unavailableReason;       // set when `seconds` is absent
};

struct TheoreticalBestLap {
    SegmentationResultStamp stamp;
    QVector<TheoreticalBestSector> sectors; // in approved order
    std::optional<double> totalSeconds;     // only when every sector has a time
    QString unavailableReason;              // set when totalSeconds is absent, or nothing was computed
    bool valid = false; // an approved segmentation existed and the computation ran
};

// Selects the fastest valid time for each approved sector across a
// population of laps' own whole-lap sector-time results. Each entry of
// `population` must already be computed against the same shared progress
// axis as `approved`; a result from a different revision or track
// configuration reference is ignored rather than guessed into alignment
// (mirrors compareSectorTimes' own same-revision requirement). Honoring
// exclusions/compatibility is the caller's responsibility: only laps already
// filtered into the compatible population should be passed in.
[[nodiscard]] TheoreticalBestLap computeTheoreticalBest(
    const ApprovedSegmentation &approved, const QVector<LapSectorTimes> &population);

} // namespace FlappedEar
