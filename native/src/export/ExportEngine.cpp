#include "export/ExportEngine.h"

#include "export/EncoderDetector.h"
#include "export/FfmpegTools.h"
#include "export/ExportProgress.h"
#include "export/TelemetryFrameRenderer.h"

#include <QFileInfo>
#include <QDir>
#include <QProcess>
#include <QTemporaryFile>
#include <QElapsedTimer>
#include <QScopeGuard>
#include <algorithm>
#include <cmath>

namespace FlappedEar {
namespace {

QByteArray rgbaBytes(const QImage &image, const QSize &size)
{
    if (image.format() == QImage::Format_RGBA8888 && image.size() == size
        && image.bytesPerLine() == size.width() * 4) {
        return QByteArray::fromRawData(
            reinterpret_cast<const char *>(image.constBits()), size.width() * size.height() * 4);
    }
    const QImage rgba = image.convertToFormat(QImage::Format_RGBA8888);
    QByteArray packed(size.width() * size.height() * 4, Qt::Uninitialized);
    char *destination = packed.data();
    for (int row = 0; row < size.height(); ++row) {
        memcpy(destination + row * size.width() * 4, rgba.constScanLine(row), size.width() * 4);
    }
    return packed;
}

QString rateString(const MediaRational &rate)
{
    return QStringLiteral("%1/%2").arg(rate.numerator).arg(rate.denominator);
}

QString qualityBitrate(const QString &quality)
{
    if (quality == "fast") {
        return QStringLiteral("6M");
    }
    if (quality == "maximum") {
        return QStringLiteral("24M");
    }
    return QStringLiteral("12M");
}

bool isCancelled(const ExportSettings &settings)
{
    return !settings.cancellationFilePath.isEmpty() && QFileInfo::exists(settings.cancellationFilePath);
}

void observe(
    const ExportSettings &settings,
    const QString &type,
    const QString &state,
    const QString &operation,
    const QString &message,
    const QString &component = QStringLiteral("export"),
    const QVariantMap &details = {})
{
    if (settings.observationCallback) {
        settings.observationCallback({type, state, operation, message, component, details});
    }
}

MediaProbeProgressCallback probeObservations(
    const ExportSettings &settings, const QString &state, const QString &operation)
{
    return [&settings, state, operation](const MediaProbeEvent &event) {
        QVariantMap details{{"mode", event.mode}, {"target", event.targetPath}};
        QString message;
        if (event.phase == MediaProbeEvent::Phase::Started) {
            details.insert("executable", event.executable);
            details.insert("arguments", event.arguments);
            message = QStringLiteral("ffprobe started");
        } else if (event.phase == MediaProbeEvent::Phase::Heartbeat) {
            details.insert("probeElapsedMilliseconds", event.elapsedMilliseconds);
            message = QStringLiteral("ffprobe running · %1 s")
                          .arg(event.elapsedMilliseconds / 1000.0, 0, 'f', 1);
        } else {
            details.insert("probeElapsedMilliseconds", event.elapsedMilliseconds);
            details.insert("exitCode", event.exitCode);
            message = QStringLiteral("ffprobe exited · code %1").arg(event.exitCode);
        }
        observe(settings, QStringLiteral("log"), state, operation, message,
                QStringLiteral("ffprobe"), details);
    };
}

QString formatArgumentList(const QStringList &arguments)
{
    QStringList quoted;
    quoted.reserve(arguments.size());
    for (const QString &argument : arguments) {
        QString escapedArgument = argument;
        escapedArgument.replace('"', QStringLiteral("\\\""));
        quoted.append(argument.contains(QLatin1Char(' ')) || argument.contains(QLatin1Char('"'))
                          ? QStringLiteral("\"") + escapedArgument + QStringLiteral("\"")
                          : escapedArgument);
    }
    return quoted.join(QLatin1Char(' '));
}

QString formatDiagnostics(
    const QProcess &process,
    const FfmpegProgress &progress,
    const QString &stderr,
    const QString &stageName,
    const QStringList &arguments,
    const QString &temporaryOverlayPath = {},
    const qint64 temporaryOverlayBytes = 0,
    const QString &stdinCloseReason = {})
{
    QString details = QStringLiteral("Stage: %1\nExit code: %2\nExit status: %3\nLast encoded frame: %4\n"
                                     "Last encoded time: %5 s\nEncoder fps: %6\nEncoder realtime: %7x")
                          .arg(stageName)
                          .arg(process.exitCode())
                          .arg(process.exitStatus() == QProcess::NormalExit ? QStringLiteral("normal")
                                                                             : QStringLiteral("crash"))
                          .arg(progress.encodedFrames)
                          .arg(progress.outputMicroseconds >= 0
                                   ? QString::number(progress.outputMicroseconds / 1'000'000.0, 'f', 3)
                                   : QStringLiteral("unavailable"))
                          .arg(progress.encoderFps, 0, 'f', 2)
                          .arg(progress.realtimeFactor, 0, 'f', 2);
    details += QStringLiteral("\nFFmpeg arguments: %1").arg(formatArgumentList(arguments));
    if (!temporaryOverlayPath.isEmpty()) {
        details += QStringLiteral("\nTemporary overlay: %1 (%2 bytes)")
                       .arg(temporaryOverlayPath)
                       .arg(temporaryOverlayBytes);
    }
    if (!stdinCloseReason.isEmpty()) {
        details += QStringLiteral("\nStage A stdin close reason: %1").arg(stdinCloseReason);
    }
    if (!stderr.trimmed().isEmpty()) {
        details += QStringLiteral("\n\nFFmpeg stderr:\n") + stderr.trimmed();
    }
    return details;
}

} // namespace

qsizetype ExportEngine::frameCount(
    const double sourceRangeStart, const double sourceRangeEnd, const MediaRational &frameRate)
{
    if (!frameRate.isValid() || !std::isfinite(sourceRangeStart) || !std::isfinite(sourceRangeEnd)
        || sourceRangeEnd <= sourceRangeStart) {
        return 0;
    }
    return static_cast<qsizetype>(
        std::ceil((sourceRangeEnd - sourceRangeStart) * static_cast<double>(frameRate.numerator)
                  / static_cast<double>(frameRate.denominator) - 1e-9));
}

double ExportEngine::framePresentationTime(
    const double startTime, const qsizetype frameIndex, const MediaRational &frameRate)
{
    return sourceVideoTime(startTime, frameIndex, frameRate);
}

double ExportEngine::exportRelativeTime(const qsizetype frameIndex, const MediaRational &frameRate)
{
    if (!frameRate.isValid()) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    return static_cast<double>(frameIndex) * static_cast<double>(frameRate.denominator)
        / static_cast<double>(frameRate.numerator);
}

double ExportEngine::outputDuration(const qsizetype frameCount, const MediaRational &frameRate)
{
    if (!frameRate.isValid()) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    return static_cast<double>(frameCount) * static_cast<double>(frameRate.denominator)
        / static_cast<double>(frameRate.numerator);
}

MediaRational ExportEngine::effectiveFrameRate(
    const MediaInfo &source, const MediaRational &requested)
{
    if (requested.isValid()) {
        return requested;
    }
    return source.averageFrameRate.isValid() ? source.averageFrameRate : source.frameRate;
}

double ExportEngine::sourceVideoTime(
    const double sourceRangeStart, const qsizetype frameIndex, const MediaRational &frameRate)
{
    return sourceRangeStart + exportRelativeTime(frameIndex, frameRate);
}

ExportResult ExportEngine::exportVideo(
    const ExportSettings &settings, TelemetryFrameRenderer &renderer)
{
    ExportResult result;
    QElapsedTimer elapsedTimer;
    elapsedTimer.start();
    const auto captureElapsed = qScopeGuard([&result, &elapsedTimer] {
        result.elapsedMilliseconds = elapsedTimer.elapsed();
        result.renderMilliseconds = result.renderNanoseconds / 1'000'000;
    });
    try {
        observe(settings, QStringLiteral("log"), QStringLiteral("preparing"),
                QStringLiteral("probeInput"), QStringLiteral("Export requested"));
        if (settings.stateCallback) {
            settings.stateCallback(QStringLiteral("starting"));
        }
        observe(settings, QStringLiteral("status"), QStringLiteral("preparing"),
                QStringLiteral("probeInput"), QStringLiteral("Reading input metadata with ffprobe"));
        const MediaInfo source = MediaProbe::probe(
            settings.inputPath, {}, false, -1,
            probeObservations(settings, QStringLiteral("preparing"), QStringLiteral("probeInput")));
        observe(settings, QStringLiteral("log"), QStringLiteral("preparing"),
                QStringLiteral("probeInput"), QStringLiteral("Input probed"), QStringLiteral("ffprobe"),
                {{"codec", source.videoCodec}, {"width", source.videoSize.width()},
                 {"height", source.videoSize.height()}, {"duration", source.duration}});
        const QSize outputSize = settings.outputSize.isValid() ? settings.outputSize : source.videoSize;
        // One exact rational governs overlay generation, framesync conversion,
        // progress, and final-media validation.
        const MediaRational exportFrameRate = effectiveFrameRate(source, settings.frameRate);
        const double sourceRangeStart = qMax(0.0, settings.startTime);
        const double sourceRangeEnd = settings.endTime > sourceRangeStart
            ? settings.endTime : source.duration;
        if (sourceRangeEnd > source.duration + 1.0 / exportFrameRate.value()) {
            result.error = QStringLiteral(
                "Requested source range (%1 to %2 s) exceeds the probed source duration (%3 s).")
                               .arg(sourceRangeStart, 0, 'f', 3)
                               .arg(sourceRangeEnd, 0, 'f', 3)
                               .arg(source.duration, 0, 'f', 3);
            return result;
        }
        const double requestedDuration = sourceRangeEnd - sourceRangeStart;
        const qsizetype expectedFrames = frameCount(
            sourceRangeStart, sourceRangeEnd, exportFrameRate);
        if (expectedFrames == 0 || !outputSize.isValid()) {
            result.error = QStringLiteral("Export range or frame rate is invalid.");
            return result;
        }
        const double exportDuration = outputDuration(expectedFrames, exportFrameRate);
        result.exportFrameRate = exportFrameRate;
        result.expectedFrames = expectedFrames;
        const QList<EncoderCapability> encoders = EncoderDetector::discover();
        const QString encoder = settings.encoder.isEmpty()
            ? EncoderDetector::preferredHevcEncoder(encoders)
            : settings.encoder;
        if (encoder.isEmpty()
            || !std::any_of(encoders.cbegin(), encoders.cend(), [&encoder](const auto &item) {
                   return item.id == encoder;
               })) {
            result.error = QStringLiteral("No requested HEVC encoder is available in this FFmpeg build.");
            return result;
        }
        const auto selected = std::find_if(encoders.cbegin(), encoders.cend(), [&encoder](const auto &item) {
            return item.id == encoder;
        });
        if (settings.encoderCallback && selected != encoders.cend()) {
            settings.encoderCallback(selected->id, selected->displayName);
        }
        QTemporaryFile temporaryOverlay(
            QDir::temp().filePath(QStringLiteral("flappedear-overlay-XXXXXX.mkv")));
        if (!temporaryOverlay.open()) {
            result.error = QStringLiteral("Could not create a temporary telemetry overlay file.");
            return result;
        }
        temporaryOverlay.close();
        temporaryOverlay.setAutoRemove(false);
        const auto cleanupTemporaryOverlay = qScopeGuard([&] {
            const QFileInfo temporaryInfo(temporaryOverlay.fileName());
            observe(settings, QStringLiteral("status"), QStringLiteral("cleaningUp"),
                    QStringLiteral("removeTemporaryOverlay"),
                    QStringLiteral("Cleaning temporary overlay"));
            observe(settings, QStringLiteral("log"), QStringLiteral("cleaningUp"),
                    QStringLiteral("removeTemporaryOverlay"),
                    QStringLiteral("Removing temporary overlay"), QStringLiteral("cleanup"),
                    {{"path", temporaryOverlay.fileName()}, {"bytes", temporaryInfo.size()}});
            const bool removed = !temporaryInfo.exists() || QFile::remove(temporaryOverlay.fileName());
            observe(settings, QStringLiteral("log"), QStringLiteral("cleaningUp"),
                    QStringLiteral("removeTemporaryOverlay"),
                    removed ? QStringLiteral("Temporary overlay deleted")
                            : QStringLiteral("Temporary overlay deletion failed"),
                    QStringLiteral("cleanup"), {{"result", removed ? "deleted" : "failed"}});
        });
        QProcess ffmpeg;
        QProcess *activeFfmpeg = &ffmpeg;
        const QString size = QStringLiteral("%1x%2").arg(outputSize.width()).arg(outputSize.height());
        const QStringList overlayArguments = {
            "-hide_banner", "-loglevel", "error", "-nostats", "-progress", "pipe:1", "-y",
            "-f", "rawvideo", "-pixel_format", "rgba", "-video_size", size,
            "-framerate", rateString(exportFrameRate), "-i", "pipe:0", "-an",
            "-c:v", "ffv1", "-pix_fmt", "bgra", "-f", "matroska", temporaryOverlay.fileName(),
        };
        observe(settings, QStringLiteral("status"), QStringLiteral("renderingOverlay"),
                QStringLiteral("encodeTemporaryOverlay"),
                QStringLiteral("Encoding FFV1 temporary overlay"));
        observe(settings, QStringLiteral("log"), QStringLiteral("renderingOverlay"),
                QStringLiteral("encodeTemporaryOverlay"), QStringLiteral("Stage A started"),
                QStringLiteral("ffmpeg"),
                {{"executable", FfmpegTools::ffmpegPath()}, {"arguments", overlayArguments}});
        ffmpeg.setProcessChannelMode(QProcess::SeparateChannels);
        qInfo().noquote() << QStringLiteral("Stage A FFmpeg arguments: %1").arg(formatArgumentList(overlayArguments));
        ffmpeg.start(FfmpegTools::ffmpegPath(), overlayArguments);
        if (!ffmpeg.waitForStarted()) {
            result.error = QStringLiteral("Could not start FFmpeg: %1").arg(ffmpeg.errorString());
            return result;
        }
        if (isCancelled(settings)) {
            result.cancelled = true;
            ffmpeg.kill();
            ffmpeg.waitForFinished();
            return result;
        }
        const qint64 bytesPerFrame = static_cast<qint64>(outputSize.width()) * outputSize.height() * 4;
        // Two 4K RGBA frames are about 63 MiB. Accounting for one write that
        // crosses the threshold, this caps Qt's queue at three frames while
        // retaining enough overlap for the renderer and encoder to pipeline.
        const qint64 queueHighWaterMark = qMax<qint64>(bytesPerFrame * 2, 8 * 1024 * 1024);
        const qint64 queueLowWaterMark = queueHighWaterMark / 2;
        FfmpegProgressParser progressParser;
        FfmpegProgress lastFfmpegProgress;
        QString stderr;
        QElapsedTimer activityTimer;
        activityTimer.start();
        bool finalizing = false;
        bool compositing = false;
        int lastOverlayMilestone = -5;
        int lastCompositionMilestone = -5;
        QString stageAStdinCloseReason;
        const auto refreshTemporaryOverlaySize = [&] {
            result.temporaryOverlayBytes = QFileInfo(temporaryOverlay.fileName()).size();
        };
        const auto reportProgress = [&] {
            if (!compositing) {
                refreshTemporaryOverlaySize();
            }
            const qint64 queuedBytes = static_cast<qint64>(activeFfmpeg->bytesToWrite());
            result.maximumQueuedBytes = qMax(result.maximumQueuedBytes, queuedBytes);
            if (!settings.progressCallback) return;
            ExportPipelineProgress progress;
            progress.generatedFrames = result.generatedFrames;
            progress.submittedFrames = result.renderedFrames;
            progress.expectedFrames = expectedFrames;
            progress.sourceRangeStart = sourceRangeStart;
            progress.sourceRangeEnd = sourceRangeEnd;
            progress.exportDuration = exportDuration;
            progress.exportRelativeTime = result.renderedFrames > 0
                ? ExportEngine::exportRelativeTime(result.renderedFrames - 1, exportFrameRate) : 0.0;
            progress.sourceVideoTime = sourceVideoTime(
                sourceRangeStart, result.renderedFrames > 0 ? result.renderedFrames - 1 : 0,
                exportFrameRate);
            progress.encodedFrames = lastFfmpegProgress.encodedFrames;
            progress.encodedSeconds = compositing
                ? qMax(0.0, lastFfmpegProgress.outputMicroseconds / 1'000'000.0) : 0.0;
            progress.outputDurationSeconds = exportDuration;
            progress.queuedBytes = queuedBytes;
            progress.maximumQueuedBytes = result.maximumQueuedBytes;
            progress.temporaryOverlayBytes = result.temporaryOverlayBytes;
            progress.encoderFps = lastFfmpegProgress.encoderFps;
            progress.encoderRealtimeFactor = lastFfmpegProgress.realtimeFactor;
            progress.stage = compositing && finalizing ? QStringLiteral("finalizing")
                : (compositing ? QStringLiteral("encodingVideo") : QStringLiteral("renderingOverlay"));
            settings.progressCallback(progress);
            const int percent = progress.expectedFrames > 0
                ? qBound(0, static_cast<int>(100 * (compositing ? progress.encodedFrames
                                                                : progress.submittedFrames)
                                                   / progress.expectedFrames), 100)
                : 0;
            int &lastMilestone = compositing ? lastCompositionMilestone : lastOverlayMilestone;
            const int milestone = (percent / 5) * 5;
            if (milestone >= lastMilestone + 5 || percent == 100) {
                lastMilestone = milestone;
                observe(settings, QStringLiteral("log"),
                        compositing ? QStringLiteral("encodingVideo")
                                    : QStringLiteral("renderingOverlay"),
                        compositing ? QStringLiteral("encodeFinalVideo")
                                    : QStringLiteral("renderTelemetryOverlay"),
                        QStringLiteral("%1: %2% · %3/%4")
                            .arg(compositing ? QStringLiteral("Final encode")
                                             : QStringLiteral("Overlay"))
                            .arg(milestone)
                            .arg(compositing ? progress.encodedFrames : progress.submittedFrames)
                            .arg(progress.expectedFrames),
                        QStringLiteral("ffmpeg"));
            }
        };
        const auto pumpFfmpeg = [&] {
            const QByteArray stdoutData = activeFfmpeg->readAllStandardOutput();
            const QByteArray stderrData = activeFfmpeg->readAllStandardError();
            if (!stdoutData.isEmpty() || !stderrData.isEmpty()) {
                activityTimer.restart();
            }
            stderr += QString::fromUtf8(stderrData);
            const QList<FfmpegProgress> updates = progressParser.append(stdoutData);
            for (const FfmpegProgress &update : updates) {
                lastFfmpegProgress = update;
                if (update.complete || update.outputMicroseconds >= static_cast<qint64>(exportDuration * 995'000.0)) {
                    finalizing = true;
                }
                reportProgress();
            }
        };
        const auto cancelFfmpeg = [&] {
            result.cancelled = true;
            if (!compositing && stageAStdinCloseReason.isEmpty()) {
                stageAStdinCloseReason = QStringLiteral("export cancellation after %1 of %2 frames submitted")
                                            .arg(result.renderedFrames).arg(expectedFrames);
                activeFfmpeg->closeWriteChannel();
            }
            activeFfmpeg->terminate();
            if (!activeFfmpeg->waitForFinished(5'000)) {
                activeFfmpeg->kill();
                activeFfmpeg->waitForFinished(5'000);
            }
        };
        const auto waitForQueueRoom = [&] {
            if (static_cast<qint64>(activeFfmpeg->bytesToWrite()) <= queueHighWaterMark) return true;
            while (static_cast<qint64>(activeFfmpeg->bytesToWrite()) > queueLowWaterMark) {
                if (isCancelled(settings)) {
                    cancelFfmpeg();
                    return false;
                }
                activeFfmpeg->waitForBytesWritten(250);
                pumpFfmpeg();
                reportProgress();
                if (activityTimer.elapsed() > 300'000) {
                    result.error = QStringLiteral("FFmpeg encoder appears stalled (no progress for five minutes).");
                    activeFfmpeg->terminate();
                    if (!activeFfmpeg->waitForFinished(5'000)) {
                        activeFfmpeg->kill();
                        activeFfmpeg->waitForFinished(5'000);
                    }
                    pumpFfmpeg();
                    result.diagnostics = formatDiagnostics(
                        *activeFfmpeg, lastFfmpegProgress, stderr,
                        compositing ? QStringLiteral("Stage B composition") : QStringLiteral("Stage A overlay"),
                        compositing ? QStringList{} : overlayArguments, temporaryOverlay.fileName(),
                        result.temporaryOverlayBytes, stageAStdinCloseReason);
                    return false;
                }
                if (activeFfmpeg->state() == QProcess::NotRunning) {
                    result.error = !compositing && result.renderedFrames < expectedFrames
                        ? QStringLiteral("Temporary overlay encoder exited early: %1 of %2 frames submitted.")
                              .arg(result.renderedFrames).arg(expectedFrames)
                        : QStringLiteral("FFmpeg stopped before export could be completed.");
                    result.diagnostics = formatDiagnostics(
                        *activeFfmpeg, lastFfmpegProgress, stderr,
                        compositing ? QStringLiteral("Stage B composition") : QStringLiteral("Stage A overlay"),
                        compositing ? QStringList{} : overlayArguments, temporaryOverlay.fileName(),
                        result.temporaryOverlayBytes, stageAStdinCloseReason);
                    return false;
                }
            }
            return true;
        };
        for (qsizetype frameIndex = 0; frameIndex < expectedFrames; ++frameIndex) {
            if (isCancelled(settings)) {
                cancelFfmpeg();
                return result;
            }
            pumpFfmpeg();
            if (ffmpeg.state() == QProcess::NotRunning) {
                refreshTemporaryOverlaySize();
                result.error = QStringLiteral("Temporary overlay encoder exited early: %1 of %2 frames submitted.")
                                   .arg(result.renderedFrames).arg(expectedFrames);
                result.diagnostics = formatDiagnostics(
                    ffmpeg, lastFfmpegProgress, stderr, QStringLiteral("Stage A overlay"), overlayArguments,
                    temporaryOverlay.fileName(), result.temporaryOverlayBytes, stageAStdinCloseReason);
                return result;
            }
            if (!waitForQueueRoom()) return result;
            const double currentSourceVideoTime = sourceVideoTime(
                sourceRangeStart, frameIndex, exportFrameRate);
            QElapsedTimer renderTimer;
            renderTimer.start();
            const QImage image = renderer.renderFrame(currentSourceVideoTime);
            result.renderNanoseconds += renderTimer.nsecsElapsed();
            if (image.size() != outputSize) {
                result.error = renderer.errorString().isEmpty()
                    ? QStringLiteral("Telemetry renderer returned an invalid frame.")
                    : renderer.errorString();
                ffmpeg.kill();
                ffmpeg.waitForFinished();
                return result;
            }
            ++result.generatedFrames;
            QElapsedTimer copyTimer;
            copyTimer.start();
            const QByteArray bytes = rgbaBytes(image, outputSize);
            result.cpuCopyNanoseconds += copyTimer.nsecsElapsed();
            QElapsedTimer writeTimer;
            writeTimer.start();
            if (ffmpeg.write(bytes) != bytes.size()) {
                result.error = QStringLiteral("Could not stream overlay frame to FFmpeg: %1")
                                   .arg(ffmpeg.errorString());
                ffmpeg.kill();
                ffmpeg.waitForFinished();
                pumpFfmpeg();
                result.diagnostics = formatDiagnostics(
                    ffmpeg, lastFfmpegProgress, stderr, QStringLiteral("Stage A overlay"), overlayArguments,
                    temporaryOverlay.fileName(), result.temporaryOverlayBytes, stageAStdinCloseReason);
                return result;
            }
            result.ffmpegWriteNanoseconds += writeTimer.nsecsElapsed();
            ++result.renderedFrames;
            reportProgress();
        }
        Q_ASSERT(result.renderedFrames == expectedFrames);
        stageAStdinCloseReason = QStringLiteral("all %1 expected overlay frames submitted").arg(expectedFrames);
        observe(settings, QStringLiteral("log"), QStringLiteral("renderingOverlay"),
                QStringLiteral("flushTemporaryOverlay"), QStringLiteral("Stage A stdin closed"),
                QStringLiteral("ffmpeg"), {{"reason", stageAStdinCloseReason}});
        observe(settings, QStringLiteral("status"), QStringLiteral("renderingOverlay"),
                QStringLiteral("flushTemporaryOverlay"),
                QStringLiteral("Flushing temporary overlay container"));
        ffmpeg.closeWriteChannel();
        // FFmpeg may still be encoding a bounded amount of raw input and then
        // muxing. There is deliberately no fixed total timeout: only a generous
        // no-activity watchdog detects an actual encoder stall.
        while (ffmpeg.state() != QProcess::NotRunning) {
            if (isCancelled(settings)) {
                cancelFfmpeg();
                return result;
            }
            ffmpeg.waitForFinished(250);
            pumpFfmpeg();
            reportProgress();
            if (activityTimer.elapsed() > 300'000) {
                result.error = QStringLiteral("FFmpeg encoder appears stalled (no progress for five minutes).");
                ffmpeg.terminate();
                if (!ffmpeg.waitForFinished(5'000)) {
                    ffmpeg.kill();
                    ffmpeg.waitForFinished(5'000);
                }
                pumpFfmpeg();
                result.diagnostics = formatDiagnostics(
                    ffmpeg, lastFfmpegProgress, stderr, QStringLiteral("Stage A overlay"), overlayArguments,
                    temporaryOverlay.fileName(), result.temporaryOverlayBytes, stageAStdinCloseReason);
                return result;
            }
        }
        pumpFfmpeg();
        if (ffmpeg.exitStatus() != QProcess::NormalExit || ffmpeg.exitCode() != 0) {
            result.error = QStringLiteral("Temporary overlay encoder failed.");
            result.diagnostics = formatDiagnostics(
                ffmpeg, lastFfmpegProgress, stderr, QStringLiteral("Stage A overlay"), overlayArguments,
                temporaryOverlay.fileName(), result.temporaryOverlayBytes, stageAStdinCloseReason);
            return result;
        }
        observe(settings, QStringLiteral("log"), QStringLiteral("renderingOverlay"),
                QStringLiteral("flushTemporaryOverlay"),
                QStringLiteral("Stage A FFmpeg exited · code %1").arg(ffmpeg.exitCode()),
                QStringLiteral("ffmpeg"), {{"exitCode", ffmpeg.exitCode()},
                                            {"stderr", stderr.right(16 * 1024)}});
        if (!QFileInfo(temporaryOverlay.fileName()).isFile()
            || QFileInfo(temporaryOverlay.fileName()).size() <= 0) {
            result.error = QStringLiteral("FFmpeg did not create the temporary telemetry overlay.");
            return result;
        }
        refreshTemporaryOverlaySize();
        if (result.generatedFrames != expectedFrames || result.renderedFrames != expectedFrames) {
            result.error = QStringLiteral("Temporary telemetry overlay frame count is incomplete.");
            result.diagnostics = QStringLiteral("Generated %1, submitted %2, expected %3 telemetry frames; "
                                                "FFmpeg stored %4 temporary overlay frames.")
                                     .arg(result.generatedFrames).arg(result.renderedFrames).arg(expectedFrames)
                                     .arg(lastFfmpegProgress.encodedFrames);
            return result;
        }
        if (settings.stateCallback) {
            settings.stateCallback(QStringLiteral("validating"));
        }
        observe(settings, QStringLiteral("status"), QStringLiteral("validatingOverlay"),
                QStringLiteral("countTemporaryOverlayFrames"),
                QStringLiteral("Counting temporary overlay frames with ffprobe"));
        observe(settings, QStringLiteral("log"), QStringLiteral("validatingOverlay"),
                QStringLiteral("countTemporaryOverlayFrames"),
                QStringLiteral("Temporary overlay validation started"));
        const MediaInfo temporaryOverlayInfo = MediaProbe::probe(
            temporaryOverlay.fileName(), {}, true, -1,
            probeObservations(settings, QStringLiteral("validatingOverlay"),
                              QStringLiteral("countTemporaryOverlayFrames")));
        const double frameInterval = 1.0 / exportFrameRate.value();
        const bool temporaryCodecOk = temporaryOverlayInfo.videoCodec == QStringLiteral("ffv1");
        const bool temporarySizeOk = temporaryOverlayInfo.videoSize == outputSize;
        const bool temporaryRateOk = temporaryOverlayInfo.averageFrameRate.isEquivalentTo(exportFrameRate);
        const bool temporaryFramesOk = temporaryOverlayInfo.videoFrameCount == expectedFrames;
        const bool temporaryDurationOk = qAbs(temporaryOverlayInfo.duration - exportDuration)
            <= frameInterval * 1.5;
        const auto validationLog = [&](const QString &name, const QVariant &expected,
                                       const QVariant &actual, const bool passed) {
            observe(settings, QStringLiteral("log"), QStringLiteral("validatingOverlay"),
                    QStringLiteral("validateTemporaryOverlay"),
                    QStringLiteral("%1 expected=%2 actual=%3 %4")
                        .arg(name, expected.toString(), actual.toString(),
                             passed ? QStringLiteral("PASS") : QStringLiteral("FAIL")),
                    QStringLiteral("validation"),
                    {{"check", name}, {"expected", expected}, {"actual", actual},
                     {"passed", passed}});
        };
        validationLog(QStringLiteral("Temporary codec"), QStringLiteral("ffv1"),
                      temporaryOverlayInfo.videoCodec, temporaryCodecOk);
        validationLog(QStringLiteral("Temporary resolution"),
                      QStringLiteral("%1x%2").arg(outputSize.width()).arg(outputSize.height()),
                      QStringLiteral("%1x%2").arg(temporaryOverlayInfo.videoSize.width())
                                               .arg(temporaryOverlayInfo.videoSize.height()), temporarySizeOk);
        validationLog(QStringLiteral("Temporary frame rate"), rateString(exportFrameRate),
                      rateString(temporaryOverlayInfo.averageFrameRate), temporaryRateOk);
        validationLog(QStringLiteral("Temporary frames"), expectedFrames,
                      temporaryOverlayInfo.videoFrameCount, temporaryFramesOk);
        validationLog(QStringLiteral("Temporary duration"), exportDuration,
                      temporaryOverlayInfo.duration, temporaryDurationOk);
        if (!temporaryCodecOk || !temporarySizeOk || !temporaryRateOk
            || !temporaryFramesOk || !temporaryDurationOk) {
            result.error = QStringLiteral("Temporary telemetry overlay failed validation.");
            result.diagnostics = QStringLiteral(
                "Expected FFV1 %1x%2 at %3 fps, %4 frames, %5 s; staged %6 %7x%8 at %9 fps, %10 frames, %11 s, %12 bytes.\n"
                "FFmpeg arguments: %13")
                                     .arg(outputSize.width()).arg(outputSize.height())
                                     .arg(exportFrameRate.value(), 0, 'f', 6).arg(expectedFrames)
                                     .arg(exportDuration, 0, 'f', 6)
                                     .arg(temporaryOverlayInfo.videoCodec)
                                     .arg(temporaryOverlayInfo.videoSize.width())
                                     .arg(temporaryOverlayInfo.videoSize.height())
                                     .arg(temporaryOverlayInfo.averageFrameRate.value(), 0, 'f', 6)
                                     .arg(temporaryOverlayInfo.videoFrameCount)
                                     .arg(temporaryOverlayInfo.duration, 0, 'f', 6)
                                     .arg(result.temporaryOverlayBytes)
                                     .arg(formatArgumentList(overlayArguments));
            return result;
        }
        observe(settings, QStringLiteral("log"), QStringLiteral("validatingOverlay"),
                QStringLiteral("validateTemporaryOverlay"),
                QStringLiteral("Temporary overlay validation passed"));
        // Framesync selects the latest secondary frame at or before each primary
        // timestamp. A live secondary pipe can therefore repeat stale telemetry
        // while the primary decoder advances. Compose only after a complete,
        // rational-rate FFV1/BGRA overlay stream exists on disk.
        QProcess compositor;
        activeFfmpeg = &compositor;
        progressParser = FfmpegProgressParser{};
        lastFfmpegProgress = {};
        stderr.clear();
        activityTimer.restart();
        compositing = true;
        finalizing = false;
        const QString timeRangeFilter = QStringLiteral(
            "[0:v]trim=start=%1:end=%2,setpts=PTS-STARTPTS,"
            "fps=fps=%3:start_time=0:round=near:eof_action=round,"
            "trim=end_frame=%4,setpts=PTS-STARTPTS[sourceVideo];"
            "[1:v]setpts=PTS-STARTPTS[temporaryOverlay];"
            "[sourceVideo][temporaryOverlay]overlay=0:0:shortest=1:repeatlast=0:eof_action=endall:format=auto[video]")
                                            .arg(sourceRangeStart, 0, 'f', 9)
                                            .arg(sourceRangeEnd, 0, 'f', 9)
                                            .arg(rateString(exportFrameRate))
                                            .arg(expectedFrames);
        QStringList compositionArguments = {
            "-hide_banner", "-loglevel", "error", "-nostats", "-progress", "pipe:1", "-y",
            "-i", settings.inputPath, "-i", temporaryOverlay.fileName(),
            "-filter_complex", timeRangeFilter,
            "-map", "[video]", "-fps_mode:v", "cfr", "-c:v", encoder,
            "-b:v", qualityBitrate(settings.quality),
            "-tag:v", "hvc1", "-pix_fmt", "yuv420p",
        };
        if (settings.audioEnabled && !source.audioCodecs.isEmpty()) {
            compositionArguments[compositionArguments.indexOf("-filter_complex") + 1] += QStringLiteral(
                ";[0:a]atrim=start=%1:end=%2,asetpts=PTS-STARTPTS[audio]")
                .arg(sourceRangeStart, 0, 'f', 9).arg(sourceRangeEnd, 0, 'f', 9);
            compositionArguments.append({"-map", "[audio]", "-c:a", "aac", "-b:a", "192k"});
        } else {
            compositionArguments.append("-an");
        }
        compositionArguments.append(settings.outputPath);
        observe(settings, QStringLiteral("status"), QStringLiteral("encodingVideo"),
                QStringLiteral("startFinalComposition"),
                QStringLiteral("Starting final HEVC composition"));
        observe(settings, QStringLiteral("log"), QStringLiteral("encodingVideo"),
                QStringLiteral("startFinalComposition"), QStringLiteral("Stage B started"),
                QStringLiteral("ffmpeg"),
                {{"executable", FfmpegTools::ffmpegPath()}, {"arguments", compositionArguments}});
        compositor.setProcessChannelMode(QProcess::SeparateChannels);
        qInfo().noquote() << QStringLiteral("Stage B FFmpeg arguments: %1").arg(formatArgumentList(compositionArguments));
        compositor.start(FfmpegTools::ffmpegPath(), compositionArguments);
        if (!compositor.waitForStarted()) {
            result.error = QStringLiteral("Could not start FFmpeg composition: %1").arg(compositor.errorString());
            return result;
        }
        while (compositor.state() != QProcess::NotRunning) {
            if (isCancelled(settings)) {
                cancelFfmpeg();
                return result;
            }
            compositor.waitForFinished(250);
            pumpFfmpeg();
            reportProgress();
            if (lastFfmpegProgress.encodedFrames > result.renderedFrames) {
                result.error = QStringLiteral("FFmpeg output advanced beyond available telemetry overlay frames.");
                compositor.terminate();
                if (!compositor.waitForFinished(5'000)) compositor.kill();
                pumpFfmpeg();
                result.diagnostics = formatDiagnostics(
                    compositor, lastFfmpegProgress, stderr, QStringLiteral("Stage B composition"),
                    compositionArguments, temporaryOverlay.fileName(), result.temporaryOverlayBytes,
                    stageAStdinCloseReason);
                return result;
            }
            if (activityTimer.elapsed() > 300'000) {
                result.error = QStringLiteral("FFmpeg encoder appears stalled (no progress for five minutes).");
                compositor.terminate();
                if (!compositor.waitForFinished(5'000)) {
                    compositor.kill();
                    compositor.waitForFinished(5'000);
                }
                pumpFfmpeg();
                result.diagnostics = formatDiagnostics(
                    compositor, lastFfmpegProgress, stderr, QStringLiteral("Stage B composition"),
                    compositionArguments, temporaryOverlay.fileName(), result.temporaryOverlayBytes,
                    stageAStdinCloseReason);
                return result;
            }
        }
        pumpFfmpeg();
        observe(settings, QStringLiteral("status"), QStringLiteral("encodingVideo"),
                QStringLiteral("flushOutputContainer"), QStringLiteral("Flushing MP4 container"));
        if (compositor.exitStatus() != QProcess::NormalExit || compositor.exitCode() != 0) {
            result.error = QStringLiteral("FFmpeg composition failed.");
            result.diagnostics = formatDiagnostics(
                compositor, lastFfmpegProgress, stderr, QStringLiteral("Stage B composition"),
                compositionArguments, temporaryOverlay.fileName(), result.temporaryOverlayBytes,
                stageAStdinCloseReason);
            return result;
        }
        observe(settings, QStringLiteral("log"), QStringLiteral("encodingVideo"),
                QStringLiteral("flushOutputContainer"),
                QStringLiteral("Stage B FFmpeg exited · code %1").arg(compositor.exitCode()),
                QStringLiteral("ffmpeg"), {{"exitCode", compositor.exitCode()},
                                            {"stderr", stderr.right(16 * 1024)}});
        if (lastFfmpegProgress.encodedFrames != expectedFrames) {
            result.error = QStringLiteral("FFmpeg output advanced beyond the telemetry overlay.");
            result.diagnostics = QStringLiteral("Prepared %1 telemetry frames; FFmpeg reported %2 output frames.")
                                     .arg(expectedFrames).arg(lastFfmpegProgress.encodedFrames);
            return result;
        }
        observe(settings, QStringLiteral("status"), QStringLiteral("validatingOutput"),
                QStringLiteral("checkOutputExists"), QStringLiteral("Checking output file exists"));
        const QFileInfo outputInfo(settings.outputPath);
        const bool outputExists = outputInfo.isFile();
        observe(settings, QStringLiteral("log"), QStringLiteral("validatingOutput"),
                QStringLiteral("checkOutputExists"),
                QStringLiteral("Output file exists actual=%1 %2")
                    .arg(outputExists ? QStringLiteral("yes") : QStringLiteral("no"),
                         outputExists ? QStringLiteral("PASS") : QStringLiteral("FAIL")),
                QStringLiteral("validation"), {{"passed", outputExists}});
        observe(settings, QStringLiteral("status"), QStringLiteral("validatingOutput"),
                QStringLiteral("checkOutputSize"), QStringLiteral("Checking output file size"));
        const bool outputSizeOk = outputInfo.size() > 0;
        observe(settings, QStringLiteral("log"), QStringLiteral("validatingOutput"),
                QStringLiteral("checkOutputSize"),
                QStringLiteral("Output size actual=%1 bytes %2")
                    .arg(outputInfo.size()).arg(outputSizeOk ? QStringLiteral("PASS")
                                                            : QStringLiteral("FAIL")),
                QStringLiteral("validation"), {{"bytes", outputInfo.size()}, {"passed", outputSizeOk}});
        if (!outputExists || !outputSizeOk) {
            result.error = QStringLiteral("FFmpeg did not create an output file.");
            return result;
        }
        result.outputBytes = outputInfo.size();
        result.encodedFrames = lastFfmpegProgress.encodedFrames;
        result.encodedSeconds = qMax(0.0, lastFfmpegProgress.outputMicroseconds / 1'000'000.0);
        const TelemetryFrameRenderer::TimingMetrics rendererMetrics = renderer.timingMetrics();
        result.polishNanoseconds = rendererMetrics.polishNanoseconds;
        result.syncRenderNanoseconds = rendererMetrics.syncRenderNanoseconds;
        result.readbackNanoseconds = rendererMetrics.readbackNanoseconds;
        if (settings.stateCallback) {
            settings.stateCallback(QStringLiteral("validating"));
        }
        observe(settings, QStringLiteral("status"), QStringLiteral("validatingOutput"),
                QStringLiteral("probeFinalOutput"),
                QStringLiteral("Reading output metadata with ffprobe"));
        observe(settings, QStringLiteral("log"), QStringLiteral("validatingOutput"),
                QStringLiteral("probeFinalOutput"), QStringLiteral("Final validation started"));
        try {
            result.mediaInfo = MediaProbe::probe(
                settings.outputPath, {}, false, 30'000,
                probeObservations(settings, QStringLiteral("validatingOutput"),
                                  QStringLiteral("probeFinalOutput")), {}, true);
        } catch (const std::exception &error) {
            result.error = QStringLiteral("Automatic media validation failed; the staged output was not committed.");
            result.diagnostics = QString::fromUtf8(error.what());
            return result;
        }
        const bool codecOk = result.mediaInfo.videoCodec == QStringLiteral("hevc");
        const bool dimensionsOk = result.mediaInfo.videoSize == outputSize;
        const bool averageRateOk = result.mediaInfo.averageFrameRate.isEquivalentTo(exportFrameRate);
        const bool nominalRateOk = result.mediaInfo.frameRate.isEquivalentTo(exportFrameRate);
        const bool packetCountAvailable = result.mediaInfo.videoPacketCount > 0;
        const bool packetCountOk = !packetCountAvailable
            || result.mediaInfo.videoPacketCount == expectedFrames;
        const double videoStartTolerance = result.mediaInfo.timeBase.isValid()
            ? result.mediaInfo.timeBase.value() : frameInterval;
        const bool videoStartOk = qAbs(result.mediaInfo.videoStartTime) <= videoStartTolerance;
        const double videoDuration = result.mediaInfo.videoDuration > 0.0
            ? result.mediaInfo.videoDuration : result.mediaInfo.duration;
        const bool durationOk = qAbs(videoDuration - exportDuration) <= frameInterval;
        const bool audioExpected = settings.audioEnabled && !source.audioCodecs.isEmpty();
        const double audioFrameDuration = result.mediaInfo.audioSampleRate > 0
            ? 1024.0 / result.mediaInfo.audioSampleRate : frameInterval;
        const double audioTimingTolerance = qMax(
            result.mediaInfo.audioTimeBase.isValid() ? result.mediaInfo.audioTimeBase.value() : 0.0,
            audioFrameDuration);
        const bool audioPresent = !result.mediaInfo.audioCodecs.isEmpty();
        const bool audioStartOk = !audioExpected
            || qAbs(result.mediaInfo.audioStartTime - result.mediaInfo.videoStartTime)
                <= audioTimingTolerance;
        const bool audioDurationOk = !audioExpected
            || qAbs(result.mediaInfo.audioDuration - requestedDuration) <= audioTimingTolerance;
        const bool audioOk = !audioExpected || (audioPresent && audioStartOk && audioDurationOk);
        const auto finalValidationLog = [&](const QString &operation, const QString &name,
                                            const QVariant &expected, const QVariant &actual,
                                            const bool passed) {
            observe(settings, QStringLiteral("status"), QStringLiteral("validatingOutput"), operation,
                    QStringLiteral("Checking %1").arg(name.toLower()));
            observe(settings, QStringLiteral("log"), QStringLiteral("validatingOutput"), operation,
                    QStringLiteral("%1 expected=%2 actual=%3 %4")
                        .arg(name, expected.toString(), actual.toString(),
                             passed ? QStringLiteral("PASS") : QStringLiteral("FAIL")),
                    QStringLiteral("validation"),
                    {{"check", name}, {"expected", expected}, {"actual", actual},
                     {"passed", passed}});
        };
        finalValidationLog(QStringLiteral("checkVideoCodec"), QStringLiteral("Video codec"),
                           QStringLiteral("hevc"), result.mediaInfo.videoCodec, codecOk);
        finalValidationLog(QStringLiteral("checkDimensions"), QStringLiteral("Resolution"),
                           QStringLiteral("%1x%2").arg(outputSize.width()).arg(outputSize.height()),
                           QStringLiteral("%1x%2").arg(result.mediaInfo.videoSize.width())
                                                    .arg(result.mediaInfo.videoSize.height()), dimensionsOk);
        finalValidationLog(QStringLiteral("checkDuration"), QStringLiteral("Duration"),
                           QString::number(exportDuration, 'f', 6),
                           QString::number(videoDuration, 'f', 6), durationOk);
        finalValidationLog(QStringLiteral("checkAverageFrameRate"), QStringLiteral("Average frame rate"),
                           rateString(exportFrameRate), rateString(result.mediaInfo.averageFrameRate), averageRateOk);
        finalValidationLog(QStringLiteral("checkNominalFrameRate"), QStringLiteral("Nominal frame rate"),
                           rateString(exportFrameRate), rateString(result.mediaInfo.frameRate), nominalRateOk);
        finalValidationLog(QStringLiteral("checkPacketCount"), QStringLiteral("Video packet count"),
                           expectedFrames,
                           packetCountAvailable ? QVariant::fromValue(result.mediaInfo.videoPacketCount)
                                                : QVariant(QStringLiteral("unavailable")), packetCountOk);
        finalValidationLog(QStringLiteral("checkVideoStart"), QStringLiteral("Video start"),
                           QStringLiteral("0"),
                           QString::number(result.mediaInfo.videoStartTime, 'f', 9), videoStartOk);
        finalValidationLog(QStringLiteral("checkAudio"), QStringLiteral("Audio"),
                           audioExpected ? QStringLiteral("yes") : QStringLiteral("not required"),
                           result.mediaInfo.audioCodecs.isEmpty()
                               ? QStringLiteral("none") : result.mediaInfo.audioCodecs.join(','), audioOk);
        if (audioExpected) {
            finalValidationLog(QStringLiteral("checkAudioStart"), QStringLiteral("A/V start delta"),
                               QStringLiteral("≤ %1 s").arg(audioTimingTolerance, 0, 'f', 6),
                               QString::number(qAbs(result.mediaInfo.audioStartTime
                                                    - result.mediaInfo.videoStartTime), 'f', 9),
                               audioStartOk);
            finalValidationLog(QStringLiteral("checkAudioDuration"), QStringLiteral("Audio duration"),
                               QString::number(requestedDuration, 'f', 6),
                               QString::number(result.mediaInfo.audioDuration, 'f', 6), audioDurationOk);
        }
        if (!codecOk || !dimensionsOk || !averageRateOk || !nominalRateOk || !packetCountOk
            || !videoStartOk || !durationOk || !audioOk) {
            result.error = QStringLiteral("Export failed final timing validation.");
            return result;
        }
        observe(settings, QStringLiteral("log"), QStringLiteral("validatingOutput"),
                QStringLiteral("validationComplete"), QStringLiteral("Final validation passed"),
                QStringLiteral("validation"));
        result.success = true;
    } catch (const std::exception &error) {
        result.error = QString::fromUtf8(error.what());
    }
    return result;
}

} // namespace FlappedEar
