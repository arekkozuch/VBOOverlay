#pragma once

#include "export/MediaProbe.h"

namespace FlappedEar {

struct TemporaryOverlayValidationResult {
    bool codecOk = false;
    bool dimensionsOk = false;
    bool nominalRateExact = false;
    bool nominalRateOk = false;
    bool averageRateOk = false;
    bool durationOk = false;
    bool startTimeOk = false;
    qint64 timestampToleranceMicroseconds = 0;
    double rateTolerance = 0.0;

    [[nodiscard]] bool passed() const;
};

// Validates metadata emitted by the temporary Matroska file. The rawvideo
// producer owns the exact rate; Matroska's millisecond timestamps can make
// both r_frame_rate and avg_frame_rate a nearby, non-equivalent rational.
class TemporaryOverlayValidation final {
public:
    [[nodiscard]] static TemporaryOverlayValidationResult validate(
        const MediaInfo &overlay, const QSize &expectedSize, const MediaRational &scheduledRate,
        qsizetype expectedFrames, double scheduledDuration);
};

} // namespace FlappedEar
