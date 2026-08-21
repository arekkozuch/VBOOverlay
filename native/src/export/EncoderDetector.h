#pragma once

#include <QList>
#include <QString>
#include <functional>

namespace FlappedEar {

struct EncoderCapability {
    QString id;
    QString displayName;
    bool hardware = false;
};

class EncoderDetector final {
public:
    [[nodiscard]] static QList<EncoderCapability> discover(
        const QString &ffmpegPath = {}, const std::function<bool()> &cancelled = {});
    [[nodiscard]] static QList<EncoderCapability> parseEncoders(const QString &output);
    [[nodiscard]] static QString preferredHevcEncoder(const QList<EncoderCapability> &encoders);
};

} // namespace FlappedEar
