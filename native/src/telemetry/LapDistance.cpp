#include "telemetry/LapDistance.h"
#include "telemetry/TelemetryGeometry.h"

#include <algorithm>
#include <cmath>

namespace FlappedEar {

LapDistanceProfile buildLapDistanceProfile(
    const TelemetrySession &session, const double startTime, const double endTime, const CancellationCheck &cancelled)
{
    throwIfCancelled(cancelled);
    LapDistanceProfile profile;
    if (!std::isfinite(startTime) || !std::isfinite(endTime) || endTime <= startTime) return profile;

    const auto latitudeSegments = session.sampledSegments("latitude", startTime, endTime, 4000);
    std::optional<GeoCoordinate> origin;
    std::optional<MetricPoint> previous;
    double cumulative = 0.0;

    for (const auto &segment : latitudeSegments) {
        for (const auto &sample : segment) {
            throwIfCancelled(cancelled);
            const auto longitude = session.valueAt("longitude", sample.x());
            const GeoCoordinate coordinate{sample.y(), longitude.value_or(0.0)};
            if (!longitude || !isValidCoordinate(coordinate)) {
                previous.reset(); // do not bridge distance across a missing/invalid fix
                continue;
            }
            if (!origin) origin = coordinate;
            const MetricPoint projected = projectCoordinate(coordinate, *origin);
            if (previous) {
                const double step = std::hypot(
                    projected.eastMeters - previous->eastMeters, projected.northMeters - previous->northMeters);
                if (std::isfinite(step)) cumulative += step;
            }
            previous = projected;
            profile.times.append(sample.x());
            profile.distances.append(cumulative);
        }
    }
    profile.totalMeters = cumulative;
    profile.valid = profile.times.size() >= 2 && profile.totalMeters > 0.0;
    return profile;
}

std::optional<double> timeAtDistance(const LapDistanceProfile &profile, const double distanceMeters)
{
    if (!profile.valid || !std::isfinite(distanceMeters)) return std::nullopt;
    const double clamped = std::clamp(distanceMeters, 0.0, profile.totalMeters);
    const auto next = std::lower_bound(profile.distances.cbegin(), profile.distances.cend(), clamped);
    if (next == profile.distances.cbegin()) return profile.times.front();
    if (next == profile.distances.cend()) return profile.times.back();
    const qsizetype nextIndex = std::distance(profile.distances.cbegin(), next);
    const qsizetype previousIndex = nextIndex - 1;
    const double span = profile.distances[nextIndex] - profile.distances[previousIndex];
    if (span <= 0.0) return profile.times[previousIndex];
    const double ratio = (clamped - profile.distances[previousIndex]) / span;
    return profile.times[previousIndex] + (profile.times[nextIndex] - profile.times[previousIndex]) * ratio;
}

} // namespace FlappedEar
