#pragma once

#include <QHash>
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

    [[nodiscard]] std::optional<double> valueAt(
        const QString &channelName,
        double time,
        InterpolationMode mode = InterpolationMode::Linear) const;

    [[nodiscard]] QStringList channelNames() const;
};

[[nodiscard]] double videoToTelemetryTime(double videoTime, const SyncTransform &transform);

} // namespace FlappedEar
