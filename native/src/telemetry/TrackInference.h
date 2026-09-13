#pragma once

#include "telemetry/LapTiming.h"
#include <QJsonObject>
#include <QSet>

namespace FlappedEar {

inline constexpr auto trackInferenceVersion = "gps-route-v1";

// Ephemeral, spatially resampled complete-lap geometry. Never serialized.
struct RouteShape {
    GeoCoordinate origin;
    QVector<QPointF> points;
    double lengthMeters = 0;
    QString direction;
};

struct TrackInference {
    RouteShape route;
    QSet<int> matchingLaps;
    QString reason;
    [[nodiscard]] bool supported() const { return !route.points.isEmpty(); }
};

[[nodiscard]] bool routesMatch(const RouteShape &a, const RouteShape &b,
    const CancellationCheck &cancelled = {});
[[nodiscard]] TrackInference inferTrack(const LapSession &laps, bool longitudeIsWestPositive = false,
    const CancellationCheck &cancelled = {});
[[nodiscard]] bool manualTrackConfiguration(const QJsonObject &configuration);

struct InferredTrackGroups {
    QHash<QString, QJsonObject> configurations;
    QHash<QString, QJsonObject> provenance;
    QHash<QString, QString> reasons;
};

// Complete-link spatial groups: a near match cannot bridge incompatible routes.
// Saved IDs are reusable only for the same algorithm, full content and gates.
[[nodiscard]] InferredTrackGroups groupInferredTracks(
    const QHash<QString, TrackInference> &inferences, const QJsonArray &sources,
    const CancellationCheck &cancelled = {});

} // namespace FlappedEar
