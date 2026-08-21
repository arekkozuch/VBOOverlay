#pragma once

#include "telemetry/TelemetrySession.h"

#include <QPointF>
#include <QRectF>
#include <QVector>
#include <optional>

namespace FlappedEar {

struct TrackGeometry {
    QVector<QPointF> points;
    QRectF localBounds;
    QPointF localCenter;
    double normalizationScale = 1.0;
    double originLatitude = 0.0;
    double originLongitude = 0.0;
    bool valid = false;
};

[[nodiscard]] TrackGeometry buildTrackGeometry(const TelemetrySession &session);
[[nodiscard]] std::optional<QPointF> currentTrackPoint(
    const TelemetrySession &session, double time, const TrackGeometry &geometry);

} // namespace FlappedEar
