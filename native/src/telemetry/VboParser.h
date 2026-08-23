#pragma once

#include "telemetry/TelemetrySession.h"
#include "telemetry/SourceOperation.h"

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
    static constexpr qint64 kMaximumFileBytes = 128LL * 1024 * 1024;
    static constexpr qsizetype kMaximumLines = 1'000'000;
    static constexpr qsizetype kMaximumDataRows = 500'000;
    static constexpr qsizetype kMaximumColumns = 512;
    static constexpr qsizetype kMaximumLineCharacters = 1'048'576;
    static constexpr qsizetype kMaximumFieldCharacters = 65'536;

    [[nodiscard]] static TelemetrySession parse(
        QStringView text, const CancellationCheck &cancelled = {});
    [[nodiscard]] static TelemetrySession parseFile(
        const QString &path, const CancellationCheck &cancelled = {});
};

} // namespace FlappedEar
