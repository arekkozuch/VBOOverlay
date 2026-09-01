#pragma once

#include "telemetry/SourceOperation.h"
#include "telemetry/TelemetrySession.h"
#include "telemetry/TimingGate.h"

#include <QVector>
#include <optional>

namespace FlappedEar {

enum class LapSessionStatus {
    Available,
    NoSourceStartGate,
    AmbiguousSourceStartGate,
    InvalidGate,
    NoUsableGps,
    NoAcceptedPasses,
    InsufficientPasses,
};

struct LapDetectionOptions {
    double innerCorridorMeters = 5.0;
    double outerCorridorMeters = 10.0;
    double minimumGroundSpeedMetersPerSecond = 2.0;
    double minimumNormalSpeedMetersPerSecond = 2.0;
    double minimumNormalMotionRatio = 0.10;
    double refractorySeconds = 1.0;
    double maximumClusterSeconds = 5.0;
    double minimumGateLengthMeters = 1.0;
    double maximumGateLengthMeters = 200.0;
    qsizetype maximumAcceptedPasses = 100'000;
};

struct LapDetectionDiagnostics {
    qsizetype usableGpsSegments = 0;
    qsizetype candidateClusters = 0;
    qsizetype discardedGapClusters = 0;
    qsizetype rejectedSlowClusters = 0;
    qsizetype rejectedParallelClusters = 0;
    qsizetype rejectedLongClusters = 0;
    qsizetype rejectedOppositeDirectionClusters = 0;
    qsizetype invalidLapDurations = 0;
};

struct GatePass {
    double telemetryTime = 0.0;
    double closestDistanceMeters = 0.0;
    int direction = 0;
    double gateFraction = 0.0;
    double groundSpeedMetersPerSecond = 0.0;
    double normalSpeedMetersPerSecond = 0.0;
};

struct TimedLap {
    int number = 0;
    double startTelemetryTime = 0.0;
    double endTelemetryTime = 0.0;
    double durationSeconds = 0.0;
    double deltaToBestSeconds = 0.0;
};

struct LapSession {
    LapSessionStatus status = LapSessionStatus::NoSourceStartGate;
    std::optional<TimingGate> selectedStartGate;
    QVector<GatePass> acceptedPasses;
    QVector<TimedLap> timedLaps;
    std::optional<qsizetype> fastestLapIndex;
    LapDetectionDiagnostics diagnostics;
};

[[nodiscard]] LapSession detectLaps(
    const TelemetrySession &session,
    const TimingGate &startGate,
    const LapDetectionOptions &options = {},
    const CancellationCheck &cancelled = {});
[[nodiscard]] LapSession deriveSourceLapSession(
    const TelemetrySession &session,
    const LapDetectionOptions &options = {},
    const CancellationCheck &cancelled = {});

} // namespace FlappedEar
