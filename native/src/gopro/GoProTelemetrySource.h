#pragma once

#include "telemetry/TelemetrySession.h"
#include "telemetry/SourceOperation.h"

#include <QByteArray>
#include <QString>
#include <QVector>

namespace FlappedEar {

struct GpmfPacket {
    QByteArray data;
    double pts = 0.0;
    double duration = 0.0;
};

struct GoProTelemetryResult {
    TelemetrySession session;
    qsizetype packetCount = 0;
    QString gpsStream;
};

class GoProTelemetrySource {
public:
    static constexpr qsizetype kMaximumProbeOutputBytes = 64 * 1024 * 1024;
    static constexpr qsizetype kMaximumPacketCount = 100'000;
    static constexpr qint64 kMaximumMetadataBytes = 512LL * 1024 * 1024;
    static constexpr qsizetype kMaximumRecordCount = 250'000;
    static constexpr int kMaximumContainerDepth = 32;

    [[nodiscard]] static GoProTelemetryResult load(
        const QString &videoPath,
        const CancellationCheck &cancelled = {},
        const QString &ffprobeExecutable = {});
    [[nodiscard]] static GoProTelemetryResult decodeGpsPackets(
        const QVector<GpmfPacket> &packets,
        double videoDuration = 0.0,
        const CancellationCheck &cancelled = {});
};

} // namespace FlappedEar
