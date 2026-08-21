#include "export/ExportEngine.h"

#include "export/EncoderDetector.h"
#include "export/FfmpegTools.h"
#include "export/TelemetryFrameRenderer.h"

#include <QFileInfo>
#include <QProcess>
#include <algorithm>
#include <cmath>

namespace FlappedEar {
namespace {

QByteArray rgbaBytes(const QImage &image, const QSize &size)
{
    const QImage rgba = image.convertToFormat(QImage::Format_RGBA8888);
    QByteArray bytes;
    bytes.reserve(size.width() * size.height() * 4);
    for (int row = 0; row < size.height(); ++row) {
        bytes.append(
            reinterpret_cast<const char *>(rgba.constScanLine(row)), size.width() * 4);
    }
    return bytes;
}

QString rateString(const MediaRational &rate)
{
    return QStringLiteral("%1/%2").arg(rate.numerator).arg(rate.denominator);
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

ExportResult ExportEngine::exportVideo(
    const ExportSettings &settings, TelemetryFrameRenderer &renderer)
{
    ExportResult result;
    try {
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
        QProcess ffmpeg;
        const QString duration = QString::number(end - start, 'f', 9);
        const QString size = QStringLiteral("%1x%2").arg(outputSize.width()).arg(outputSize.height());
        const QStringList arguments = {
            "-hide_banner", "-y", "-ss", QString::number(start, 'f', 9), "-i", settings.inputPath,
            "-f", "rawvideo", "-pixel_format", "rgba", "-video_size", size,
            "-framerate", rateString(frameRate), "-i", "pipe:0", "-t", duration,
            "-filter_complex", "[0:v][1:v]overlay=0:0:format=auto[v]", "-map", "[v]",
            "-c:v", encoder, "-b:v", "12M", "-tag:v", "hvc1", "-pix_fmt", "yuv420p", "-an",
            settings.outputPath,
        };
        ffmpeg.setProcessChannelMode(QProcess::SeparateChannels);
        ffmpeg.start(FfmpegTools::ffmpegPath(), arguments);
        if (!ffmpeg.waitForStarted()) {
            result.error = QStringLiteral("Could not start FFmpeg: %1").arg(ffmpeg.errorString());
            return result;
        }
        for (qsizetype frame = 0; frame < frames; ++frame) {
            const double presentationTime = start
                + static_cast<double>(frame) * static_cast<double>(frameRate.denominator)
                    / static_cast<double>(frameRate.numerator);
            const QImage image = renderer.renderFrame(presentationTime);
            if (image.size() != outputSize) {
                result.error = renderer.errorString().isEmpty()
                    ? QStringLiteral("Telemetry renderer returned an invalid frame.")
                    : renderer.errorString();
                ffmpeg.kill();
                ffmpeg.waitForFinished();
                return result;
            }
            const QByteArray bytes = rgbaBytes(image, outputSize);
            if (ffmpeg.write(bytes) != bytes.size() || !ffmpeg.waitForBytesWritten(30'000)) {
                result.error = QStringLiteral("Could not stream overlay frame to FFmpeg: %1")
                                   .arg(ffmpeg.errorString());
                ffmpeg.kill();
                ffmpeg.waitForFinished();
                return result;
            }
            ++result.renderedFrames;
        }
        ffmpeg.closeWriteChannel();
        if (!ffmpeg.waitForFinished(120'000) || ffmpeg.exitStatus() != QProcess::NormalExit
            || ffmpeg.exitCode() != 0) {
            result.error = QStringLiteral("FFmpeg export failed: %1")
                               .arg(QString::fromUtf8(ffmpeg.readAllStandardError()));
            return result;
        }
        if (!QFileInfo(settings.outputPath).isFile() || QFileInfo(settings.outputPath).size() <= 0) {
            result.error = QStringLiteral("FFmpeg did not create an output file.");
            return result;
        }
        result.mediaInfo = MediaProbe::probe(settings.outputPath);
        if (result.mediaInfo.videoCodec != "hevc" || result.mediaInfo.videoSize != outputSize
            || qAbs(result.mediaInfo.duration - (end - start))
                > 2.0 / qMax(1.0, frameRate.value())) {
            result.error = QStringLiteral("Export failed validation (codec, size, or duration).");
            return result;
        }
        result.success = true;
    } catch (const std::exception &error) {
        result.error = QString::fromUtf8(error.what());
    }
    return result;
}

} // namespace FlappedEar
