#pragma once

#include "telemetry/SectorTiming.h"

#include <QString>
#include <QVector>
#include <optional>

namespace FlappedEar {

// Time-loss observations between two laps (KAN-59). Each approved segment is
// one loss window. Approved segments never overlap, so any stretch of track
// belongs to at most one window. A straight that starts where a corner ends
// is marked as that corner's continuation, so a loss carried onto the
// straight is not counted in the corner.
//
// Over a window the A-minus-B delta changes by exactly A's time through the
// window minus B's. That is the window's increment: positive means A lost
// time in this window, negative means A gained. The running (cumulative)
// delta at the window's entry and exit is reported separately. "A is 1.2 s
// behind here" is not "A lost 1.2 s here".
inline constexpr auto timeLossAlgorithm = "time-loss-windows-v1";

inline constexpr auto timeLossRoleCorner = "corner";
inline constexpr auto timeLossRoleContinuation = "continuation"; // straight right after a corner
inline constexpr auto timeLossRoleStraight = "straight";
inline constexpr auto timeLossRoleSector = "sector";

// Why a window has no increment: either lap has no complete time through it
// (coverage gap, or a segment crossing the gate).
inline constexpr auto timeLossUntimed = "untimed";

// Two window ends closer than this are treated as the same boundary.
inline constexpr double timeLossAdjacencyMeters = 0.5;

struct TimeLossWindow {
    QString segmentId;
    QString name;
    QString type;
    QString role;
    QString cornerSegmentId; // set for a continuation: the corner it follows
    double startProgressMeters = 0.0;
    double endProgressMeters = 0.0;
    std::optional<double> incrementSeconds;         // A minus B through this window
    std::optional<double> cumulativeAtStartSeconds; // running A-minus-B delta at entry
    std::optional<double> cumulativeAtEndSeconds;   // running A-minus-B delta at exit
    QString unavailableReason; // set when incrementSeconds is absent
};

struct TimeLossObservations {
    SegmentationResultStamp stamp;
    QVector<TimeLossWindow> windows; // in track order
    // Sum of every timed increment. With a complete partition and every
    // window timed, this equals the lap-time difference.
    double timedIncrementSumSeconds = 0.0;
    bool allWindowsTimed = false;
    QString unavailableReason;
    bool valid = false;
};

inline constexpr auto timeLossDifferentSegmentOrRevision = "differentSegmentOrRevision";

// `a` and `b` must be computed against `approved` on one shared axis
// (computeLapSectorTimes). `lapStartA`/`lapStartB` are the laps' timed starts,
// used only for the cumulative values.
[[nodiscard]] TimeLossObservations computeTimeLossObservations(const ApprovedSegmentation &approved,
    double axisLengthMeters, const LapSectorTimes &a, double lapStartA, const LapSectorTimes &b, double lapStartB);

} // namespace FlappedEar
