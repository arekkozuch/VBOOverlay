#pragma once

#include "telemetry/TelemetrySession.h"
#include "telemetry/TrackProgress.h"

#include <QString>
#include <QStringList>
#include <QVector>
#include <optional>

namespace FlappedEar {

inline constexpr auto brakingOnsetAlgorithm = "braking-onset-v1";

inline constexpr auto brakingMethodMeasured = "measuredBrake";
inline constexpr auto brakingMethodInferred = "inferredDeceleration";
inline constexpr auto brakingProvenanceMeasured = "measured";
inline constexpr auto brakingProvenanceInferred = "inferred";

// Unresolved reasons (no candidates).
inline constexpr auto brakingNoChannel = "noBrakeOrDecelerationChannel";
inline constexpr auto brakingInferenceDisabled = "inferenceDisabled";
inline constexpr auto brakingUnitMismatch = "unitMismatch";
inline constexpr auto brakingNoSamples = "noSamplesInWindow";

// Candidate uncertainty reasons.
inline constexpr auto brakingFollowsGap = "followsGap";
inline constexpr auto brakingAlreadyActive = "alreadyBrakingAtWindowStart";
inline constexpr auto brakingInterruptedByGap = "interruptedByGap";
inline constexpr auto brakingTruncatedAtWindowEnd = "truncatedAtWindowEnd";
inline constexpr auto brakingUnitUndeclared = "channelUnitUndeclared";

// Hysteresis thresholds on braking magnitude, in `unit`. For deceleration the
// magnitude is the negated longitudinal acceleration (negative G is braking).
struct BrakingThreshold {
    double on = 0.0;
    double off = 0.0;
    QString unit;
};

struct BrakingOnsetOptions {
    BrakingThreshold measuredBrake{10.0, 5.0, "%"};
    BrakingThreshold inferredDeceleration{0.30, 0.15, "g"};
    double minimumDurationSeconds = 0.2; // shorter episodes are rejected as spikes
    bool allowInferred = true;
};

struct BrakingOnsetCandidate {
    double telemetryTime = 0.0;
    double toleranceSeconds = 0.0;
    double durationSeconds = 0.0;
    double peakValue = 0.0; // channel value in its own sign convention
    std::optional<double> progressMeters;
    QStringList uncertaintyReasons;
};

struct BrakingOnsetDetection {
    QString method;
    QString provenance;
    QString channel;
    QString channelUnit; // as declared by the source; empty when undeclared
    BrakingThreshold threshold;
    double minimumDurationSeconds = 0.0;
    QVector<BrakingOnsetCandidate> candidates;
    int rejectedSpikes = 0;
    int gaps = 0;
    QString unresolvedReason;
    bool valid = false;
};

// Uses the measured "brake" channel whenever the session has one, and never
// substitutes deceleration for it, even where brake data is missing. Only a
// session without a brake channel falls back to "longitudinalAcceleration",
// labelled inferred. Raw samples in [startTime, endTime] are scanned without
// interpolation across gaps; `lapTrace`, when given, maps onsets to progress.
[[nodiscard]] BrakingOnsetDetection detectBrakingOnsets(const TelemetrySession &session, double startTime,
    double endTime, const BrakingOnsetOptions &options = {}, const QVector<ProgressSegment> *lapTrace = nullptr);

} // namespace FlappedEar
