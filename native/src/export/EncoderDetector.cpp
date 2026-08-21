#include "export/EncoderDetector.h"

#include "export/FfmpegTools.h"
#include "export/ExportProcessSupervisor.h"

#include <QElapsedTimer>
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

bool waitForFinished(QProcess &process, ExportProcessSupervisor &supervisor,
                     const std::function<bool()> &cancelled, const int timeoutMilliseconds)
{
    QElapsedTimer elapsed;
    elapsed.start();
    while (process.state() != QProcess::NotRunning && elapsed.elapsed() < timeoutMilliseconds) {
        if (cancelled && cancelled()) {
            static_cast<void>(supervisor.stopAndWait());
            throw std::runtime_error("Encoder discovery cancelled.");
        }
        process.waitForFinished(100);
    }
    return process.state() == QProcess::NotRunning;
}

bool canEncodeHevc(const QString &executable, const QString &encoder,
                   const std::function<bool()> &cancelled)
{
    // `ffmpeg -encoders` reports compiled-in encoders. Hardware entries can
    // still be unusable because a driver, device, or operating-system service
    // is unavailable, so verify the selected binary with a tiny in-memory job.
    QProcess process;
    ExportProcessSupervisor supervisor(process, false);
    supervisor.start(
        executable,
        {"-hide_banner", "-loglevel", "error", "-f", "lavfi", "-i",
         "color=c=black:s=64x64:r=30", "-frames:v", "1", "-c:v", encoder, "-f", "null", "-"});
    return supervisor.waitForStarted(5'000) && waitForFinished(process, supervisor, cancelled, 15'000)
        && process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0;
}

} // namespace

QList<EncoderCapability> EncoderDetector::discover(
    const QString &requestedFfmpegPath, const std::function<bool()> &cancelled)
{
    const QString executable = requestedFfmpegPath.isEmpty()
        ? FfmpegTools::ffmpegPath()
        : requestedFfmpegPath;
    if (executable.isEmpty()) {
        throw std::runtime_error(FfmpegTools::missingToolsMessage().toStdString());
    }
    QProcess process;
    ExportProcessSupervisor supervisor(process, false);
    supervisor.start(executable, {"-hide_banner", "-encoders"});
    if (!supervisor.waitForStarted(5'000) || !waitForFinished(process, supervisor, cancelled, 15'000)
        || process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        throw std::runtime_error("Could not query FFmpeg encoders.");
    }
    const QList<EncoderCapability> advertised = parseEncoders(
        QString::fromUtf8(process.readAllStandardOutput()));
    QList<EncoderCapability> usable;
    for (const EncoderCapability &capability : advertised) {
        if (canEncodeHevc(executable, capability.id, cancelled)) {
            usable.append(capability);
        }
    }
    return usable;
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
    // This order favours working platform hardware encoders, then a working
    // x265 build. discover() removes encoders merely advertised by FFmpeg.
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
