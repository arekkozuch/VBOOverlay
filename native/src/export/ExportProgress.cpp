#include "export/ExportProgress.h"

#include <QtGlobal>

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

} // namespace FlappedEar
