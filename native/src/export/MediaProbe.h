#pragma once

#include <QSize>
#include <QString>
#include <QStringList>
#include <functional>

namespace FlappedEar {

struct MediaRational {
    qint64 numerator = 0;
    qint64 denominator = 1;
    [[nodiscard]] double value() const;
    [[nodiscard]] bool isValid() const;
};

struct MediaInfo {
    QString path;
    double duration = 0.0;
    QSize videoSize;
    MediaRational frameRate;
    MediaRational averageFrameRate;
    MediaRational timeBase;
    QString videoCodec;
    QString pixelFormat;
    QStringList audioCodecs;
    qsizetype videoFrameCount = 0;
    double startTime = 0.0;
    bool likelyVariableFrameRate = false;
};

struct MediaProbeEvent {
    enum class Phase { Started, Heartbeat, Finished };
    Phase phase = Phase::Started;
    qint64 elapsedMilliseconds = 0;
    QString executable;
    QStringList arguments;
    QString targetPath;
    QString mode;
    int exitCode = 0;
};

using MediaProbeProgressCallback = std::function<void(const MediaProbeEvent &event)>;
using MediaProbeCancellationCallback = std::function<bool()>;

class MediaProbe final {
public:
    [[nodiscard]] static MediaInfo probe(
        const QString &path,
        const QString &ffprobePath = {},
        bool countVideoFrames = false,
        int timeoutMilliseconds = -1,
        const MediaProbeProgressCallback &progressCallback = {},
        const MediaProbeCancellationCallback &cancellationCallback = {});
    [[nodiscard]] static MediaInfo probeSummary(
        const QString &path,
        const QString &ffprobePath = {},
        int timeoutMilliseconds = 30'000,
        const MediaProbeProgressCallback &progressCallback = {},
        const MediaProbeCancellationCallback &cancellationCallback = {});
    [[nodiscard]] static MediaInfo parseJson(const QByteArray &json, const QString &path = {});
    [[nodiscard]] static MediaRational parseRational(const QString &value);
};

} // namespace FlappedEar
