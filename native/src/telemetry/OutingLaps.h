#pragma once

#include "telemetry/LapTiming.h"
#include <QJsonObject>

namespace FlappedEar {

enum class LapSectionType { Out, Lap, In, Unknown };

struct OutingLapRow {
    QString runId;
    QString runName;
    LapSectionType type = LapSectionType::Unknown;
    int lapNumber = 0;
    double start = 0;
    double end = 0;
    std::optional<qint64> timestampMilliseconds;
    qsizetype sourceOrder = 0;
    bool referenceEligible = false;
    LapReferenceIssue referenceIssue = LapReferenceIssue::None;
    bool bestOfRun = false;
    QJsonObject reference = {};
};

// Portable references use exact telemetry bounds, not row indices or lap numbers.
// Algorithm changes that can alter sections must bump this tag.
inline constexpr auto lapReferenceAlgorithm = "source-laps-v1";
[[nodiscard]] QString lapSectionName(LapSectionType type);
[[nodiscard]] QJsonObject makeLapReference(const OutingLapRow &row, const QString &eventId,
    const QString &sourceId, const QByteArray &sourceRevision, const QByteArray &derivationKey);
[[nodiscard]] bool validLapReference(const QJsonObject &reference);

inline constexpr qsizetype maximumOutingLapRows = 20'000;

[[nodiscard]] std::optional<qint64> recordingTimestamp(const TelemetrySession &session);
[[nodiscard]] QVector<OutingLapRow> outingLapRows(
    const TelemetrySession &session, const LapSession &laps, const QString &runId,
    const QString &runName, qsizetype sourceOrder, const CancellationCheck &cancelled = {});
void sortOutingLaps(QVector<OutingLapRow> &rows);

} // namespace FlappedEar
