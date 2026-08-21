#include "export/MediaProbe.h"

#include "export/FfmpegTools.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <stdexcept>

namespace FlappedEar {

double MediaRational::value() const
{
    return denominator == 0 ? 0.0 : static_cast<double>(numerator) / denominator;
}

bool MediaRational::isValid() const { return numerator > 0 && denominator > 0; }

MediaInfo MediaProbe::probe(
    const QString &path, const QString &requestedFfprobePath, const bool countVideoFrames)
{
    const QString executable = requestedFfprobePath.isEmpty()
        ? FfmpegTools::ffprobePath()
        : requestedFfprobePath;
    if (executable.isEmpty()) {
        throw std::runtime_error(FfmpegTools::missingToolsMessage(false).toStdString());
    }
    QProcess process;
    QStringList arguments{"-v", "error", "-print_format", "json"};
    if (countVideoFrames) {
        arguments.append("-count_frames");
    }
    arguments.append({"-show_format", "-show_streams", path});
    process.start(executable, arguments);
    if (!process.waitForStarted()) {
        throw std::runtime_error("Could not start ffprobe.");
    }
    if (!process.waitForFinished(30'000) || process.exitStatus() != QProcess::NormalExit
        || process.exitCode() != 0) {
        throw std::runtime_error(
            QStringLiteral("ffprobe failed: %1").arg(QString::fromUtf8(process.readAllStandardError())).toStdString());
    }
    return parseJson(process.readAllStandardOutput(), path);
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
