#pragma once

#include "telemetry/TelemetryGeometry.h"

#include <QString>

namespace FlappedEar {

enum class TimingGateType { Start, Split, Unknown };

struct TimingGate {
    TimingGateType type = TimingGateType::Unknown;
    QString sourceName;
    GeoCoordinate endpointA;
    GeoCoordinate endpointB;
    QString sourceDescription;
};

} // namespace FlappedEar
