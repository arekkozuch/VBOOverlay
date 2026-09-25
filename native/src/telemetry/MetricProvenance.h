#pragma once

#include <QString>

namespace FlappedEar {

// Shared status vocabulary for the Corner Analyzer view (KAN-55): how a
// displayed metric sample was obtained. Each of the four calculators (sector
// timing, corner speeds, braking, exit) keeps its own narrower provenance
// concept internally -- "measured"/"inferred" fields on some, none at all on
// sector times, which are always an interpolated boundary crossing rather
// than a recorded channel value. This is the single vocabulary the UI shows,
// derived from those at the point of display; it does not replace or
// retrofit the calculators' own fields.
enum class MetricProvenance { Measured, Calculated, Inferred, Unavailable };

[[nodiscard]] inline QString metricProvenanceName(const MetricProvenance provenance)
{
    switch (provenance) {
    case MetricProvenance::Measured: return QStringLiteral("measured");
    case MetricProvenance::Calculated: return QStringLiteral("calculated");
    case MetricProvenance::Inferred: return QStringLiteral("inferred");
    case MetricProvenance::Unavailable: return QStringLiteral("unavailable");
    }
    return QStringLiteral("unavailable");
}

} // namespace FlappedEar
