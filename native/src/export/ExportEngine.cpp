#include "export/ExportEngine.h"

#include "export/EncoderDetector.h"
#include "export/FfmpegTools.h"
#include "export/ExportProgress.h"
#include "export/TelemetryFrameRenderer.h"

#include <QFileInfo>
#include <QProcess>
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

QString formatDiagnostics(const QProcess &process, const FfmpegProgress &progress, const QString &stderr)
{
    QString details = QStringLiteral("Exit code: %1\nExit status: %2\nLast encoded frame: %3\n"
                                     "Last encoded time: %4 s\nEncoder fps: %5\nEncoder realtime: %6x")
                          .arg(process.exitCode())
                          .arg(process.exitStatus() == QProcess::NormalExit ? QStringLiteral("normal")
                                                                              : QStringLiteral("crash"))
                          .arg(progress.encodedFrames)
                          .arg(progress.outputMicroseconds >= 0
                                   ? QString::number(progress.outputMicroseconds / 1'000'000.0, 'f', 3)
                                   : QStringLiteral("unavailable"))
                          .arg(progress.encoderFps, 0, 'f', 2)
                          .arg(progress.realtimeFactor, 0, 'f', 2);
    if (!stderr.trimmed().isEmpty()) {
        details += QStringLiteral("\n\nFFmpeg stderr:\n") + stderr.trimmed();
    }
    return details;
}

} // namespace

qsizetype ExportEngine::frameCount(
    const double startTime, const double endTime, const MediaRational &frameRate)
{
    if (!frameRate.isValid() || !std::isfinite(startTime) || !std::isfinite(endTime)
        || endTime <= startTime) {
        return 0;
    }
    return static_cast<qsizetype>(
        std::ceil((endTime - startTime) * static_cast<double>(frameRate.numerator)
                  / static_cast<double>(frameRate.denominator) - 1e-9));
}

