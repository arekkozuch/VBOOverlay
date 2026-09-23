#pragma once

#include "telemetry/SourceOperation.h"
#include "telemetry/TelemetrySession.h"

#include <QVector>
#include <optional>

namespace FlappedEar {

// Cumulative GPS arc-length ("meters into this lap") for a single lap's own
// trace. This is independent per lap -- it does not attempt to align two
// laps' distances to a shared reference line; it only parameterizes each
// lap's own samples by how far the car had travelled since the lap start.
struct LapDistanceProfile {
    QVector<double> times;     // strictly increasing telemetry timestamps with a GPS fix
    QVector<double> distances; // cumulative meters, same size as times, non-decreasing
    double totalMeters = 0.0;
    bool valid = false;
};

// A GPS outage does not bridge: distance simply stops accumulating for the
// missing stretch, so samples after a gap understate true distance by the
// length of that stretch. Acceptable for corner-level A/B comparison; not a
// substitute for a cross-lap aligned progress axis.
[[nodiscard]] LapDistanceProfile buildLapDistanceProfile(
    const TelemetrySession &session, double startTime, double endTime,
    const CancellationCheck &cancelled = {});

// Interpolated telemetry time at a given distance into the lap. Returns
// nullopt only when the profile has no valid GPS coverage at all; a
// distance outside [0, totalMeters] clamps to the nearest end.
[[nodiscard]] std::optional<double> timeAtDistance(
    const LapDistanceProfile &profile, double distanceMeters);

} // namespace FlappedEar
