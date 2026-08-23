#pragma once

#include "telemetry/SourceOperation.h"

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
    [[nodiscard]] bool isEquivalentTo(const MediaRational &other) const;
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
    qsizetype videoPacketCount = 0;
    double startTime = 0.0;
    double videoStartTime = 0.0;
    double videoDuration = 0.0;
    double audioStartTime = 0.0;
    double audioDuration = 0.0;
    MediaRational audioTimeBase;
    int audioSampleRate = 0;
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
using MediaProbeCancellationCallback = CancellationCheck;

class MediaProbe final {
public:
    [[nodiscard]] static MediaInfo probe(
        const QString &path,
        const QString &ffprobePath = {},
        bool countVideoFrames = false,
        int timeoutMilliseconds = -1,
        const MediaProbeProgressCallback &progressCallback = {},
        const MediaProbeCancellationCallback &cancellationCallback = {},
        bool countVideoPackets = false);
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
