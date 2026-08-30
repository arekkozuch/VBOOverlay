#pragma once

#include "export/MediaProbe.h"

#include <optional>

namespace FlappedEar {

enum class FinalOutputClassification { Failure, Success, SuccessWithWarning };

struct FinalOutputEvidence {
    qint64 expectedFrames = 0;
    qint64 stageAGeneratedFrames = 0;
    qint64 stageASubmittedFrames = 0;
    qint64 temporaryOverlayFrames = 0;
    std::optional<qint64> stageBProgressFrames;
    qint64 finalFrameCount = 0;
    MediaInfo finalMedia;
    MediaRational frameRate;
    bool otherValidationPassed = false;
    bool outputTransactionSafe = false;
};

struct FinalOutputValidationResult {
    FinalOutputClassification classification = FinalOutputClassification::Failure;
    qint64 frameDeficit = 0;
    bool stageCountsMatch = false;
    bool stageBProgressMatches = false;
    bool startsAtOrigin = false;
    bool contiguousCfrTiming = false;

    [[nodiscard]] bool accepted() const
    {
        return classification != FinalOutputClassification::Failure;
    }
};

class FinalOutputValidation final {
public:
    [[nodiscard]] static FinalOutputValidationResult evaluate(const FinalOutputEvidence &evidence);
};

} // namespace FlappedEar
