#pragma once

#include "export/MediaProbe.h"

#include <QSize>
#include <QString>
#include <functional>

namespace FlappedEar {

class TelemetryFrameRenderer;

struct ExportSettings {
    QString inputPath;
    QString outputPath;
    QSize outputSize;
    MediaRational frameRate;
    double startTime = 0.0;
    double endTime = 0.0;
    QString encoder;
    QString quality = QStringLiteral("high");
    bool audioEnabled = true;
    QString cancellationFilePath;
    std::function<void(const QString &state)> stateCallback;
    std::function<bool(qsizetype currentFrame, qsizetype totalFrames, double sourceTime)> progressCallback;
    std::function<void(const QString &id, const QString &displayName)> encoderCallback;
};

struct ExportResult {
    bool success = false;
    QString error;
    MediaInfo mediaInfo;
    qsizetype renderedFrames = 0;
    qint64 elapsedMilliseconds = 0;
    qint64 renderMilliseconds = 0;
    qint64 renderNanoseconds = 0;
    qint64 polishNanoseconds = 0;
    qint64 syncRenderNanoseconds = 0;
    qint64 readbackNanoseconds = 0;
    qint64 cpuCopyNanoseconds = 0;
    qint64 ffmpegWriteNanoseconds = 0;
    bool cancelled = false;
};

class ExportEngine final {
public:
    [[nodiscard]] static ExportResult exportVideo(
        const ExportSettings &settings, TelemetryFrameRenderer &renderer);
    [[nodiscard]] static qsizetype frameCount(
        double startTime, double endTime, const MediaRational &frameRate);
    [[nodiscard]] static double framePresentationTime(
        double startTime, qsizetype frameIndex, const MediaRational &frameRate);
};

} // namespace FlappedEar
