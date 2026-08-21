#include "telemetry/TrackGeometry.h"

#include <QtMath>
#include <algorithm>
#include <cmath>
#include <limits>

namespace FlappedEar {
namespace {

QPointF toLocal(
    const double latitude,
    const double longitude,
    const double originLatitude,
    const double originLongitude)
{
    constexpr double earthRadius = 6'371'000.0;
    const double latitudeScale = M_PI / 180.0;
    return {(longitude - originLongitude) * latitudeScale * earthRadius
                * std::cos(originLatitude * latitudeScale),
            -(latitude - originLatitude) * latitudeScale * earthRadius};
}

} // namespace

TrackGeometry buildTrackGeometry(const TelemetrySession &session)
{
    TrackGeometry geometry;
    const auto latitude = session.channels.constFind(session.aliases.value("latitude"));
    const auto longitude = session.channels.constFind(session.aliases.value("longitude"));
    if (latitude == session.channels.cend() || longitude == session.channels.cend()) {
        return geometry;
    }

    const qsizetype count = std::min(latitude->values.size(), longitude->values.size());
    QVector<QPointF> localPoints;
    localPoints.reserve(count);
    for (qsizetype index = 0; index < count; ++index) {
        const double lat = latitude->values[index];
        const double lon = longitude->values[index];
        if (!std::isfinite(lat) || !std::isfinite(lon) || std::abs(lat) > 90.0
            || std::abs(lon) > 180.0) {
            continue;
        }
        if (!geometry.valid) {
            geometry.originLatitude = lat;
            geometry.originLongitude = lon;
            geometry.valid = true;
        }
        localPoints.append(
            toLocal(lat, lon, geometry.originLatitude, geometry.originLongitude));
    }
    if (localPoints.isEmpty()) {
        geometry.valid = false;
        return geometry;
    }

    double minimumX = std::numeric_limits<double>::infinity();
    double minimumY = std::numeric_limits<double>::infinity();
    double maximumX = -std::numeric_limits<double>::infinity();
    double maximumY = -std::numeric_limits<double>::infinity();
    for (const QPointF &point : localPoints) {
        minimumX = std::min(minimumX, point.x());
        minimumY = std::min(minimumY, point.y());
        maximumX = std::max(maximumX, point.x());
        maximumY = std::max(maximumY, point.y());
    }
    geometry.localBounds = QRectF(QPointF(minimumX, minimumY), QPointF(maximumX, maximumY));
    geometry.localCenter = geometry.localBounds.center();
    geometry.normalizationScale = std::max(
        1.0, std::max(geometry.localBounds.width(), geometry.localBounds.height()));
    geometry.points.reserve(localPoints.size());
    for (const QPointF &point : localPoints) {
        geometry.points.append(
            {(point.x() - geometry.localCenter.x()) / geometry.normalizationScale + 0.5,
             (point.y() - geometry.localCenter.y()) / geometry.normalizationScale + 0.5});
    }
    return geometry;
}

std::optional<QPointF> currentTrackPoint(
    const TelemetrySession &session, const double time, const TrackGeometry &geometry)
{
    if (!geometry.valid) {
        return std::nullopt;
    }
    const auto latitude = session.valueAt("latitude", time);
    const auto longitude = session.valueAt("longitude", time);
    if (!latitude || !longitude) {
        return std::nullopt;
    }
    const QPointF local = toLocal(
        *latitude, *longitude, geometry.originLatitude, geometry.originLongitude);
    return QPointF(
        (local.x() - geometry.localCenter.x()) / geometry.normalizationScale + 0.5,
        (local.y() - geometry.localCenter.y()) / geometry.normalizationScale + 0.5);
}

} // namespace FlappedEar
