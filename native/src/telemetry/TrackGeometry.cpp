#include "telemetry/TrackGeometry.h"
#include "telemetry/TelemetryGeometry.h"

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
    const MetricPoint projected = projectCoordinate(
        {latitude, longitude}, {originLatitude, originLongitude});
    return {projected.eastMeters, -projected.northMeters};
}

} // namespace

TrackGeometry buildTrackGeometry(const TelemetrySession &session, const CancellationCheck &cancelled)
{
    throwIfCancelled(cancelled);
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
        if ((index & 0xff) == 0) throwIfCancelled(cancelled);
        const double lat = latitude->values[index];
        const double lon = longitude->values[index];
        if (!isValidCoordinate({lat, lon})) {
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
    for (qsizetype index = 0; index < localPoints.size(); ++index) {
        if ((index & 0xff) == 0) throwIfCancelled(cancelled);
        const QPointF &point = localPoints[index];
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
    for (qsizetype index = 0; index < localPoints.size(); ++index) {
        if ((index & 0xff) == 0) throwIfCancelled(cancelled);
        const QPointF &point = localPoints[index];
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
    if (!latitude || !longitude || !isValidCoordinate({*latitude, *longitude})) {
        return std::nullopt;
    }
    const QPointF local = toLocal(
        *latitude, *longitude, geometry.originLatitude, geometry.originLongitude);
    if (!std::isfinite(local.x()) || !std::isfinite(local.y())
        || !std::isfinite(geometry.normalizationScale) || geometry.normalizationScale <= 0.0) {
        return std::nullopt;
    }
    const QPointF normalized(
        (local.x() - geometry.localCenter.x()) / geometry.normalizationScale + 0.5,
        (local.y() - geometry.localCenter.y()) / geometry.normalizationScale + 0.5);
    return std::isfinite(normalized.x()) && std::isfinite(normalized.y())
        ? std::optional<QPointF>(normalized) : std::nullopt;
}

} // namespace FlappedEar
