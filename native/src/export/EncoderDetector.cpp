#include "export/EncoderDetector.h"

#include "export/FfmpegTools.h"

#include <QProcess>
#include <QRegularExpression>
#include <stdexcept>

namespace FlappedEar {
namespace {

struct KnownEncoder {
    const char *id;
    const char *name;
    bool hardware;
};

constexpr KnownEncoder kKnownHevcEncoders[] = {
    {"hevc_videotoolbox", "Apple VideoToolbox HEVC", true},
    {"hevc_nvenc", "NVIDIA NVENC HEVC", true},
    {"hevc_qsv", "Intel Quick Sync HEVC", true},
    {"hevc_amf", "AMD AMF HEVC", true},
    {"libx265", "x265 HEVC", false},
};

} // namespace

QList<EncoderCapability> EncoderDetector::discover(const QString &requestedFfmpegPath)
{
    const QString executable = requestedFfmpegPath.isEmpty()
        ? FfmpegTools::ffmpegPath()
        : requestedFfmpegPath;
    if (executable.isEmpty()) {
        throw std::runtime_error(FfmpegTools::missingToolsMessage().toStdString());
    }
    QProcess process;
    process.start(executable, {"-hide_banner", "-encoders"});
    if (!process.waitForStarted() || !process.waitForFinished(15'000)
        || process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        throw std::runtime_error("Could not query FFmpeg encoders.");
    }
    return parseEncoders(QString::fromUtf8(process.readAllStandardOutput()));
}

QList<EncoderCapability> EncoderDetector::parseEncoders(const QString &output)
{
    QList<EncoderCapability> capabilities;
    for (const KnownEncoder &known : kKnownHevcEncoders) {
        const QRegularExpression expression(
            QStringLiteral("^\\s*V[^\\s]*\\s+%1(?:\\s|$)")
                .arg(QString::fromLatin1(known.id)),
            QRegularExpression::MultilineOption);
        if (expression.match(output).hasMatch()) {
            capabilities.append(
                {QString::fromLatin1(known.id), QString::fromLatin1(known.name), known.hardware});
        }
    }
    return capabilities;
}

QString EncoderDetector::preferredHevcEncoder(const QList<EncoderCapability> &encoders)
{
    // This order favours platform hardware encoders when the queried FFmpeg
    // actually exposes one, then falls back only to an installed x265 build.
    for (const KnownEncoder &known : kKnownHevcEncoders) {
        for (const EncoderCapability &capability : encoders) {
            if (capability.id == QLatin1String(known.id)) {
                return capability.id;
            }
        }
    }
    return {};
}

} // namespace FlappedEar
