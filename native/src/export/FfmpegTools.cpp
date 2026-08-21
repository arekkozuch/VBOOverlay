#include "export/FfmpegTools.h"

#include <QFileInfo>
#include <QStandardPaths>

namespace FlappedEar {
namespace {

QString findTool(const QString &name)
{
    const QString fromPath = QStandardPaths::findExecutable(name);
    if (!fromPath.isEmpty()) {
        return fromPath;
    }
#ifdef Q_OS_MACOS
    for (const QString &directory : {QStringLiteral("/opt/homebrew/bin"),
                                     QStringLiteral("/usr/local/bin")}) {
        const QString candidate = directory + QLatin1Char('/') + name;
        if (QFileInfo(candidate).isExecutable()) {
            return candidate;
        }
    }
#endif
    return {};
}

} // namespace

QString FfmpegTools::ffmpegPath() { return findTool(QStringLiteral("ffmpeg")); }
QString FfmpegTools::ffprobePath() { return findTool(QStringLiteral("ffprobe")); }

QString FfmpegTools::missingToolsMessage(const bool needsEncoder)
{
    return needsEncoder
        ? QStringLiteral("FFmpeg with an HEVC encoder is required. Install FFmpeg and ensure "
                         "ffmpeg and ffprobe are available on PATH.")
        : QStringLiteral("FFprobe is required. Install FFmpeg and ensure ffprobe is available on PATH.");
}

} // namespace FlappedEar
