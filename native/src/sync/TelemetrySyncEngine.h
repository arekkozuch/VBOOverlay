#pragma once

#include "telemetry/TelemetrySession.h"

#include <QString>

namespace FlappedEar {

struct SyncDiagnostics {
    double correlation = -1.0;
    double peakUniqueness = 0.0;
    int validSamples = 0;
    double sampleRate = 0.0;
    double coarseOffset = 0.0;
};

struct SyncCandidate {
    double offset = 0.0;
    double timeScale = 1.0;
    double confidence = 0.0;
    QString strategy = QStringLiteral("GPS speed");
    SyncDiagnostics diagnostics;
};

class TelemetrySyncEngine {
public:
    [[nodiscard]] static SyncCandidate synchronize(
        const TelemetrySession &video,
        const TelemetrySession &telemetry);
};

} // namespace FlappedEar
