#pragma once

#include <QSize>
#include <QString>
#include <QStringList>

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

class MediaProbe final {
public:
    [[nodiscard]] static MediaInfo probe(
        const QString &path,
        const QString &ffprobePath = {},
        bool countVideoFrames = false,
        int timeoutMilliseconds = -1);
    [[nodiscard]] static MediaInfo probeSummary(
        const QString &path, const QString &ffprobePath = {}, int timeoutMilliseconds = 30'000);
    [[nodiscard]] static MediaInfo parseJson(const QByteArray &json, const QString &path = {});
    [[nodiscard]] static MediaRational parseRational(const QString &value);
};

} // namespace FlappedEar
