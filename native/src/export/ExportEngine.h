#pragma once

#include "export/MediaProbe.h"

#include <QSize>
#include <QString>

namespace FlappedEar {

class TelemetryFrameRenderer;

struct ExportSettings {
    QString inputPath;
    QString outputPath;
    QSize outputSize;
    MediaRational frameRate;
    double startTime = 0.0;
    double endTime = 0.0;
    QString encoder;
};

struct ExportResult {
    bool success = false;
    QString error;
    MediaInfo mediaInfo;
    qsizetype renderedFrames = 0;
};

class ExportEngine final {
public:
    [[nodiscard]] static ExportResult exportVideo(
        const ExportSettings &settings, TelemetryFrameRenderer &renderer);
    [[nodiscard]] static qsizetype frameCount(
        double startTime, double endTime, const MediaRational &frameRate);
};

} // namespace FlappedEar
