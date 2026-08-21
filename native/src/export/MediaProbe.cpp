#include "export/MediaProbe.h"

#include "export/FfmpegTools.h"
#include "export/ExportDiagnostics.h"

#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <stdexcept>

namespace FlappedEar {

namespace {

constexpr int metadataProbeTimeoutMilliseconds = 30'000;
constexpr int frameCountProbeTimeoutMilliseconds = 300'000;
constexpr int processShutdownTimeoutMilliseconds = 1'000;

QString stderrDiagnostic(const QByteArray &stderrOutput)
{
    const QString text = QString::fromUtf8(stderrOutput).trimmed();
    return text.isEmpty() ? QStringLiteral("<no stderr output>") : text;
}

QString secondsText(const qint64 milliseconds)
{
    return QString::number(milliseconds / 1'000.0, 'f', milliseconds >= 1'000 ? 1 : 3);
}

MediaInfo runProbe(
    const QString &path,
    const QString &requestedFfprobePath,
    const QStringList &arguments,
    const int timeoutMilliseconds,
    const QString &mode,
    const MediaProbeProgressCallback &progressCallback,
    const MediaProbeCancellationCallback &cancellationCallback)
{
    const QString executable = requestedFfprobePath.isEmpty()
        ? FfmpegTools::ffprobePath()
        : requestedFfprobePath;
    if (executable.isEmpty()) {
        throw std::runtime_error(FfmpegTools::missingToolsMessage(false).toStdString());
    }

    QProcess process;
    process.setProcessChannelMode(QProcess::SeparateChannels);
    QElapsedTimer elapsed;
    elapsed.start();
    process.start(executable, arguments);
    if (!process.waitForStarted()) {
        throw std::runtime_error(
            QStringLiteral("Could not start ffprobe while probing: %1 (%2)")
                .arg(path, process.errorString()).toStdString());
    }
    if (progressCallback) {
        progressCallback({MediaProbeEvent::Phase::Started, 0, executable, arguments, path, mode, 0});
    }
    DiagnosticHeartbeat heartbeat(500);
    bool finished = false;
    while (!finished && elapsed.elapsed() < timeoutMilliseconds) {
        if (cancellationCallback && cancellationCallback()) {
            process.terminate();
            if (!process.waitForFinished(processShutdownTimeoutMilliseconds)) {
                process.kill();
                process.waitForFinished();
            }
            throw std::runtime_error(
                QStringLiteral("ffprobe cancelled while probing: %1").arg(path).toStdString());
        }
        const qint64 remaining = timeoutMilliseconds - elapsed.elapsed();
        finished = process.waitForFinished(static_cast<int>(qMin<qint64>(250, remaining)));
        if (!finished && progressCallback && heartbeat.shouldEmit(elapsed.elapsed())) {
            progressCallback({MediaProbeEvent::Phase::Heartbeat, elapsed.elapsed(), executable,
                              arguments, path, mode, 0});
        }
    }
    if (!finished) {
        process.terminate();
        if (!process.waitForFinished(processShutdownTimeoutMilliseconds)) {
            process.kill();
            process.waitForFinished();
        }
        const QByteArray stdoutOutput = process.readAllStandardOutput();
        const QByteArray stderrOutput = process.readAllStandardError();
        throw std::runtime_error(
            QStringLiteral("ffprobe timed out after %1 seconds while probing: %2 "
                           "(elapsed %3 seconds, stdout %4 bytes). stderr: %5")
                .arg(secondsText(timeoutMilliseconds), path, secondsText(elapsed.elapsed()),
                     QString::number(stdoutOutput.size()), stderrDiagnostic(stderrOutput)).toStdString());
    }

    const QByteArray stdoutOutput = process.readAllStandardOutput();
    const QByteArray stderrOutput = process.readAllStandardError();
    if (progressCallback) {
        progressCallback({MediaProbeEvent::Phase::Finished, elapsed.elapsed(), executable,
                          arguments, path, mode, process.exitCode()});
    }
    if (process.exitStatus() != QProcess::NormalExit) {
        throw std::runtime_error(
            QStringLiteral("ffprobe crashed while probing: %1. stderr: %2")
                .arg(path, stderrDiagnostic(stderrOutput)).toStdString());
    }
    if (process.exitCode() != 0) {
        throw std::runtime_error(
            QStringLiteral("ffprobe exited with code %1 while probing: %2. stderr: %3")
                .arg(process.exitCode()).arg(path, stderrDiagnostic(stderrOutput)).toStdString());
    }
    return MediaProbe::parseJson(stdoutOutput, path);
}

} // namespace

double MediaRational::value() const
{
    return denominator == 0 ? 0.0 : static_cast<double>(numerator) / denominator;
}

bool MediaRational::isValid() const { return numerator > 0 && denominator > 0; }

MediaInfo MediaProbe::probe(
    const QString &path,
    const QString &requestedFfprobePath,
    const bool countVideoFrames,
    const int timeoutMilliseconds,
    const MediaProbeProgressCallback &progressCallback,
    const MediaProbeCancellationCallback &cancellationCallback)
{
    QStringList arguments{"-v", "error", "-print_format", "json"};
    if (countVideoFrames) {
        arguments.append("-count_frames");
    }
    arguments.append({"-show_format", "-show_streams", path});
    const int effectiveTimeout = timeoutMilliseconds >= 0
        ? timeoutMilliseconds
        : (countVideoFrames ? frameCountProbeTimeoutMilliseconds : metadataProbeTimeoutMilliseconds);
    return runProbe(path, requestedFfprobePath, arguments, effectiveTimeout,
                    countVideoFrames ? QStringLiteral("frameCount") : QStringLiteral("full"),
                    progressCallback, cancellationCallback);
}

MediaInfo MediaProbe::probeSummary(
    const QString &path,
    const QString &requestedFfprobePath,
    const int timeoutMilliseconds,
    const MediaProbeProgressCallback &progressCallback,
    const MediaProbeCancellationCallback &cancellationCallback)
{
    const QStringList arguments{
        "-v", "error",
        "-show_entries",
        "format=duration:stream=index,codec_type,codec_name,width,height,r_frame_rate,avg_frame_rate",
        "-of", "json",
        path,
    };
    return runProbe(path, requestedFfprobePath, arguments, timeoutMilliseconds,
                    QStringLiteral("summary"), progressCallback, cancellationCallback);
}

MediaInfo MediaProbe::parseJson(const QByteArray &json, const QString &path)
{
    const QJsonDocument document = QJsonDocument::fromJson(json);
    if (!document.isObject()) {
        throw std::runtime_error("ffprobe returned invalid JSON.");
    }
    MediaInfo info;
    info.path = path;
    const QJsonObject format = document.object().value("format").toObject();
    info.duration = format.value("duration").toString().toDouble();
    info.startTime = format.value("start_time").toString().toDouble();
    for (const QJsonValue &value : document.object().value("streams").toArray()) {
        const QJsonObject stream = value.toObject();
        if (stream.value("codec_type").toString() == "video" && info.videoCodec.isEmpty()) {
            info.videoSize = {stream.value("width").toInt(), stream.value("height").toInt()};
            info.frameRate = parseRational(stream.value("r_frame_rate").toString());
            info.averageFrameRate = parseRational(stream.value("avg_frame_rate").toString());
            info.timeBase = parseRational(stream.value("time_base").toString());
            info.videoCodec = stream.value("codec_name").toString();
            info.pixelFormat = stream.value("pix_fmt").toString();
            bool frameCountOk = false;
            const qint64 frameCount = stream.value("nb_read_frames").toString().toLongLong(&frameCountOk);
            if (frameCountOk && frameCount >= 0) {
                info.videoFrameCount = static_cast<qsizetype>(frameCount);
            }
            if (stream.contains("start_time")) {
                info.startTime = stream.value("start_time").toString().toDouble();
            }
        } else if (stream.value("codec_type").toString() == "audio") {
            info.audioCodecs.append(stream.value("codec_name").toString());
        }
    }
    if (info.videoSize.isEmpty() || info.videoCodec.isEmpty() || info.duration <= 0.0) {
        throw std::runtime_error("ffprobe did not find a usable video stream.");
    }
    // r_frame_rate is the stream's nominal rate, while avg_frame_rate exposes
    // actual presentation cadence. A meaningful mismatch is a conservative
    // VFR warning; export remains explicit-CFR in this milestone.
    if (info.frameRate.isValid() && info.averageFrameRate.isValid()) {
        const double nominal = info.frameRate.value();
        const double average = info.averageFrameRate.value();
        info.likelyVariableFrameRate = qAbs(nominal - average) > 0.01 * qMax(1.0, nominal);
    }
    return info;
}

MediaRational MediaProbe::parseRational(const QString &value)
{
    const QStringList pieces = value.split('/');
    if (pieces.size() != 2) {
        return {};
    }
    bool numeratorOk = false;
    bool denominatorOk = false;
    const qint64 numerator = pieces[0].toLongLong(&numeratorOk);
    const qint64 denominator = pieces[1].toLongLong(&denominatorOk);
    return numeratorOk && denominatorOk && denominator > 0 ? MediaRational{numerator, denominator}
                                                            : MediaRational{};
}

} // namespace FlappedEar
