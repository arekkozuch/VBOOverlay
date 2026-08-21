#pragma once

#include <QString>

namespace FlappedEar {

class FfmpegTools final {
public:
    [[nodiscard]] static QString ffmpegPath();
    [[nodiscard]] static QString ffprobePath();
    [[nodiscard]] static QString missingToolsMessage(bool needsEncoder = true);
};

} // namespace FlappedEar
