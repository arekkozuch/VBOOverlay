#pragma once

#include "telemetry/TelemetrySession.h"

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
    [[nodiscard]] static GoProTelemetryResult load(const QString &videoPath);
    [[nodiscard]] static GoProTelemetryResult decodeGpsPackets(
        const QVector<GpmfPacket> &packets,
        double videoDuration = 0.0);
};

} // namespace FlappedEar
