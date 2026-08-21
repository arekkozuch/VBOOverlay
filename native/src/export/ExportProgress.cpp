#include "export/ExportProgress.h"

#include <QtGlobal>
#include <cmath>

namespace FlappedEar {

ExportProgressSnapshot ExportProgressEstimator::update(
    const qsizetype renderedFrames, const qsizetype totalFrames, const qint64 elapsedMilliseconds,
    const MediaRational &outputRate)
{
    m_samples.append({renderedFrames, elapsedMilliseconds});
    while (m_samples.size() > 1 && elapsedMilliseconds - m_samples.front().milliseconds > 5'000) {
        m_samples.removeFirst();
    }
    ExportProgressSnapshot result;
    result.visiblePercent = stageProgress(
        QStringLiteral("rendering"), totalFrames > 0 ? double(renderedFrames) / totalFrames : 0.0);
    if (elapsedMilliseconds < 1'000 || renderedFrames < 10 || m_samples.isEmpty()) {
        return result;
    }
    const Sample first = m_samples.front();
    const qint64 span = elapsedMilliseconds - first.milliseconds;
    const qsizetype frames = renderedFrames - first.frames;
    if (span < 500 || frames == 0) {
        return result;
    }
    result.throughputFps = 1'000.0 * frames / span;
    result.realtimeFactor = outputRate.isValid() ? result.throughputFps / outputRate.value() : 0.0;
    result.etaSeconds = totalFrames > renderedFrames
        ? double(totalFrames - renderedFrames) / result.throughputFps
        : 0.0;
    result.etaAvailable = true;
    return result;
}

double ExportProgressEstimator::stageProgress(const QString &stage, const double frameFraction)
{
    if (stage == "rendering") return qBound(0.0, frameFraction, 1.0) * 95.0;
    if (stage == "finalizing") return 97.0;
    if (stage == "validating") return 99.0;
    if (stage == "complete") return 100.0;
    return 0.0;
}

QList<FfmpegProgress> FfmpegProgressParser::append(QByteArray data)
{
    m_pending += std::move(data);
    QList<FfmpegProgress> updates;
    qsizetype newline = -1;
    while ((newline = m_pending.indexOf('\n')) >= 0) {
        const QByteArray line = m_pending.left(newline).trimmed();
        m_pending.remove(0, newline + 1);
        const qsizetype separator = line.indexOf('=');
        if (separator < 1) {
            continue;
        }
        const QByteArray key = line.left(separator);
        const QByteArray value = line.mid(separator + 1);
        bool valid = false;
        if (key == "frame") {
            const qlonglong frame = value.toLongLong(&valid);
            if (valid && frame >= 0) m_current.encodedFrames = static_cast<qsizetype>(frame);
        } else if (key == "out_time_us" || key == "out_time_ms") {
            const qlonglong time = value.toLongLong(&valid);
            if (valid && time >= 0) m_current.outputMicroseconds = time;
        } else if (key == "fps") {
            const double fps = value.toDouble(&valid);
            if (valid && std::isfinite(fps)) m_current.encoderFps = fps;
        } else if (key == "speed") {
            QByteArray numeric = value;
            if (numeric.endsWith('x')) numeric.chop(1);
            const double speed = numeric.toDouble(&valid);
            if (valid && std::isfinite(speed)) m_current.realtimeFactor = speed;
        } else if (key == "progress") {
            m_current.complete = value == "end";
            updates.append(m_current);
        }
    }
    return updates;
}

double FfmpegProgressParser::overallPercent(const double outputSeconds, const double durationSeconds)
{
    if (!std::isfinite(outputSeconds) || !std::isfinite(durationSeconds) || durationSeconds <= 0.0) {
        return 0.0;
    }
    return qBound(0.0, outputSeconds / durationSeconds, 1.0) * 95.0;
}

} // namespace FlappedEar
