#pragma once
#include "telemetry/TelemetrySession.h"
#include "telemetry/SourceOperation.h"
namespace FlappedEar {
class TelemetrySource final {
public:
    [[nodiscard]] static bool supportsPath(const QString &path);
    // Bounded, cancellable full-content identity, independent of file paths.
    [[nodiscard]] static QByteArray contentSha256(const QString &path, qint64 expectedBytes,
        const CancellationCheck &cancelled = {});
    [[nodiscard]] static TelemetrySession load(const QString &path, const CancellationCheck &cancelled = {});
};
}
