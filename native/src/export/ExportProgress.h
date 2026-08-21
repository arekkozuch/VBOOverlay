#pragma once

#include "export/MediaProbe.h"

#include <QList>

namespace FlappedEar {

struct ExportProgressSnapshot {
    double visiblePercent = 0.0;
    double throughputFps = 0.0;
    double realtimeFactor = 0.0;
    double etaSeconds = -1.0;
    bool etaAvailable = false;
};

class ExportProgressEstimator final {
public:
    [[nodiscard]] ExportProgressSnapshot update(
        qsizetype renderedFrames, qsizetype totalFrames, qint64 elapsedMilliseconds,
        const MediaRational &outputRate);
    [[nodiscard]] static double stageProgress(const QString &stage, double frameFraction);

private:
    struct Sample { qsizetype frames; qint64 milliseconds; };
    QList<Sample> m_samples;
};

} // namespace FlappedEar
