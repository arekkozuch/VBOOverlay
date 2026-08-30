#pragma once

#include "export/MediaProbe.h"
#include "export/ExportMediaProfile.h"

#include <QSize>
#include <QVariantMap>
#include <QString>
#include <functional>
#include <optional>

namespace FlappedEar {

class TelemetryFrameRenderer;

struct ExportFrameRange {
    qint64 firstFrame = 0;
    qint64 lastFrame = -1;

    [[nodiscard]] bool isValid() const { return firstFrame >= 0 && lastFrame >= firstFrame; }
    [[nodiscard]] qint64 frameCount() const { return isValid() ? lastFrame - firstFrame + 1 : 0; }
};

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
    ExportFrameRange frameRange;
    double startTime = 0.0;
    double endTime = 0.0;
    QString encoder;
    qint64 videoBitrate = 0;
    qint64 audioBitrate = 192'000;
    bool audioEnabled = true;
    QString cancellationFilePath;
    QString temporaryOverlayPath;
    QString manifestPath;
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
    ExportMediaProfile mediaProfile;
    MediaRational exportFrameRate;
    qint64 firstFrame = 0;
    qint64 lastFrame = -1;
    qint64 sourceFrameCount = 0;
    qint64 finalFrameCount = 0;
    qint64 frameDeficit = 0;
    qsizetype expectedFrames = 0;
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

// Stage B uses FFmpeg input seeking to avoid decoding an entire source prefix.
// The requested range is expressed on the source's original FFmpeg timeline;
// trim arguments are expressed on FFmpeg's zero-based, post-seek timeline.
struct StageBSourceAccess {
    double inputSeekSeconds = 0.0;
    double localTrimStartSeconds = 0.0;
    double localTrimEndSeconds = 0.0;
};

class ExportEngine final {
public:
    [[nodiscard]] static MediaRational effectiveFrameRate(
        const MediaInfo &source, const MediaRational &requested = {});
    [[nodiscard]] static ExportResult exportVideo(
        const ExportSettings &settings, TelemetryFrameRenderer &renderer);
    [[nodiscard]] static qsizetype frameCount(
        double sourceRangeStart, double sourceRangeEnd, const MediaRational &frameRate);
    [[nodiscard]] static std::optional<ExportFrameRange> frameRangeFromInclusiveFrames(
        qint64 firstFrame, qint64 lastFrame);
    [[nodiscard]] static std::optional<ExportFrameRange> fullVideoFrameRange(
        const MediaInfo &source, const MediaRational &exportFrameRate);
    [[nodiscard]] static QString formatSmpteTimecode(
        qint64 frame, const MediaRational &frameRate);
    [[nodiscard]] static std::optional<qint64> parseSmpteTimecode(
        const QString &timecode, const MediaRational &frameRate);
    [[nodiscard]] static std::optional<ExportFrameRange> frameRangeForSourceTimecode(
        const MediaInfo &source, const MediaRational &exportFrameRate,
        const QString &inTimecode, const QString &outTimecode);
    [[nodiscard]] static double audioDurationForRange(
        const MediaInfo &source, double sourceRangeStart, double sourceRangeEnd);
    [[nodiscard]] static double exportRelativeTime(
        qsizetype frameIndex, const MediaRational &frameRate);
    [[nodiscard]] static double outputDuration(
        qsizetype frameCount, const MediaRational &frameRate);
    [[nodiscard]] static double sourceVideoTime(
        double sourceRangeStart, qsizetype frameIndex, const MediaRational &frameRate);
    [[nodiscard]] static double framePresentationTime(
        double startTime, qsizetype frameIndex, const MediaRational &frameRate);
    [[nodiscard]] static StageBSourceAccess stageBSourceAccess(
        double sourceRangeStart, double sourceRangeEnd, double prerollSeconds = 5.0);
    [[nodiscard]] static QString stageBVideoFilterGraph(
        const StageBSourceAccess &sourceAccess,
        const QSize &sourceSize,
        const QSize &outputSize,
        const MediaRational &frameRate,
        qsizetype expectedFrames,
        const ExportMediaProfile &mediaProfile);
};

} // namespace FlappedEar
