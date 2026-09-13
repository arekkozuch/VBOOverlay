#include "telemetry/OutingLaps.h"

#include <QRegularExpression>
#include <QJsonDocument>
#include <QSet>

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace FlappedEar {

QString lapSectionName(const LapSectionType type)
{
    switch (type) {
    case LapSectionType::Out: return "OUT";
    case LapSectionType::Lap: return "LAP";
    case LapSectionType::In: return "IN";
    case LapSectionType::Unknown: return "UNKNOWN";
    }
    return {};
}

bool validLapReference(const QJsonObject &reference)
{
    if (reference.size() != 10 || !reference.value("version").isDouble()
        || reference.value("version").toDouble() != 1.0) return false;
    for (const auto *key : {"eventId", "runId", "sourceId", "algorithm"}) {
        const auto value = reference.value(key);
        if (!value.isString() || value.toString().trimmed().isEmpty()
            || value.toString().size() > 128 || value.toString().contains(QChar::Null)) return false;
    }
    static const QRegularExpression digest("^[0-9a-f]{64}$");
    for (const auto *key : {"sourceRevision", "derivationKey"}) {
        const auto value = reference.value(key);
        if (!value.isString() || !digest.match(value.toString()).hasMatch()) return false;
    }
    const auto type = reference.value("type");
    if (!type.isString() || !QStringList{"OUT", "LAP", "IN", "UNKNOWN"}.contains(type.toString())) return false;
    const auto start = reference.value("startTime"), end = reference.value("endTime");
    return start.isDouble() && end.isDouble() && std::isfinite(start.toDouble())
        && std::isfinite(end.toDouble()) && start.toDouble() >= 0 && end.toDouble() > start.toDouble();
}

QJsonObject makeLapReference(const OutingLapRow &row, const QString &eventId,
    const QString &sourceId, const QByteArray &sourceRevision, const QByteArray &derivationKey)
{
    const QJsonObject reference{{"version", 1}, {"algorithm", lapReferenceAlgorithm},
        {"eventId", eventId}, {"runId", row.runId}, {"sourceId", sourceId},
        {"sourceRevision", QString::fromLatin1(sourceRevision)},
        {"derivationKey", QString::fromLatin1(derivationKey)}, {"type", lapSectionName(row.type)},
        {"startTime", row.start}, {"endTime", row.end}};
    return validLapReference(reference) ? reference : QJsonObject{};
}

QByteArray lapReferenceKey(const QJsonObject &reference)
{
    return QJsonDocument(reference).toJson(QJsonDocument::Compact);
}

bool validLapExclusions(const QJsonValue &value, const QString &eventId)
{
    if (value.isUndefined()) return true;
    if (!value.isArray() || value.toArray().size() > maximumOutingLapRows) return false;
    QSet<QByteArray> seen;
    for (const auto &item : value.toArray()) {
        const auto entry = item.toObject();
        const auto reference = entry.value("reference").toObject();
        const auto reason = entry.value("reason");
        const auto key = lapReferenceKey(reference);
        if (entry.size() != 2 || !validLapReference(reference) || reference.value("type") != "LAP"
            || reference.value("eventId") != eventId || seen.contains(key) || !reason.isString()
            || reason.toString().trimmed().isEmpty() || reason.toString().size() > 256
            || reason.toString().contains(QChar::Null)) return false;
        seen.insert(key);
    }
    return true;
}

LapExclusionReasons lapExclusionReasons(const QJsonArray &exclusions)
{
    LapExclusionReasons reasons;
    for (const auto &item : exclusions) {
        const auto entry = item.toObject();
        reasons.insert(lapReferenceKey(entry.value("reference").toObject()), entry.value("reason").toString());
    }
    return reasons;
}

void applyLapExclusions(LapSession &laps, const QJsonObject &binding, const QJsonArray &exclusions)
{
    const auto reasons = lapExclusionReasons(exclusions);
    for (auto &lap : laps.timedLaps) {
        OutingLapRow row;
        row.runId = binding.value("runId").toString(); row.type = LapSectionType::Lap;
        row.start = lap.startTelemetryTime; row.end = lap.endTelemetryTime;
        const auto reference = makeLapReference(row, binding.value("eventId").toString(),
            binding.value("sourceId").toString(), binding.value("sourceRevision").toString().toLatin1(),
            binding.value("derivationKey").toString().toLatin1());
        lap.userExclusionReason = reasons.value(lapReferenceKey(reference));
    }
    recomputeLapRanking(laps);
}

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
    const auto append = [&](LapSectionType type, int number, double start, double end, const TimedLap *lap = nullptr) {
        throwIfCancelled(cancelled);
        if (!std::isfinite(start) || !std::isfinite(end) || start < 0 || end > session.duration || end <= start)
            return;
        OutingLapRow row{runId, runName, type, number, start, end, {}, sourceOrder, false, LapReferenceIssue::None, false, {}};
        if (lap) {
            row.referenceEligible = lap->referenceEligible();
            row.referenceIssue = lap->referenceIssue;
            row.bestOfRun = lap->referenceEligible() && laps.fastestLapIndex && *laps.fastestLapIndex >= 0
                && *laps.fastestLapIndex < laps.timedLaps.size()
                && laps.timedLaps[*laps.fastestLapIndex].number == lap->number;
        }
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
        append(LapSectionType::Lap, lap.number, lap.startTelemetryTime, lap.endTelemetryTime, &lap);
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
