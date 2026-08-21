#pragma once

#include <QHash>
#include <QPointF>
#include <QString>
#include <QStringList>
#include <QVector>
#include <optional>

namespace FlappedEar {

enum class InterpolationMode { Nearest, Previous, Linear };

struct TelemetryChannel {
    QString name;
    QString unit;
    QVector<double> timestamps;
    QVector<float> values;
};

struct SyncTransform {
    double offset = 0.0;
    double timeScale = 1.0;
};

class TelemetrySession {
public:
    double duration = 0.0;
    double startTime = 0.0;
    QHash<QString, QString> metadata;
    QHash<QString, TelemetryChannel> channels;
    QHash<QString, QString> aliases;
    QStringList warnings;
    qsizetype sampleCount = 0;

    // Public telemetry semantics are intentionally strict: queries outside a
    // channel's range and internal non-finite samples are no data. Linear
    // interpolation requires two adjacent finite samples; gaps are never bridged.
    [[nodiscard]] std::optional<double> valueAt(
        const QString &channelName,
        double time,
        InterpolationMode mode = InterpolationMode::Linear) const;

    [[nodiscard]] QStringList channelNames() const;
    [[nodiscard]] QVector<QPointF> sampledRange(
        const QString &channelName,
        double startTime,
        double endTime,
        int maximumPoints) const;
};

[[nodiscard]] double videoToTelemetryTime(double videoTime, const SyncTransform &transform);

} // namespace FlappedEar
