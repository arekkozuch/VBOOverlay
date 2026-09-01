#pragma once

#include <optional>

namespace FlappedEar {

enum class CoordinateAxis { Latitude, Longitude };

struct GeoCoordinate {
    double latitudeDegrees = 0.0;
    double longitudeDegrees = 0.0;
};

struct MetricPoint {
    double eastMeters = 0.0;
    double northMeters = 0.0;
};

[[nodiscard]] std::optional<double> normalizeCoordinateDegrees(
    CoordinateAxis axis, double value);
[[nodiscard]] bool isValidCoordinate(const GeoCoordinate &coordinate);
[[nodiscard]] MetricPoint projectCoordinate(
    const GeoCoordinate &coordinate, const GeoCoordinate &origin);

} // namespace FlappedEar
