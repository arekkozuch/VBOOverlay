#pragma once

#include "telemetry/TelemetrySession.h"

#include <QString>
#include <QStringView>
#include <stdexcept>

namespace FlappedEar {

class VboParseError final : public std::runtime_error {
public:
    explicit VboParseError(const QString &message);
};

class VboParser final {
public:
    [[nodiscard]] static TelemetrySession parse(QStringView text);
    [[nodiscard]] static TelemetrySession parseFile(const QString &path);
};

} // namespace FlappedEar
