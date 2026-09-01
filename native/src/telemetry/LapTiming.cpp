#include "telemetry/LapTiming.h"

#include "telemetry/TelemetryGeometry.h"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <limits>
#include <stdexcept>

namespace FlappedEar {
namespace {

struct Vector2 {
    double x = 0.0;
    double y = 0.0;
};

struct ClosestSegments {
    double distanceMeters = std::numeric_limits<double>::infinity();
    double vehicleFraction = 0.0;
    double gateFraction = 0.0;
};

struct GpsSample {
    double time = 0.0;
    Vector2 point;
};

struct PassageCluster {
    bool active = false;
    double firstTime = 0.0;
    double lastTime = 0.0;
    Vector2 firstPoint;
    Vector2 lastPoint;
    double candidateTime = 0.0;
    double candidateDistance = std::numeric_limits<double>::infinity();
    double candidateGateFraction = 0.0;
};

double dot(const Vector2 &left, const Vector2 &right)
{
    return left.x * right.x + left.y * right.y;
}

Vector2 subtract(const Vector2 &left, const Vector2 &right)
{
    return {left.x - right.x, left.y - right.y};
}

Vector2 add(const Vector2 &left, const Vector2 &right)
{
    return {left.x + right.x, left.y + right.y};
}

Vector2 multiply(const Vector2 &value, const double scale)
{
    return {value.x * scale, value.y * scale};
}

double length(const Vector2 &value)
{
    return std::hypot(value.x, value.y);
}

double cross(const Vector2 &left, const Vector2 &right)
{
    return left.x * right.y - left.y * right.x;
}

double clampedProjectionFraction(
    const Vector2 &point, const Vector2 &segmentStart, const Vector2 &segmentEnd)
{
    const Vector2 segment = subtract(segmentEnd, segmentStart);
    const double lengthSquared = dot(segment, segment);
    if (!std::isfinite(lengthSquared) || lengthSquared <= 0.0) return 0.0;
    return std::clamp(dot(subtract(point, segmentStart), segment) / lengthSquared, 0.0, 1.0);
}

ClosestSegments closestSegments(
    const Vector2 &vehicleStart,
    const Vector2 &vehicleEnd,
    const Vector2 &gateStart,
    const Vector2 &gateEnd)
{
    const Vector2 vehicle = subtract(vehicleEnd, vehicleStart);
    const Vector2 gate = subtract(gateEnd, gateStart);
    const double denominator = cross(vehicle, gate);
    if (std::isfinite(denominator) && std::abs(denominator) > 1e-12) {
        const Vector2 offset = subtract(gateStart, vehicleStart);
        const double vehicleFraction = cross(offset, gate) / denominator;
        const double gateFraction = cross(offset, vehicle) / denominator;
        if (vehicleFraction >= 0.0 && vehicleFraction <= 1.0
            && gateFraction >= 0.0 && gateFraction <= 1.0) {
            return {0.0, vehicleFraction, gateFraction};
        }
    }

    ClosestSegments best;
    const auto consider = [&best](
                              const Vector2 &first,
                              const Vector2 &second,
                              const double vehicleFraction,
                              const double gateFraction) {
        const double distance = length(subtract(first, second));
        if (std::isfinite(distance) && distance < best.distanceMeters) {
            best = {distance, vehicleFraction, gateFraction};
        }
    };

    const double gateForVehicleStart = clampedProjectionFraction(
        vehicleStart, gateStart, gateEnd);
    consider(vehicleStart, add(gateStart, multiply(gate, gateForVehicleStart)),
             0.0, gateForVehicleStart);
    const double gateForVehicleEnd = clampedProjectionFraction(vehicleEnd, gateStart, gateEnd);
    consider(vehicleEnd, add(gateStart, multiply(gate, gateForVehicleEnd)),
             1.0, gateForVehicleEnd);
    const double vehicleForGateStart = clampedProjectionFraction(
        gateStart, vehicleStart, vehicleEnd);
    consider(add(vehicleStart, multiply(vehicle, vehicleForGateStart)), gateStart,
             vehicleForGateStart, 0.0);
    const double vehicleForGateEnd = clampedProjectionFraction(gateEnd, vehicleStart, vehicleEnd);
    consider(add(vehicleStart, multiply(vehicle, vehicleForGateEnd)), gateEnd,
             vehicleForGateEnd, 1.0);
    return best;
}

void validateOptions(const LapDetectionOptions &options)
{
    const double values[] = {
        options.innerCorridorMeters,
        options.outerCorridorMeters,
        options.minimumGroundSpeedMetersPerSecond,
        options.minimumNormalSpeedMetersPerSecond,
        options.minimumNormalMotionRatio,
        options.refractorySeconds,
        options.maximumClusterSeconds,
        options.minimumGateLengthMeters,
        options.maximumGateLengthMeters,
    };
    if (std::any_of(std::begin(values), std::end(values), [](const double value) {
            return !std::isfinite(value) || value < 0.0;
        })
        || options.innerCorridorMeters > 50.0
        || options.outerCorridorMeters > 100.0
        || options.outerCorridorMeters <= options.innerCorridorMeters
        || options.minimumNormalMotionRatio > 1.0
        || options.refractorySeconds > 10.0
        || options.maximumClusterSeconds <= 0.0
        || options.minimumGateLengthMeters <= 0.0
        || options.maximumGateLengthMeters > 1'000.0
        || options.maximumGateLengthMeters < options.minimumGateLengthMeters
        || options.maximumAcceptedPasses <= 0
        || options.maximumAcceptedPasses > 100'000) {
        throw std::invalid_argument("Invalid lap-detection options.");
    }
}

std::optional<GpsSample> gpsSampleAt(
    const TelemetryChannel &latitude,
    const TelemetryChannel &longitude,
    const qsizetype index,
    const GeoCoordinate &origin)
{
    if (index < 0 || index >= latitude.timestamps.size() || index >= latitude.values.size()
        || index >= longitude.timestamps.size() || index >= longitude.values.size()) {
        return std::nullopt;
    }
    const double latitudeTime = latitude.timestamps[index];
    const double longitudeTime = longitude.timestamps[index];
    const GeoCoordinate coordinate{latitude.values[index], longitude.values[index]};
    if (!std::isfinite(latitudeTime) || latitudeTime != longitudeTime
        || !isValidCoordinate(coordinate)) {
        return std::nullopt;
    }
    const MetricPoint projected = projectCoordinate(coordinate, origin);
    if (!std::isfinite(projected.eastMeters) || !std::isfinite(projected.northMeters)) {
        return std::nullopt;
    }
    return GpsSample{latitudeTime, {projected.eastMeters, projected.northMeters}};
}

void deriveTimedLaps(LapSession &result)
{
    for (qsizetype index = 1; index < result.acceptedPasses.size(); ++index) {
        const double start = result.acceptedPasses[index - 1].telemetryTime;
        const double end = result.acceptedPasses[index].telemetryTime;
        const double duration = end - start;
        if (!std::isfinite(duration) || duration <= 0.0) {
            ++result.diagnostics.invalidLapDurations;
            continue;
        }
        result.timedLaps.append({
            static_cast<int>(result.timedLaps.size() + 1), start, end, duration, 0.0});
    }
    if (result.timedLaps.isEmpty()) return;
    qsizetype fastest = 0;
    for (qsizetype index = 1; index < result.timedLaps.size(); ++index) {
        if (result.timedLaps[index].durationSeconds
            < result.timedLaps[fastest].durationSeconds) {
            fastest = index;
        }
    }
    result.fastestLapIndex = fastest;
    const double fastestDuration = result.timedLaps[fastest].durationSeconds;
    for (TimedLap &lap : result.timedLaps) {
        lap.deltaToBestSeconds = lap.durationSeconds - fastestDuration;
    }
}

void buildLapTraces(
    const TelemetrySession &session,
    LapSession &result,
    const GeoCoordinate &origin,
    const CancellationCheck &cancelled)
{
    const auto latitude = session.channels.constFind(session.aliases.value("latitude"));
    const auto longitude = session.channels.constFind(session.aliases.value("longitude"));
    if (latitude == session.channels.cend() || longitude == session.channels.cend()
        || latitude->timestamps.size() != latitude->values.size()
        || longitude->timestamps.size() != longitude->values.size()) {
        return;
    }

    constexpr qsizetype MaximumTracePoints = 700'000;
    constexpr qsizetype MaximumPointsPerLapTrace = 4'096;
    qsizetype totalPoints = 0;
    const auto appendCoordinate = [&](LapTrace &trace, const double time) {
        const auto latitudeValue = session.valueAt("latitude", time, InterpolationMode::Linear);
        const auto longitudeValue = session.valueAt("longitude", time, InterpolationMode::Linear);
        if (!latitudeValue || !longitudeValue) return;
        const GeoCoordinate coordinate{*latitudeValue, *longitudeValue};
        if (!isValidCoordinate(coordinate)) return;
        const MetricPoint point = projectCoordinate(coordinate, origin);
        if (!std::isfinite(point.eastMeters) || !std::isfinite(point.northMeters)) return;
        if (!trace.points.isEmpty()
            && std::abs(trace.points.constLast().telemetryTime - time) <= 1e-9) {
            return;
        }
        if (totalPoints >= MaximumTracePoints) {
            throw ResourceLimitError("Lap traces contain too many GPS points.");
        }
        trace.points.append({time, point.eastMeters, point.northMeters});
        ++totalPoints;
    };

    result.lapTraces.reserve(result.timedLaps.size());
    for (const TimedLap &lap : result.timedLaps) {
        throwIfCancelled(cancelled);
        LapTrace trace{lap.number, lap.startTelemetryTime, lap.durationSeconds, {}};
        appendCoordinate(trace, lap.startTelemetryTime);
        auto latitudeTime = std::upper_bound(
            latitude->timestamps.cbegin(), latitude->timestamps.cend(), lap.startTelemetryTime);
        const auto latitudeEnd = std::lower_bound(
            latitudeTime, latitude->timestamps.cend(), lap.endTelemetryTime);
        const qsizetype rawPointCount = std::distance(latitudeTime, latitudeEnd);
        const qsizetype stride = std::max<qsizetype>(
            1, (rawPointCount + MaximumPointsPerLapTrace - 3)
                   / (MaximumPointsPerLapTrace - 2));
        qsizetype ordinal = 0;
        for (; latitudeTime != latitude->timestamps.cend() && *latitudeTime < lap.endTelemetryTime;
             ++latitudeTime, ++ordinal) {
            const qsizetype index = std::distance(latitude->timestamps.cbegin(), latitudeTime);
            if ((index & 0xff) == 0) throwIfCancelled(cancelled);
            if (ordinal % stride != 0) continue;
            if (index >= longitude->timestamps.size()
                || latitude->timestamps[index] != longitude->timestamps[index]) {
                continue;
            }
            const GeoCoordinate coordinate{latitude->values[index], longitude->values[index]};
            if (!isValidCoordinate(coordinate)) continue;
            const MetricPoint point = projectCoordinate(coordinate, origin);
            if (!std::isfinite(point.eastMeters) || !std::isfinite(point.northMeters)) continue;
            if (totalPoints >= MaximumTracePoints) {
                throw ResourceLimitError("Lap traces contain too many GPS points.");
            }
            trace.points.append({*latitudeTime, point.eastMeters, point.northMeters});
            ++totalPoints;
        }
        appendCoordinate(trace, lap.endTelemetryTime);
        if (trace.points.size() >= 2) result.lapTraces.append(std::move(trace));
    }
}

} // namespace

LapSession detectLaps(
    const TelemetrySession &session,
    const TimingGate &startGate,
    const LapDetectionOptions &options,
    const CancellationCheck &cancelled)
{
    validateOptions(options);
    throwIfCancelled(cancelled);
    LapSession result;
    result.selectedStartGate = startGate;

    const GeoCoordinate origin{
        (startGate.endpointA.latitudeDegrees + startGate.endpointB.latitudeDegrees) / 2.0,
        (startGate.endpointA.longitudeDegrees + startGate.endpointB.longitudeDegrees) / 2.0,
    };
    if (!isValidCoordinate(startGate.endpointA) || !isValidCoordinate(startGate.endpointB)
        || !isValidCoordinate(origin)) {
        result.status = LapSessionStatus::InvalidGate;
        return result;
    }
    const MetricPoint projectedGateA = projectCoordinate(startGate.endpointA, origin);
    const MetricPoint projectedGateB = projectCoordinate(startGate.endpointB, origin);
    const Vector2 gateA{projectedGateA.eastMeters, projectedGateA.northMeters};
    const Vector2 gateB{projectedGateB.eastMeters, projectedGateB.northMeters};
    const Vector2 gateVector = subtract(gateB, gateA);
    const double gateLength = length(gateVector);
    if (!std::isfinite(gateLength) || gateLength < options.minimumGateLengthMeters
        || gateLength > options.maximumGateLengthMeters) {
        result.status = LapSessionStatus::InvalidGate;
        return result;
    }
    const Vector2 gateNormal{-gateVector.y / gateLength, gateVector.x / gateLength};

    const auto latitude = session.channels.constFind(session.aliases.value("latitude"));
    const auto longitude = session.channels.constFind(session.aliases.value("longitude"));
    if (latitude == session.channels.cend() || longitude == session.channels.cend()) {
        result.status = LapSessionStatus::NoUsableGps;
        return result;
    }
    const qsizetype sampleCount = std::max(
        std::max(latitude->timestamps.size(), latitude->values.size()),
        std::max(longitude->timestamps.size(), longitude->values.size()));
    const double gapThreshold = std::max(
        telemetryGapThreshold(*latitude), telemetryGapThreshold(*longitude));
    std::optional<GpsSample> previous;
    PassageCluster cluster;
    bool armed = false;
    int acceptedDirection = 0;
    std::optional<double> lastAcceptedTime;

    const auto discardContinuity = [&] {
        if (cluster.active) ++result.diagnostics.discardedGapClusters;
        cluster = {};
        previous.reset();
        armed = false;
    };

    const auto finalizeCluster = [&] {
        if (!cluster.active) return;
        ++result.diagnostics.candidateClusters;
        const double duration = cluster.lastTime - cluster.firstTime;
        const Vector2 displacement = subtract(cluster.lastPoint, cluster.firstPoint);
        const double groundSpeed = duration > 0.0 ? length(displacement) / duration : 0.0;
        const double normalSpeed = duration > 0.0 ? dot(displacement, gateNormal) / duration : 0.0;
        const double normalRatio = groundSpeed > 0.0 ? std::abs(normalSpeed) / groundSpeed : 0.0;
        if (!std::isfinite(duration) || duration <= 0.0
            || duration > options.maximumClusterSeconds) {
            ++result.diagnostics.rejectedLongClusters;
        } else if (!std::isfinite(groundSpeed)
                   || groundSpeed < options.minimumGroundSpeedMetersPerSecond) {
            ++result.diagnostics.rejectedSlowClusters;
        } else if (!std::isfinite(normalSpeed)
                   || std::abs(normalSpeed) < options.minimumNormalSpeedMetersPerSecond
                   || !std::isfinite(normalRatio)
                   || normalRatio < options.minimumNormalMotionRatio) {
            ++result.diagnostics.rejectedParallelClusters;
        } else {
            const int direction = normalSpeed > 0.0 ? 1 : -1;
            if (acceptedDirection != 0 && direction != acceptedDirection) {
                ++result.diagnostics.rejectedOppositeDirectionClusters;
            } else {
                if (acceptedDirection == 0) acceptedDirection = direction;
                if (result.acceptedPasses.size() >= options.maximumAcceptedPasses) {
                    throw ResourceLimitError("Lap detector produced too many accepted passes.");
                }
                result.acceptedPasses.append({
                    cluster.candidateTime,
                    cluster.candidateDistance,
                    direction,
                    cluster.candidateGateFraction,
                    groundSpeed,
                    normalSpeed,
                });
                lastAcceptedTime = cluster.candidateTime;
            }
        }
        cluster = {};
        armed = false;
    };

    for (qsizetype index = 0; index < sampleCount; ++index) {
        if ((index & 0xff) == 0) throwIfCancelled(cancelled);
        const auto current = gpsSampleAt(*latitude, *longitude, index, origin);
        if (!current) {
            discardContinuity();
            continue;
        }
        if (!previous) {
            previous = current;
            continue;
        }
        const double interval = current->time - previous->time;
        if (!std::isfinite(interval) || interval <= 0.0 || gapThreshold <= 0.0
            || interval > gapThreshold) {
            discardContinuity();
            previous = current;
            continue;
        }
        ++result.diagnostics.usableGpsSegments;
        const ClosestSegments closest = closestSegments(
            previous->point, current->point, gateA, gateB);
        if (!std::isfinite(closest.distanceMeters)) {
            discardContinuity();
            previous = current;
            continue;
        }
        const bool outsideOuter = closest.distanceMeters > options.outerCorridorMeters;
        if (cluster.active && outsideOuter) {
            finalizeCluster();
        }
        if (!cluster.active && outsideOuter) {
            const bool refractoryComplete = !lastAcceptedTime
                || current->time - *lastAcceptedTime >= options.refractorySeconds;
            if (refractoryComplete) armed = true;
        }
        if (armed && !cluster.active
            && closest.distanceMeters <= options.innerCorridorMeters) {
            cluster.active = true;
            cluster.firstTime = previous->time;
            cluster.lastTime = current->time;
            cluster.firstPoint = previous->point;
            cluster.lastPoint = current->point;
            cluster.candidateTime = previous->time + closest.vehicleFraction * interval;
            cluster.candidateDistance = closest.distanceMeters;
            cluster.candidateGateFraction = closest.gateFraction;
        } else if (cluster.active && !outsideOuter) {
            cluster.lastTime = current->time;
            cluster.lastPoint = current->point;
            if (closest.distanceMeters < cluster.candidateDistance) {
                cluster.candidateTime = previous->time + closest.vehicleFraction * interval;
                cluster.candidateDistance = closest.distanceMeters;
                cluster.candidateGateFraction = closest.gateFraction;
            }
        }
        previous = current;
    }
    finalizeCluster();
    throwIfCancelled(cancelled);
    if (result.diagnostics.usableGpsSegments == 0) {
        result.status = LapSessionStatus::NoUsableGps;
        return result;
    }
    deriveTimedLaps(result);
    if (result.acceptedPasses.isEmpty()) {
        result.status = LapSessionStatus::NoAcceptedPasses;
    } else if (result.timedLaps.isEmpty()) {
        result.status = LapSessionStatus::InsufficientPasses;
    } else {
        buildLapTraces(session, result, origin, cancelled);
        result.status = LapSessionStatus::Available;
    }
    return result;
}

LapSession deriveSourceLapSession(
    const TelemetrySession &session,
    const LapDetectionOptions &options,
    const CancellationCheck &cancelled)
{
    throwIfCancelled(cancelled);
    const TimingGate *startGate = nullptr;
    for (const TimingGate &gate : session.timingGates) {
        if (gate.type != TimingGateType::Start) continue;
        if (startGate) {
            LapSession result;
            result.status = LapSessionStatus::AmbiguousSourceStartGate;
            return result;
        }
        startGate = &gate;
    }
    if (!startGate) {
        LapSession result;
        result.status = LapSessionStatus::NoSourceStartGate;
        return result;
    }
    return detectLaps(session, *startGate, options, cancelled);
}

} // namespace FlappedEar