double ExportEngine::framePresentationTime(
    const double startTime, const qsizetype frameIndex, const MediaRational &frameRate)
{
    if (!frameRate.isValid()) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    return startTime + static_cast<double>(frameIndex) * static_cast<double>(frameRate.denominator)
        / static_cast<double>(frameRate.numerator);
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
        if (settings.stateCallback) {
            settings.stateCallback(QStringLiteral("starting"));
        }
        const MediaInfo source = MediaProbe::probe(settings.inputPath);
        const QSize outputSize = settings.outputSize.isValid() ? settings.outputSize : source.videoSize;
        const MediaRational frameRate = settings.frameRate.isValid()
            ? settings.frameRate
            : (source.averageFrameRate.isValid() ? source.averageFrameRate : source.frameRate);
        const double start = qMax(0.0, settings.startTime);
        const double end = settings.endTime > start ? qMin(settings.endTime, source.duration) : source.duration;
        const qsizetype frames = frameCount(start, end, frameRate);
        if (frames == 0 || !outputSize.isValid()) {
            result.error = QStringLiteral("Export range or frame rate is invalid.");
            return result;
        }
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
        QProcess ffmpeg;
        const QString duration = QString::number(end - start, 'f', 9);
        const QString size = QStringLiteral("%1x%2").arg(outputSize.width()).arg(outputSize.height());
        QStringList arguments = {
            "-hide_banner", "-loglevel", "error", "-nostats", "-progress", "pipe:1", "-y",
            "-ss", QString::number(start, 'f', 9), "-i", settings.inputPath,
            "-f", "rawvideo", "-pixel_format", "rgba", "-video_size", size,
            "-framerate", rateString(frameRate), "-i", "pipe:0", "-t", duration,
            "-filter_complex", "[0:v][1:v]overlay=0:0:format=auto[v]", "-map", "[v]",
            "-c:v", encoder, "-b:v", qualityBitrate(settings.quality), "-tag:v", "hvc1",
            "-pix_fmt", "yuv420p",
        };
        if (settings.audioEnabled && !source.audioCodecs.isEmpty()) {
            arguments.append({"-map", "0:a?", "-c:a", "aac", "-b:a", "192k"});
        } else {
            arguments.append("-an");
        }
        arguments.append(settings.outputPath);
        ffmpeg.setProcessChannelMode(QProcess::SeparateChannels);
        ffmpeg.start(FfmpegTools::ffmpegPath(), arguments);
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
        const auto reportProgress = [&] {
            const qint64 queuedBytes = static_cast<qint64>(ffmpeg.bytesToWrite());
            result.maximumQueuedBytes = qMax(result.maximumQueuedBytes, queuedBytes);
            if (!settings.progressCallback) return;
            ExportPipelineProgress progress;
            progress.submittedFrames = result.renderedFrames;
            progress.totalFrames = frames;
            progress.submittedSourceTime = result.renderedFrames > 0
                ? framePresentationTime(start, result.renderedFrames - 1, frameRate) : start;
            progress.encodedFrames = lastFfmpegProgress.encodedFrames;
            progress.encodedSeconds = qMax(0.0, lastFfmpegProgress.outputMicroseconds / 1'000'000.0);
            progress.outputDurationSeconds = end - start;
            progress.queuedBytes = queuedBytes;
            progress.maximumQueuedBytes = result.maximumQueuedBytes;
            progress.encoderFps = lastFfmpegProgress.encoderFps;
            progress.encoderRealtimeFactor = lastFfmpegProgress.realtimeFactor;
            progress.stage = finalizing ? QStringLiteral("finalizing") : QStringLiteral("rendering");
            settings.progressCallback(progress);
        };
        const auto pumpFfmpeg = [&] {
            const QByteArray stdoutData = ffmpeg.readAllStandardOutput();
            const QByteArray stderrData = ffmpeg.readAllStandardError();
            if (!stdoutData.isEmpty() || !stderrData.isEmpty()) {
                activityTimer.restart();
            }
            stderr += QString::fromUtf8(stderrData);
            const QList<FfmpegProgress> updates = progressParser.append(stdoutData);
            for (const FfmpegProgress &update : updates) {
                lastFfmpegProgress = update;
                if (update.complete || update.outputMicroseconds >= static_cast<qint64>((end - start) * 995'000.0)) {
                    finalizing = true;
                }
                reportProgress();
            }
        };
        const auto cancelFfmpeg = [&] {
            result.cancelled = true;
            ffmpeg.closeWriteChannel();
            ffmpeg.terminate();
            if (!ffmpeg.waitForFinished(5'000)) {
                ffmpeg.kill();
                ffmpeg.waitForFinished(5'000);
            }
            QFile::remove(settings.outputPath);
        };
        const auto waitForQueueRoom = [&] {
            if (static_cast<qint64>(ffmpeg.bytesToWrite()) <= queueHighWaterMark) return true;
            while (static_cast<qint64>(ffmpeg.bytesToWrite()) > queueLowWaterMark) {
                if (isCancelled(settings)) {
                    cancelFfmpeg();
                    return false;
                }
                ffmpeg.waitForBytesWritten(250);
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
                    result.diagnostics = formatDiagnostics(ffmpeg, lastFfmpegProgress, stderr);
                    return false;
                }
                if (ffmpeg.state() == QProcess::NotRunning) {
                    result.error = QStringLiteral("FFmpeg stopped before export could be completed.");
                    result.diagnostics = formatDiagnostics(ffmpeg, lastFfmpegProgress, stderr);
                    return false;
                }
            }
            return true;
        };
        for (qsizetype frame = 0; frame < frames; ++frame) {
            if (isCancelled(settings)) {
                cancelFfmpeg();
                return result;
            }
            pumpFfmpeg();
            if (!waitForQueueRoom()) return result;
            const double presentationTime = framePresentationTime(start, frame, frameRate);
            QElapsedTimer renderTimer;
            renderTimer.start();
            const QImage image = renderer.renderFrame(presentationTime);
            result.renderNanoseconds += renderTimer.nsecsElapsed();
            if (image.size() != outputSize) {
                result.error = renderer.errorString().isEmpty()
                    ? QStringLiteral("Telemetry renderer returned an invalid frame.")
                    : renderer.errorString();
                ffmpeg.kill();
                ffmpeg.waitForFinished();
                return result;
            }
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
                result.diagnostics = formatDiagnostics(ffmpeg, lastFfmpegProgress, stderr);
                return result;
            }
            result.ffmpegWriteNanoseconds += writeTimer.nsecsElapsed();
            ++result.renderedFrames;
            reportProgress();
        }
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
                result.diagnostics = formatDiagnostics(ffmpeg, lastFfmpegProgress, stderr);
                return result;
            }
        }
        pumpFfmpeg();
        if (ffmpeg.exitStatus() != QProcess::NormalExit || ffmpeg.exitCode() != 0) {
            result.error = QStringLiteral("FFmpeg stopped before export could be completed.");
            result.diagnostics = formatDiagnostics(ffmpeg, lastFfmpegProgress, stderr);
            return result;
        }
        if (!QFileInfo(settings.outputPath).isFile() || QFileInfo(settings.outputPath).size() <= 0) {
            result.error = QStringLiteral("FFmpeg did not create an output file.");
            return result;
        }
        if (settings.stateCallback) {
            settings.stateCallback(QStringLiteral("validating"));
        }
        result.mediaInfo = MediaProbe::probe(settings.outputPath);
        if (result.mediaInfo.videoCodec != "hevc" || result.mediaInfo.videoSize != outputSize
            || qAbs(result.mediaInfo.duration - (end - start))
                > 2.0 / qMax(1.0, frameRate.value())
            || (settings.audioEnabled && !source.audioCodecs.isEmpty()
                && result.mediaInfo.audioCodecs.isEmpty())) {
            result.error = QStringLiteral("Export failed validation (codec, size, or duration).");
            return result;
        }
        result.success = true;
        result.encodedFrames = lastFfmpegProgress.encodedFrames;
        result.encodedSeconds = qMax(0.0, lastFfmpegProgress.outputMicroseconds / 1'000'000.0);
        const TelemetryFrameRenderer::TimingMetrics rendererMetrics = renderer.timingMetrics();
        result.polishNanoseconds = rendererMetrics.polishNanoseconds;
        result.syncRenderNanoseconds = rendererMetrics.syncRenderNanoseconds;
        result.readbackNanoseconds = rendererMetrics.readbackNanoseconds;
    } catch (const std::exception &error) {
        result.error = QString::fromUtf8(error.what());
    }
    return result;
}

} // namespace FlappedEar
