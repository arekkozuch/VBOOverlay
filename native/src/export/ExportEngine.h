#pragma once

#include "export/MediaProbe.h"

#include <QSize>
#include <QVariantMap>
#include <QString>
#include <functional>

namespace FlappedEar {

class TelemetryFrameRenderer;

struct ExportObservation {
    QString type = QStringLiteral("status");
    QString state;
    QString operation;
    QString message;
    QString component = QStringLiteral("export");
    QVariantMap details;
};

struct ExportPipelineProgress {
    qsizetype generatedFrames = 0;
    qsizetype submittedFrames = 0;
    qsizetype expectedFrames = 0;
    double sourceRangeStart = 0.0;
    double sourceRangeEnd = 0.0;
    double exportDuration = 0.0;
    double exportRelativeTime = 0.0;
    double sourceVideoTime = 0.0;
    qsizetype encodedFrames = 0;
    double encodedSeconds = 0.0;
    double outputDurationSeconds = 0.0;
    qint64 queuedBytes = 0;
    qint64 maximumQueuedBytes = 0;
    qint64 temporaryOverlayBytes = 0;
    double encoderFps = 0.0;
    double encoderRealtimeFactor = 0.0;
    QString stage = QStringLiteral("rendering");
};

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
    std::function<void(const ExportPipelineProgress &progress)> progressCallback;
    std::function<void(const QString &id, const QString &displayName)> encoderCallback;
    std::function<void(const ExportObservation &observation)> observationCallback;
};

struct ExportResult {
    bool success = false;
    QString error;
    QString validationWarning;
    MediaInfo mediaInfo;
    qsizetype generatedFrames = 0;
    qsizetype renderedFrames = 0;
    qint64 elapsedMilliseconds = 0;
    qint64 renderMilliseconds = 0;
    qint64 renderNanoseconds = 0;
    qint64 polishNanoseconds = 0;
    qint64 syncRenderNanoseconds = 0;
    qint64 readbackNanoseconds = 0;
    qint64 cpuCopyNanoseconds = 0;
    qint64 ffmpegWriteNanoseconds = 0;
    qint64 maximumQueuedBytes = 0;
    qint64 temporaryOverlayBytes = 0;
    qint64 outputBytes = 0;
    qsizetype encodedFrames = 0;
    double encodedSeconds = 0.0;
    QString diagnostics;
    bool cancelled = false;
};

class ExportEngine final {
public:
    [[nodiscard]] static ExportResult exportVideo(
        const ExportSettings &settings, TelemetryFrameRenderer &renderer);
    [[nodiscard]] static qsizetype frameCount(
        double sourceRangeStart, double sourceRangeEnd, const MediaRational &frameRate);
    [[nodiscard]] static double exportRelativeTime(
        qsizetype frameIndex, const MediaRational &frameRate);
    [[nodiscard]] static double sourceVideoTime(
        double sourceRangeStart, qsizetype frameIndex, const MediaRational &frameRate);
    [[nodiscard]] static double framePresentationTime(
        double startTime, qsizetype frameIndex, const MediaRational &frameRate);
};

} // namespace FlappedEar
