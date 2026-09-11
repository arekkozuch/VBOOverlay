#pragma once
#include "telemetry/TelemetrySession.h"
#include "telemetry/SourceOperation.h"
namespace FlappedEar {
class TelemetrySource final {
public:
    [[nodiscard]] static bool supportsPath(const QString &path);
    [[nodiscard]] static TelemetrySession load(const QString &path, const CancellationCheck &cancelled = {});
};
}
