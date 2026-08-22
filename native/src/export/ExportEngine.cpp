#include "export/ExportEngine.h"

#include "export/EncoderDetector.h"
#include "export/ExportArtifactManifest.h"
#include "export/ExportDiagnostics.h"
#include "export/ExportProcessSupervisor.h"
#include "export/ExportStoragePolicy.h"
#include "export/ExportFormat.h"
#include "export/FfmpegTools.h"
#include "export/ExportProgress.h"
#include "export/RawFrameTransport.h"
#include "export/TelemetryFrameRenderer.h"
#include "export/TemporaryOverlayValidation.h"

#include <QFileInfo>
#include <QDir>
#include <QProcess>
#include <QTemporaryFile>
#include <QElapsedTimer>
#include <QScopeGuard>
#include <QUuid>
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

bool isCancelled(const ExportSettings &settings)
{
    return !settings.cancellationFilePath.isEmpty() && QFileInfo::exists(settings.cancellationFilePath);
}

bool updateManifestState(const ExportSettings &settings, const QString &state)
{
    if (settings.manifestPath.isEmpty()) return true;
    ExportArtifactManifestData manifest;
    if (!ExportArtifactManifest::read(settings.manifestPath, &manifest)) return false;
    manifest.state = state;
    return ExportArtifactManifest::update(settings.manifestPath, manifest);
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

struct OverlaySampleResult {
    qsizetype frames = 0;
    qint64 encodedBytes = 0;
    qint64 maximumQueuedBytes = 0;
    QString error;
    bool cancelled = false;
};

OverlaySampleResult sampleTemporaryOverlay(
    const ExportSettings &settings, TelemetryFrameRenderer &renderer, const QSize &outputSize,
    const double sourceRangeStart, const qsizetype expectedFrames, const MediaRational &frameRate)
{
    OverlaySampleResult result;
    const qsizetype sampleFrames = qMin<qsizetype>(24, expectedFrames);
    if (sampleFrames == 0) return result;
    const QString size = QStringLiteral("%1x%2").arg(outputSize.width()).arg(outputSize.height());
    const QStringList arguments = {"-hide_banner", "-loglevel", "error", "-nostats", "-y",
        "-f", "rawvideo", "-pixel_format", "rgba", "-video_size", size,
        "-framerate", rateString(frameRate), "-i", "pipe:0", "-an", "-c:v", "ffv1",
        "-pix_fmt", "bgra", "-f", "matroska", "pipe:1"};
    QProcess process;
    process.setProcessChannelMode(QProcess::SeparateChannels);
    ExportProcessSupervisor supervisor(process, false);
    supervisor.start(FfmpegTools::ffmpegPath(), arguments);
    if (!supervisor.waitForStarted(5'000)) {
        result.error = QStringLiteral("Could not start FFmpeg representative sample: %1").arg(process.errorString());
        return result;
    }
    QByteArray encoded;
    QString standardErrorText;
    const auto drain = [&] {
        encoded.append(process.readAllStandardOutput());
        standardErrorText += QString::fromUtf8(process.readAllStandardError());
    };
    RawFrameTransport transport(
        process, {}, [&settings] { return isCancelled(settings); }, drain,
        [&result](const qint64, const qint64 maximumQueuedBytes) {
            result.maximumQueuedBytes = maximumQueuedBytes;
        });
    for (qsizetype sampleIndex = 0; sampleIndex < sampleFrames; ++sampleIndex) {
        if (isCancelled(settings)) {
            result.cancelled = true;
            static_cast<void>(supervisor.stopAndWait());
            return result;
        }
        const qsizetype frameIndex = sampleFrames == 1 ? 0
            : (sampleIndex * (expectedFrames - 1)) / (sampleFrames - 1);
        const QImage image = renderer.renderFrame(
            ExportEngine::sourceVideoTime(sourceRangeStart, frameIndex, frameRate));
        if (image.size() != outputSize) {
            result.error = renderer.errorString().isEmpty()
                ? QStringLiteral("Telemetry renderer could not produce a representative sample frame.")
                : renderer.errorString();
            static_cast<void>(supervisor.stopAndWait());
            return result;
        }
        const QByteArray bytes = rgbaBytes(image, outputSize);
        const RawFrameTransportResult writeResult = transport.writeFrame(bytes);
        if (writeResult.status == RawFrameTransportResult::Status::Cancelled) {
            result.cancelled = true;
            static_cast<void>(supervisor.stopAndWait());
            return result;
        }
        if (!writeResult.succeeded()) {
            result.error = QStringLiteral("Could not stream representative sample frame to FFmpeg: %1")
                .arg(writeResult.error);
            static_cast<void>(supervisor.stopAndWait());
            return result;
        }
        ++result.frames;
        drain();
    }
    process.closeWriteChannel();
    QElapsedTimer elapsed;
    elapsed.start();
    while (process.state() != QProcess::NotRunning && elapsed.elapsed() < 30'000) {
        if (isCancelled(settings)) {
            result.cancelled = true;
            static_cast<void>(supervisor.stopAndWait());
            return result;
        }
        process.waitForFinished(100);
        drain();
    }
    drain();
    if (process.state() != QProcess::NotRunning) {
        result.error = QStringLiteral("FFmpeg representative sample timed out.");
        static_cast<void>(supervisor.stopAndWait());
        return result;
    }
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0 || encoded.isEmpty()) {
        result.error = QStringLiteral("FFmpeg representative sample failed: %1")
                           .arg(standardErrorText.trimmed());
        return result;
    }
    result.encodedBytes = encoded.size();
    return result;
}

QString formatDiagnostics(
    const QProcess &process,
    const FfmpegProgress &progress,
    const QString &standardErrorText,
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
    if (!standardErrorText.trimmed().isEmpty()) {
        details += QStringLiteral("\n\nFFmpeg stderr:\n") + standardErrorText.trimmed();
    }
    return details;
}

QString processErrorName(const QProcess::ProcessError error)
{
    switch (error) {
    case QProcess::FailedToStart: return QStringLiteral("FailedToStart");
    case QProcess::Crashed: return QStringLiteral("Crashed");
    case QProcess::Timedout: return QStringLiteral("Timedout");
    case QProcess::WriteError: return QStringLiteral("WriteError");
    case QProcess::ReadError: return QStringLiteral("ReadError");
    case QProcess::UnknownError: return QStringLiteral("UnknownError");
    }
    return QStringLiteral("UnknownError");
}

QString processExitStatusName(const QProcess::ExitStatus status)
{
    return status == QProcess::NormalExit ? QStringLiteral("NormalExit")
                                          : QStringLiteral("CrashExit (Unix signal unavailable from QProcess)");
}

struct FilesystemSnapshot {
    QString root;
    QString inspectedPath;
    QString probePath;
    qint64 availableBytes = -1;
    qint64 totalBytes = -1;
};

FilesystemSnapshot filesystemSnapshot(const QString &path)
{
    const ExportFilesystemInfo filesystem = ExportStoragePolicy::filesystemForPath(path);
    return {filesystem.rootPath, filesystem.inspectedPath, filesystem.probePath,
            filesystem.availableBytes, filesystem.totalBytes};
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
            probeObservations(settings, QStringLiteral("preparing"), QStringLiteral("probeInput")),
            [&settings] { return isCancelled(settings); });
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
        const QList<EncoderCapability> encoders = EncoderDetector::discover(
            {}, [&settings] { return isCancelled(settings); });
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
        const QString temporaryOverlayPath = settings.temporaryOverlayPath.isEmpty()
            ? QDir::temp().filePath(QStringLiteral("flappedear-overlay-%1.mkv")
                  .arg(QUuid::createUuid().toString(QUuid::WithoutBraces)))
            : settings.temporaryOverlayPath;
        observe(settings, QStringLiteral("status"), QStringLiteral("preparing"),
                QStringLiteral("sampleTemporaryOverlay"),
                QStringLiteral("Measuring representative telemetry overlay sample"));
        const OverlaySampleResult overlaySample = sampleTemporaryOverlay(
            settings, renderer, outputSize, sourceRangeStart, expectedFrames, exportFrameRate);
        if (overlaySample.cancelled) {
            result.cancelled = true;
            return result;
        }
        const ExportStorageEstimate storageEstimate = overlaySample.error.isEmpty()
            ? ExportStoragePolicy::estimateFromSample(overlaySample.encodedBytes, overlaySample.frames,
                                                       expectedFrames, exportDuration, settings.videoBitrate)
            : ExportStoragePolicy::estimate(expectedFrames, outputSize, exportDuration, settings.videoBitrate);
        const ExportStoragePreflight storagePreflight = ExportStoragePolicy::evaluate(
            temporaryOverlayPath, settings.outputPath, storageEstimate);
        observe(settings, QStringLiteral("log"), QStringLiteral("preparing"),
                QStringLiteral("preflightStorage"), QStringLiteral("Export storage preflight"),
                QStringLiteral("storage"), {{"estimatedTemporaryOverlayBytes", storageEstimate.temporaryOverlayBytes},
                {"estimatedFinalOutputBytes", storageEstimate.finalOutputBytes},
                {"safetyReserveBytes", storageEstimate.safetyReserveBytes},
                {"estimateBasis", ExportStoragePolicy::estimateBasisText(storageEstimate.basis)},
                {"sampleFrames", static_cast<qint64>(storageEstimate.sampleFrames)},
                {"sampleEncodedBytes", storageEstimate.sampleBytes},
                {"sampleBytesPerFrame", storageEstimate.bytesPerFrame},
                {"sampleSafetyMargin", storageEstimate.safetyMargin},
                {"sampleError", overlaySample.error},
                {"sampleMaximumQueuedBytes", overlaySample.maximumQueuedBytes},
                {"temporaryFilesystemRoot", storagePreflight.temporaryFilesystem.rootPath},
                {"temporaryFilesystemInspectedPath", storagePreflight.temporaryFilesystem.inspectedPath},
                {"temporaryFilesystemProbePath", storagePreflight.temporaryFilesystem.probePath},
                {"temporaryFilesystemAvailableBytes", storagePreflight.temporaryFilesystem.availableBytes},
                {"destinationFilesystemRoot", storagePreflight.destinationFilesystem.rootPath},
                {"destinationFilesystemInspectedPath", storagePreflight.destinationFilesystem.inspectedPath},
                {"destinationFilesystemProbePath", storagePreflight.destinationFilesystem.probePath},
                {"destinationFilesystemAvailableBytes", storagePreflight.destinationFilesystem.availableBytes}});
        if (!storagePreflight.sufficient) { result.error = storagePreflight.error; return result; }
        if (!updateManifestState(settings, QStringLiteral("stageA"))) {
            result.error = QStringLiteral("Could not update export ownership manifest before Stage A.");
            return result;
        }
        const FilesystemSnapshot temporaryFilesystemAtStart = filesystemSnapshot(temporaryOverlayPath);
        const FilesystemSnapshot destinationFilesystemAtStart = filesystemSnapshot(settings.outputPath);
        const auto cleanupTemporaryOverlay = qScopeGuard([&] {
            const QFileInfo temporaryInfo(temporaryOverlayPath);
            observe(settings, QStringLiteral("status"), QStringLiteral("cleaningUp"),
                    QStringLiteral("removeTemporaryOverlay"),
                    QStringLiteral("Cleaning temporary overlay"));
            observe(settings, QStringLiteral("log"), QStringLiteral("cleaningUp"),
                    QStringLiteral("removeTemporaryOverlay"),
                    QStringLiteral("Removing temporary overlay"), QStringLiteral("cleanup"),
                    {{"path", temporaryOverlayPath}, {"bytes", temporaryInfo.size()}});
            // The controller's manifest janitor is the only authority allowed
            // to remove worker-owned artifacts. Legacy direct callers retain
            // the old local temporary-file cleanup behavior.
            const bool removed = !temporaryInfo.exists()
                || (settings.manifestPath.isEmpty() && QFile::remove(temporaryOverlayPath));
            observe(settings, QStringLiteral("log"), QStringLiteral("cleaningUp"),
                    QStringLiteral("removeTemporaryOverlay"),
                    removed ? QStringLiteral("Temporary overlay deleted")
                            : QStringLiteral("Temporary overlay deletion failed"),
                    QStringLiteral("cleanup"), {{"result", removed ? "deleted" : "failed"}});
        });
        QProcess ffmpeg;
        // Keep FFmpeg in the worker's group, so a forced GUI-worker shutdown
        // reaches it as well as any inherited descendants.
        ExportProcessSupervisor ffmpegSupervisor(ffmpeg, false);
        QProcess *activeFfmpeg = &ffmpeg;
        ExportProcessSupervisor *activeSupervisor = &ffmpegSupervisor;
        const QString size = QStringLiteral("%1x%2").arg(outputSize.width()).arg(outputSize.height());
        const QStringList overlayArguments = {
            "-hide_banner", "-loglevel", "error", "-nostats", "-progress", "pipe:1", "-y",
            "-f", "rawvideo", "-pixel_format", "rgba", "-video_size", size,
            "-framerate", rateString(exportFrameRate), "-i", "pipe:0", "-an",
            "-c:v", "ffv1", "-pix_fmt", "bgra", "-f", "matroska", temporaryOverlayPath,
        };
        observe(settings, QStringLiteral("status"), QStringLiteral("renderingOverlay"),
                QStringLiteral("encodeTemporaryOverlay"),
                QStringLiteral("Encoding FFV1 temporary overlay"));
        observe(settings, QStringLiteral("log"), QStringLiteral("renderingOverlay"),
                QStringLiteral("encodeTemporaryOverlay"), QStringLiteral("Stage A started"),
                QStringLiteral("ffmpeg"),
                {{"executable", FfmpegTools::ffmpegPath()}, {"arguments", overlayArguments},
                 {"temporaryOverlayPath", temporaryOverlayPath}, {"temporaryOverlayAutoRemove", false},
                 {"temporaryFilesystemRoot", temporaryFilesystemAtStart.root},
                 {"temporaryFilesystemAvailableBytes", temporaryFilesystemAtStart.availableBytes},
                 {"temporaryFilesystemTotalBytes", temporaryFilesystemAtStart.totalBytes},
                 {"destinationFilesystemRoot", destinationFilesystemAtStart.root},
                 {"destinationFilesystemAvailableBytes", destinationFilesystemAtStart.availableBytes},
                 {"destinationFilesystemTotalBytes", destinationFilesystemAtStart.totalBytes}});
        ffmpeg.setProcessChannelMode(QProcess::SeparateChannels);
        qInfo().noquote() << QStringLiteral("Stage A FFmpeg arguments: %1").arg(formatArgumentList(overlayArguments));
        ffmpegSupervisor.start(FfmpegTools::ffmpegPath(), overlayArguments);
        if (!ffmpegSupervisor.waitForStarted()) {
            result.error = QStringLiteral("Could not start FFmpeg: %1").arg(ffmpeg.errorString());
            return result;
        }
        if (isCancelled(settings)) {
            result.cancelled = true;
            static_cast<void>(ffmpegSupervisor.stopAndWait());
            return result;
        }
        FfmpegProgressParser progressParser;
        FfmpegProgress lastFfmpegProgress;
        QString standardErrorText;
        QElapsedTimer activityTimer;
        activityTimer.start();
        QElapsedTimer storageMonitorTimer;
        storageMonitorTimer.start();
        bool finalizing = false;
        bool compositing = false;
        int lastOverlayMilestone = -5;
        int lastCompositionMilestone = -5;
        QString stageAStdinCloseReason;
        const auto refreshTemporaryOverlaySize = [&] {
            result.temporaryOverlayBytes = QFileInfo(temporaryOverlayPath).size();
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
            standardErrorText += QString::fromUtf8(stderrData);
            const QList<FfmpegProgress> updates = progressParser.append(stdoutData);
            for (const FfmpegProgress &update : updates) {
                lastFfmpegProgress = update;
                if (update.complete || update.outputMicroseconds >= static_cast<qint64>(exportDuration * 995'000.0)) {
                    finalizing = true;
                }
                reportProgress();
            }
        };
        const auto recordUnexpectedStageAExit = [&](const QString &reason,
                                                    const QString &processErrorOverride = {},
                                                    const QString &processErrorStringOverride = {}) {
            // QProcess may transition to NotRunning before its final readyRead
            // signals are dispatched. Drain both channels before any cleanup or
            // diagnostic snapshot so FFmpeg's actual failure is not lost.
            pumpFfmpeg();
            refreshTemporaryOverlaySize();
            const FilesystemSnapshot temporaryFilesystemNow = filesystemSnapshot(temporaryOverlayPath);
            const FilesystemSnapshot destinationFilesystemNow = filesystemSnapshot(settings.outputPath);
            const qint64 queuedBytes = static_cast<qint64>(ffmpeg.bytesToWrite());
            result.maximumQueuedBytes = qMax(result.maximumQueuedBytes, queuedBytes);
            const StageAFailureDiagnostics diagnostics{
                reason,
                result.renderedFrames,
                expectedFrames,
                ffmpeg.exitCode(),
                processExitStatusName(ffmpeg.exitStatus()),
                processErrorOverride.isEmpty() ? processErrorName(ffmpeg.error()) : processErrorOverride,
                processErrorStringOverride.isEmpty() ? ffmpeg.errorString() : processErrorStringOverride,
                lastFfmpegProgress.encodedFrames,
                lastFfmpegProgress.outputMicroseconds,
                lastFfmpegProgress.encoderFps,
                lastFfmpegProgress.realtimeFactor,
                queuedBytes,
                result.maximumQueuedBytes,
                temporaryOverlayPath,
                result.temporaryOverlayBytes,
                standardErrorText.right(16 * 1024),
                temporaryFilesystemNow.root,
                temporaryFilesystemNow.availableBytes,
                temporaryFilesystemNow.totalBytes,
                destinationFilesystemNow.root,
                destinationFilesystemNow.availableBytes,
                destinationFilesystemNow.totalBytes,
                settings.cancellationFilePath,
                isCancelled(settings)};
            result.diagnostics = formatStageAFailureDiagnostics(diagnostics);
            observe(settings, QStringLiteral("log"), QStringLiteral("renderingOverlay"),
                    QStringLiteral("encodeTemporaryOverlay"),
                    QStringLiteral("Stage A exited unexpectedly"), QStringLiteral("ffmpeg"),
                    stageAFailureDiagnosticDetails(diagnostics));
        };
        const auto cancelFfmpeg = [&] {
            result.cancelled = true;
            if (!compositing && stageAStdinCloseReason.isEmpty()) {
                stageAStdinCloseReason = QStringLiteral("export cancellation after %1 of %2 frames submitted")
                                            .arg(result.renderedFrames).arg(expectedFrames);
                activeFfmpeg->closeWriteChannel();
            }
            static_cast<void>(activeSupervisor->stopAndWait());
        };
        const auto checkTemporarySpace = [&] {
            if (storageMonitorTimer.elapsed() < 1'000) return true;
            storageMonitorTimer.restart();
            const ExportFilesystemInfo filesystem = ExportStoragePolicy::filesystemForPath(temporaryOverlayPath);
            observe(settings, QStringLiteral("log"), compositing ? QStringLiteral("encodingVideo")
                : QStringLiteral("renderingOverlay"), QStringLiteral("monitorStorage"),
                QStringLiteral("Temporary volume free-space sample"), QStringLiteral("storage"),
                {{"temporaryFilesystemRoot", filesystem.rootPath},
                 {"temporaryFilesystemAvailableBytes", filesystem.availableBytes}});
            if (!ExportStoragePolicy::criticallyLow(filesystem, storageEstimate.safetyReserveBytes)) return true;
            result.error = QStringLiteral("Not enough free space for export. Temporary filesystem %1 fell below the %2 safety reserve.")
                .arg(filesystem.rootPath, ExportStoragePolicy::bytesText(storageEstimate.safetyReserveBytes));
            static_cast<void>(activeSupervisor->stopAndWait());
            return false;
        };
        const RawFrameTransportConfig transportConfig;
        RawFrameTransport transport(
            ffmpeg, transportConfig, [&settings] { return isCancelled(settings); },
            [&] { pumpFfmpeg(); reportProgress(); },
            [&result](const qint64, const qint64 maximumQueuedBytes) {
                result.maximumQueuedBytes = qMax(result.maximumQueuedBytes, maximumQueuedBytes);
            });
        observe(settings, QStringLiteral("log"), QStringLiteral("renderingOverlay"),
                QStringLiteral("configureRawFrameTransport"),
                QStringLiteral("Bounded raw-frame transport configured"), QStringLiteral("transport"),
                {{"highWaterBytes", transportConfig.highWaterBytes},
                 {"lowWaterBytes", transportConfig.lowWaterBytes},
                 {"writeChunkBytes", static_cast<qint64>(transportConfig.writeChunkBytes)},
                 {"stallTimeoutMilliseconds", transportConfig.stallTimeoutMilliseconds}});
        for (qsizetype frameIndex = 0; frameIndex < expectedFrames; ++frameIndex) {
            if (isCancelled(settings)) {
                cancelFfmpeg();
                return result;
            }
            pumpFfmpeg();
            if (!checkTemporarySpace()) return result;
            if (ffmpeg.state() == QProcess::NotRunning) {
                result.error = QStringLiteral("Temporary overlay encoder exited early: %1 of %2 frames submitted.")
                                   .arg(result.renderedFrames).arg(expectedFrames);
                recordUnexpectedStageAExit(QStringLiteral("FFmpeg stopped before the next overlay frame was rendered"));
                return result;
            }
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
                static_cast<void>(ffmpegSupervisor.stopAndWait());
                return result;
            }
            ++result.generatedFrames;
            QElapsedTimer copyTimer;
            copyTimer.start();
            const QByteArray bytes = rgbaBytes(image, outputSize);
            result.cpuCopyNanoseconds += copyTimer.nsecsElapsed();
            QElapsedTimer writeTimer;
            writeTimer.start();
            const RawFrameTransportResult writeResult = transport.writeFrame(bytes);
            if (writeResult.status == RawFrameTransportResult::Status::Cancelled) {
                cancelFfmpeg();
                return result;
            }
            if (!writeResult.succeeded()) {
                result.error = QStringLiteral("Could not stream overlay frame to FFmpeg: %1")
                                   .arg(writeResult.error);
                // Capture the original QProcess error before intentionally
                // stopping a still-running child, then drain its final stderr.
                const QString writeError = processErrorName(ffmpeg.error());
                const QString writeErrorString = ffmpeg.errorString();
                if (ffmpeg.state() != QProcess::NotRunning) {
                    static_cast<void>(ffmpegSupervisor.stopAndWait());
                }
                recordUnexpectedStageAExit(
                    writeResult.error,
                    writeError, writeErrorString);
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
            if (!checkTemporarySpace()) return result;
            if (activityTimer.elapsed() > 300'000) {
                result.error = QStringLiteral("FFmpeg encoder appears stalled (no progress for five minutes).");
                static_cast<void>(ffmpegSupervisor.stopAndWait());
                pumpFfmpeg();
                result.diagnostics = formatDiagnostics(
                    ffmpeg, lastFfmpegProgress, standardErrorText, QStringLiteral("Stage A overlay"), overlayArguments,
                    temporaryOverlayPath, result.temporaryOverlayBytes, stageAStdinCloseReason);
                return result;
            }
        }
        pumpFfmpeg();
        if (ffmpeg.exitStatus() != QProcess::NormalExit || ffmpeg.exitCode() != 0) {
            result.error = QStringLiteral("Temporary overlay encoder failed.");
            recordUnexpectedStageAExit(QStringLiteral("FFmpeg returned a nonzero or abnormal exit after Stage A stdin closed"));
            return result;
        }
        observe(settings, QStringLiteral("log"), QStringLiteral("renderingOverlay"),
                QStringLiteral("flushTemporaryOverlay"),
                QStringLiteral("Stage A FFmpeg exited · code %1").arg(ffmpeg.exitCode()),
                QStringLiteral("ffmpeg"), {{"exitCode", ffmpeg.exitCode()},
                                            {"stderr", standardErrorText.right(16 * 1024)}});
        if (!QFileInfo(temporaryOverlayPath).isFile()
            || QFileInfo(temporaryOverlayPath).size() <= 0) {
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
        qsizetype stagedEncodedFrames = lastFfmpegProgress.encodedFrames;
        QString stagedFrameCountSource = QStringLiteral("FFmpeg final progress");
        if (!lastFfmpegProgress.complete) {
            observe(settings, QStringLiteral("status"), QStringLiteral("validatingOverlay"),
                    QStringLiteral("countTemporaryOverlayPackets"),
                    QStringLiteral("FFmpeg final progress was incomplete; counting temporary overlay packets"));
            observe(settings, QStringLiteral("log"), QStringLiteral("validatingOverlay"),
                    QStringLiteral("countTemporaryOverlayPackets"),
                    QStringLiteral("Temporary overlay packet-count fallback started"),
                    QStringLiteral("validation"));
            const MediaInfo packetCountInfo = MediaProbe::probe(
                temporaryOverlayPath, {}, false, 30'000,
                probeObservations(settings, QStringLiteral("validatingOverlay"),
                                  QStringLiteral("countTemporaryOverlayPackets")),
                [&settings] { return isCancelled(settings); }, true);
            stagedEncodedFrames = packetCountInfo.videoPacketCount;
            stagedFrameCountSource = QStringLiteral("FFmpeg packet-count fallback");
        }
        if (stagedEncodedFrames != expectedFrames) {
            result.error = QStringLiteral("FFmpeg did not report every expected temporary overlay frame.");
            result.diagnostics = QStringLiteral(
                "Generated %1, submitted %2, FFmpeg encoded %3, expected %4 temporary overlay frames.")
                                     .arg(result.generatedFrames).arg(result.renderedFrames)
                                     .arg(stagedEncodedFrames).arg(expectedFrames);
            return result;
        }
        if (settings.stateCallback) {
            settings.stateCallback(QStringLiteral("validating"));
        }
        if (!updateManifestState(settings, QStringLiteral("validating"))) {
            result.error = QStringLiteral("Could not update export ownership manifest before validation.");
            return result;
        }
        observe(settings, QStringLiteral("status"), QStringLiteral("validatingOverlay"),
                QStringLiteral("probeTemporaryOverlayMetadata"),
                QStringLiteral("Reading temporary overlay metadata with ffprobe"));
        observe(settings, QStringLiteral("log"), QStringLiteral("validatingOverlay"),
                QStringLiteral("probeTemporaryOverlayMetadata"),
                QStringLiteral("Temporary overlay metadata validation started"), QStringLiteral("validation"),
                {{"frameCountSource", QStringLiteral("producer + %1").arg(stagedFrameCountSource)},
                 {"generatedFrames", static_cast<qint64>(result.generatedFrames)},
                 {"submittedFrames", static_cast<qint64>(result.renderedFrames)},
                 {"encodedFrames", static_cast<qint64>(stagedEncodedFrames)},
                 {"expectedFrames", static_cast<qint64>(expectedFrames)}});
        QElapsedTimer temporaryValidationTimer;
        temporaryValidationTimer.start();
        const MediaInfo temporaryOverlayInfo = MediaProbe::probeSummary(
            temporaryOverlayPath, {}, 30'000,
            probeObservations(settings, QStringLiteral("validatingOverlay"),
                              QStringLiteral("probeTemporaryOverlayMetadata")),
            [&settings] { return isCancelled(settings); });
        const TemporaryOverlayValidationResult temporaryValidation =
            TemporaryOverlayValidation::validate(
                temporaryOverlayInfo, outputSize, exportFrameRate, expectedFrames, exportDuration);
        const double frameInterval = 1.0 / exportFrameRate.value();
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
                      temporaryOverlayInfo.videoCodec, temporaryValidation.codecOk);
        validationLog(QStringLiteral("Temporary resolution"),
                      QStringLiteral("%1x%2").arg(outputSize.width()).arg(outputSize.height()),
                      QStringLiteral("%1x%2").arg(temporaryOverlayInfo.videoSize.width())
                                               .arg(temporaryOverlayInfo.videoSize.height()), temporaryValidation.dimensionsOk);
        validationLog(QStringLiteral("Temporary nominal rate"), rateString(exportFrameRate),
                      rateString(temporaryOverlayInfo.frameRate), temporaryValidation.nominalRateOk);
        validationLog(QStringLiteral("Temporary observed average rate"), rateString(exportFrameRate),
                      rateString(temporaryOverlayInfo.averageFrameRate), temporaryValidation.averageRateOk);
        validationLog(QStringLiteral("Temporary encoded frames"), expectedFrames,
                      stagedEncodedFrames, true);
        validationLog(QStringLiteral("Temporary duration"), exportDuration,
                      temporaryOverlayInfo.duration, temporaryValidation.durationOk);
        validationLog(QStringLiteral("Temporary video start"), QStringLiteral("0"),
                      temporaryOverlayInfo.videoStartTime, temporaryValidation.startTimeOk);
        observe(settings, QStringLiteral("log"), QStringLiteral("validatingOverlay"),
                QStringLiteral("probeTemporaryOverlayMetadata"),
                QStringLiteral("Temporary overlay metadata validation elapsed %1 ms")
                    .arg(temporaryValidationTimer.elapsed()), QStringLiteral("validation"),
                {{"validationElapsedMilliseconds", temporaryValidationTimer.elapsed()},
                 {"frameCountSource", QStringLiteral("producer + %1").arg(stagedFrameCountSource)},
                 {"scheduledRate", rateString(exportFrameRate)},
                 {"reportedNominalRate", rateString(temporaryOverlayInfo.frameRate)},
                 {"reportedAverageRate", rateString(temporaryOverlayInfo.averageFrameRate)},
                 {"timeBase", rateString(temporaryOverlayInfo.timeBase)},
                 {"timestampToleranceMicroseconds", temporaryValidation.timestampToleranceMicroseconds},
                 {"rateTolerance", temporaryValidation.rateTolerance}});
        if (!temporaryValidation.passed()) {
            result.error = QStringLiteral("Temporary telemetry overlay failed validation.");
            result.diagnostics = QStringLiteral(
                "Expected FFV1 %1x%2 at scheduled %3 fps, %4 frames, %5 s; staged %6 %7x%8 with nominal %9 fps, average %10 fps, "
                "time base %11, duration %12 s, %13 bytes. Timestamp tolerance=%14 us.\n"
                "FFmpeg arguments: %15")
                                     .arg(outputSize.width()).arg(outputSize.height())
                                     .arg(exportFrameRate.value(), 0, 'f', 6).arg(expectedFrames)
                                     .arg(exportDuration, 0, 'f', 6)
                                     .arg(temporaryOverlayInfo.videoCodec)
                                     .arg(temporaryOverlayInfo.videoSize.width())
                                     .arg(temporaryOverlayInfo.videoSize.height())
                                     .arg(temporaryOverlayInfo.frameRate.value(), 0, 'f', 6)
                                     .arg(temporaryOverlayInfo.averageFrameRate.value(), 0, 'f', 6)
                                     .arg(rateString(temporaryOverlayInfo.timeBase))
                                     .arg(temporaryOverlayInfo.duration, 0, 'f', 6)
                                     .arg(result.temporaryOverlayBytes)
                                     .arg(temporaryValidation.timestampToleranceMicroseconds)
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
        ExportProcessSupervisor compositorSupervisor(compositor, false);
        activeFfmpeg = &compositor;
        activeSupervisor = &compositorSupervisor;
        progressParser = FfmpegProgressParser{};
        lastFfmpegProgress = {};
        standardErrorText.clear();
        activityTimer.restart();
        compositing = true;
        finalizing = false;
        const QString timeRangeFilter = QStringLiteral(
            "[0:v]trim=start=%1:end=%2,setpts=PTS-STARTPTS%5,"
            "fps=fps=%3:start_time=0:round=near:eof_action=round,"
            "trim=end_frame=%4,setpts=PTS-STARTPTS[sourceVideo];"
            "[1:v]setpts=PTS-STARTPTS[temporaryOverlay];"
            "[sourceVideo][temporaryOverlay]overlay=0:0:shortest=1:repeatlast=0:eof_action=endall:format=auto[video]")
                                            .arg(sourceRangeStart, 0, 'f', 9)
                                            .arg(sourceRangeEnd, 0, 'f', 9)
                                            .arg(rateString(exportFrameRate))
                                            .arg(expectedFrames)
                                            .arg(source.videoSize == outputSize ? QString() : QStringLiteral(",scale=%1:%2:flags=lanczos")
                                                .arg(outputSize.width()).arg(outputSize.height()));
        QStringList compositionArguments = {
            "-hide_banner", "-loglevel", "error", "-nostats", "-progress", "pipe:1", "-y",
            "-i", settings.inputPath, "-i", temporaryOverlayPath,
            "-filter_complex", timeRangeFilter,
            "-map", "[video]", "-fps_mode:v", "cfr", "-c:v", encoder,
            "-b:v", QString::number(settings.videoBitrate),
            "-tag:v", "hvc1", "-pix_fmt", "yuv420p",
        };
        if (settings.audioEnabled && !source.audioCodecs.isEmpty()) {
            compositionArguments[compositionArguments.indexOf("-filter_complex") + 1] += QStringLiteral(
                ";[0:a]atrim=start=%1:end=%2,asetpts=PTS-STARTPTS[audio]")
                .arg(sourceRangeStart, 0, 'f', 9).arg(sourceRangeEnd, 0, 'f', 9);
            compositionArguments.append({"-map", "[audio]", "-c:a", "aac", "-b:a", QString::number(settings.audioBitrate)});
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
        if (!updateManifestState(settings, QStringLiteral("stageB"))) {
            result.error = QStringLiteral("Could not update export ownership manifest before Stage B.");
            return result;
        }
        compositorSupervisor.start(FfmpegTools::ffmpegPath(), compositionArguments);
        if (!compositorSupervisor.waitForStarted()) {
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
            if (!checkTemporarySpace()) return result;
            if (lastFfmpegProgress.encodedFrames > result.renderedFrames) {
                result.error = QStringLiteral("FFmpeg output advanced beyond available telemetry overlay frames.");
                static_cast<void>(compositorSupervisor.stopAndWait());
                pumpFfmpeg();
                result.diagnostics = formatDiagnostics(
                    compositor, lastFfmpegProgress, standardErrorText, QStringLiteral("Stage B composition"),
                    compositionArguments, temporaryOverlayPath, result.temporaryOverlayBytes,
                    stageAStdinCloseReason);
                return result;
            }
            if (activityTimer.elapsed() > 300'000) {
                result.error = QStringLiteral("FFmpeg encoder appears stalled (no progress for five minutes).");
                static_cast<void>(compositorSupervisor.stopAndWait());
                pumpFfmpeg();
                result.diagnostics = formatDiagnostics(
                    compositor, lastFfmpegProgress, standardErrorText, QStringLiteral("Stage B composition"),
                    compositionArguments, temporaryOverlayPath, result.temporaryOverlayBytes,
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
                compositor, lastFfmpegProgress, standardErrorText, QStringLiteral("Stage B composition"),
                compositionArguments, temporaryOverlayPath, result.temporaryOverlayBytes,
                stageAStdinCloseReason);
            return result;
        }
        observe(settings, QStringLiteral("log"), QStringLiteral("encodingVideo"),
                QStringLiteral("flushOutputContainer"),
                QStringLiteral("Stage B FFmpeg exited · code %1").arg(compositor.exitCode()),
                QStringLiteral("ffmpeg"), {{"exitCode", compositor.exitCode()},
                                            {"stderr", standardErrorText.right(16 * 1024)}});
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
                                  QStringLiteral("probeFinalOutput")),
                [&settings] { return isCancelled(settings); }, true);
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
