#include "telemetry/OutingLaps.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace FlappedEar {

std::optional<qint64> recordingTimestamp(const TelemetrySession &session)
{
    bool ok = false;
    const auto value = session.metadata.value("firstTimestampMilliseconds").toLongLong(&ok);
    return ok ? std::optional<qint64>(value) : std::nullopt;
}

QVector<OutingLapRow> outingLapRows(const TelemetrySession &session, const LapSession &laps,
    const QString &runId, const QString &runName, const qsizetype sourceOrder,
    const CancellationCheck &cancelled)
{
    throwIfCancelled(cancelled);
    QVector<OutingLapRow> rows;
    if (!std::isfinite(session.duration) || session.duration <= 0) return rows;
    if (laps.timedLaps.size() > maximumOutingLapRows - 2)
        throw ResourceLimitError("Too many lap sections in this recording.");
    const auto origin = recordingTimestamp(session);
    const auto append = [&](LapSectionType type, int number, double start, double end) {
        throwIfCancelled(cancelled);
        if (!std::isfinite(start) || !std::isfinite(end) || start < 0 || end > session.duration || end <= start)
            return;
        OutingLapRow row{runId, runName, type, number, start, end, {}, sourceOrder};
        const double milliseconds = start * 1000.0;
        if (origin && milliseconds < static_cast<double>(std::numeric_limits<qint64>::max())) {
            const auto delta = static_cast<qint64>(std::llround(milliseconds));
            if (*origin <= std::numeric_limits<qint64>::max() - delta)
                row.timestampMilliseconds = *origin + delta;
        }
        rows.append(row);
    };
    if (laps.acceptedPasses.isEmpty()) {
        append(LapSectionType::Unknown, 0, 0, session.duration);
        return rows;
    }
    append(LapSectionType::Out, 0, 0, laps.acceptedPasses.first().telemetryTime);
    for (const auto &lap : laps.timedLaps)
        append(LapSectionType::Lap, lap.number, lap.startTelemetryTime, lap.endTelemetryTime);
    append(LapSectionType::In, 0, laps.acceptedPasses.last().telemetryTime, session.duration);
    return rows;
}

void sortOutingLaps(QVector<OutingLapRow> &rows)
{
    if (rows.size() > maximumOutingLapRows) throw ResourceLimitError("Too many lap sections in this outing.");
    // Unknown clocks remain explicit and stable, after the time-ordered records.
    std::stable_sort(rows.begin(), rows.end(), [](const auto &a, const auto &b) {
        if (a.timestampMilliseconds.has_value() != b.timestampMilliseconds.has_value())
            return a.timestampMilliseconds.has_value();
        if (a.timestampMilliseconds && a.timestampMilliseconds != b.timestampMilliseconds)
            return *a.timestampMilliseconds < *b.timestampMilliseconds;
        if (a.sourceOrder != b.sourceOrder) return a.sourceOrder < b.sourceOrder;
        return a.start < b.start;
    });
}

} // namespace FlappedEar
