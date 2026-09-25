#pragma once

// Synthetic closed loops built from exact straights and arcs, for tests that
// run the real buildProgressAxis -> computeTrackFeatures pipeline.

#include "telemetry/LapTiming.h"
#include "telemetry/TrackProgress.h"

#include <QPointF>
#include <QVector>
#include <algorithm>
#include <cmath>
#include <numbers>

namespace SyntheticLoop {

struct Step {
    double meters = 0.0;       // straight length when degrees == 0
    double degrees = 0.0;      // arc turn, positive left
    double radiusMeters = 0.0;
};

inline Step straight(const double meters) { return {meters, 0.0, 0.0}; }
inline Step arc(const double degrees, const double radius) { return {0.0, degrees, radius}; }

// Traces `half` from (0,0) heading east at ~1m steps, then appends the same
// path rotated 180 degrees about its end point. `half` must turn a net 180
// degrees, so the loop closes exactly back at the origin, where the gate sits.
// The origin is not repeated at the end.
inline QVector<QPointF> loopPoints(const QVector<Step> &half)
{
    QVector<QPointF> points{QPointF(0.0, 0.0)};
    double heading = 0.0;
    for (const auto &step : half) {
        if (step.degrees == 0.0) {
            const int steps = std::max(1, static_cast<int>(std::lround(step.meters)));
            const double ds = step.meters / steps;
            for (int i = 0; i < steps; ++i)
                points.append(points.last() + QPointF(ds * std::cos(heading), ds * std::sin(heading)));
        } else {
            const double angle = step.degrees * std::numbers::pi / 180.0;
            const int steps = std::max(1, static_cast<int>(std::lround(std::abs(angle) * step.radiusMeters)));
            const double turn = angle / steps;
            const double ds = std::abs(angle) * step.radiusMeters / steps;
            for (int i = 0; i < steps; ++i) {
                heading += turn / 2.0; // midpoint heading: each step is an exact chord of the arc
                points.append(points.last() + QPointF(ds * std::cos(heading), ds * std::sin(heading)));
                heading += turn / 2.0;
            }
        }
    }
    const QPointF end = points.last();
    const auto halfCount = points.size();
    for (qsizetype i = 1; i + 1 < halfCount; ++i) points.append(end - points[i]);
    return points;
}

inline FlappedEar::ProgressAxis buildLoopAxis(const QVector<Step> &half)
{
    FlappedEar::LapTrace trace;
    double time = 0.0;
    for (const auto &point : loopPoints(half)) trace.points.append({time++, point.x(), point.y()});
    const FlappedEar::GeoCoordinate origin{0.0, 0.0};
    const FlappedEar::TimingGate gate{FlappedEar::TimingGateType::Start, "test", origin, origin, {}};
    return FlappedEar::buildProgressAxis(trace, origin, gate);
}

inline QVector<Step> stadium() { return {straight(100), arc(180, 40), straight(100)}; }

} // namespace SyntheticLoop
