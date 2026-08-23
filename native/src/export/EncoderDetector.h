#pragma once

#include <QList>
#include <QHash>
#include <QSize>
#include <QString>
#include "export/MediaProbe.h"
#include <functional>

namespace FlappedEar {

struct EncoderCapability {
    QString id;
    QString displayName;
    bool hardware = false;
};

struct EncoderProfileRequest {
    QString ffmpegExecutable;
    QString encoder;
    QSize size;
    MediaRational frameRate;
    QString pixelFormat;
    int bitDepth = 0;
    QString profile;

    [[nodiscard]] QString cacheKey() const;
};

struct EncoderProfileSupport {
    bool supported = false;
    bool cacheHit = false;
    QString error;
};

class EncoderCapabilityCache final {
public:
    using Probe = std::function<EncoderProfileSupport(const EncoderProfileRequest &)>;
    [[nodiscard]] EncoderProfileSupport verify(
        const EncoderProfileRequest &request, const Probe &probe);
    [[nodiscard]] qsizetype size() const;
private:
    QHash<QString, EncoderProfileSupport> m_results;
};

class EncoderDetector final {
public:
    [[nodiscard]] static QList<EncoderCapability> discover(
        const QString &ffmpegPath = {}, const std::function<bool()> &cancelled = {});
    [[nodiscard]] static QList<EncoderCapability> parseEncoders(const QString &output);
    [[nodiscard]] static QString preferredHevcEncoder(const QList<EncoderCapability> &encoders);
    [[nodiscard]] static EncoderProfileSupport verifyProfile(
        const EncoderProfileRequest &request, const QString &ffmpegPath = {},
        const std::function<bool()> &cancelled = {});
};

} // namespace FlappedEar
