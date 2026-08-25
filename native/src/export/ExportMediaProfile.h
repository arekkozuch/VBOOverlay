#pragma once

#include "export/MediaProbe.h"

#include <QSize>
#include <QString>

namespace FlappedEar {

enum class SourcePreservationMode {
    PreserveSourceCharacteristics,
};

struct ExportMediaProfile {
    QSize outputSize;
    MediaRational frameRate;
    qint64 videoBitrate = 0;
    QString outputPixelFormat;
    int outputBitDepth = 0;
    QString colorRange;
    QString colorSpace;
    QString colorTransfer;
    QString colorPrimaries;
    QString encoder;
    QString encoderProfile;
    SourcePreservationMode sourcePreservationMode =
        SourcePreservationMode::PreserveSourceCharacteristics;
    bool supported = false;
    QString error;

    [[nodiscard]] static ExportMediaProfile derive(
        const MediaInfo &source, const QSize &outputSize,
        const MediaRational &frameRate, qint64 videoBitrate,
        const QString &encoder);
    [[nodiscard]] static QString unsupportedDisplayTransformError(const MediaInfo &source);
    [[nodiscard]] bool acceptsOutputPixelFormat(const QString &actualPixelFormat) const;
};

} // namespace FlappedEar
