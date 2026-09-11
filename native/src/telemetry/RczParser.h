#pragma once
#include "telemetry/TelemetrySession.h"
#include "telemetry/SourceOperation.h"

namespace FlappedEar {
class RczParser final {
public:
    static constexpr qint64 maximumArchiveBytes = 128LL * 1024 * 1024;
    static constexpr qint64 maximumMemberBytes = 32LL * 1024 * 1024;
    static constexpr qint64 maximumExpandedBytes = 256LL * 1024 * 1024;
    static constexpr int maximumMembers = 1024;
    static constexpr qsizetype maximumSamples = 2'000'000;
    [[nodiscard]] static TelemetrySession parseFile(
        const QString &path, const CancellationCheck &cancelled = {});
};
}
