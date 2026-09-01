#include "telemetry/TelemetryGeometry.h"

#include <cmath>
#include <numbers>

namespace FlappedEar {

std::optional<double> normalizeCoordinateDegrees(const CoordinateAxis axis, double value)
{
    if (!std::isfinite(value)) {
        return std::nullopt;
    }
    const double absolute = std::abs(value);
    if (axis == CoordinateAxis::Latitude && absolute > 90.0 && absolute <= 5'400.0) {
        value /= 60.0;
    } else if (axis == CoordinateAxis::Longitude && absolute > 180.0
               && absolute <= 10'800.0) {
        value /= 60.0;
    }
    const double limit = axis == CoordinateAxis::Latitude ? 90.0 : 180.0;
    return std::abs(value) <= limit ? std::optional<double>(value) : std::nullopt;
}

bool isValidCoordinate(const GeoCoordinate &coordinate)
{
    return std::isfinite(coordinate.latitudeDegrees)
        && std::isfinite(coordinate.longitudeDegrees)
        && std::abs(coordinate.latitudeDegrees) <= 90.0
        && std::abs(coordinate.longitudeDegrees) <= 180.0;
}

MetricPoint projectCoordinate(const GeoCoordinate &coordinate, const GeoCoordinate &origin)
{
    constexpr double earthRadiusMeters = 6'371'000.0;
    constexpr double radiansPerDegree = std::numbers::pi / 180.0;
    return {
        (coordinate.longitudeDegrees - origin.longitudeDegrees) * radiansPerDegree
            * earthRadiusMeters * std::cos(origin.latitudeDegrees * radiansPerDegree),
        (coordinate.latitudeDegrees - origin.latitudeDegrees) * radiansPerDegree
            * earthRadiusMeters,
    };
}

} // namespace FlappedEar
