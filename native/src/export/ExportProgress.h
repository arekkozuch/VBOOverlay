#pragma once

#include "export/MediaProbe.h"

#include <QList>
#include <QByteArray>
#include <QString>

namespace FlappedEar {

struct ExportProgressSnapshot {
    double visiblePercent = 0.0;
    double throughputFps = 0.0;
    double realtimeFactor = 0.0;
    double etaSeconds = -1.0;
    bool etaAvailable = false;
};

// FFmpeg's machine-readable -progress pipe output. Keep this independent of
// QProcess so its parsing and progress semantics remain deterministic in tests.
struct FfmpegProgress {
    qsizetype encodedFrames = 0;
    bool encodedFramesAvailable = false;
    qint64 outputMicroseconds = -1;
    double encoderFps = 0.0;
    double realtimeFactor = 0.0;
    bool complete = false;
};

class FfmpegProgressParser final {
public:
    [[nodiscard]] QList<FfmpegProgress> append(QByteArray data);
    [[nodiscard]] static double overallPercent(double outputSeconds, double durationSeconds);
    [[nodiscard]] bool overflowed() const;

private:
    QByteArray m_pending;
    FfmpegProgress m_current;
    bool m_overflowed = false;
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
