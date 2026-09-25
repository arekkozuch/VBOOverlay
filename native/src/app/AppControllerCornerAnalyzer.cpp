// KAN-55: Corner Analyzer A/B evidence. Combines the four existing per-lap
// metric calculators (sector timing KAN-51, corner speeds KAN-52, braking
// KAN-53, exit effects KAN-54) for each comparison slot's own approved
// segmentation, using the shared comparison progress axis
// (ensureComparisonProgressAxis) rather than a second alignment. Segments are
// only ever shown when both slots' approved segmentation matches exactly
// (same revision, same track configuration) -- never a guessed
// correspondence between two independently-approved sets -- or, when opened
// from a theoretical-best sector (KAN-57), against the canonical run's
// segmentation that result used, labelled via comparisonSegmentationNote().

#include "app/AppController.h"
#include "telemetry/MetricProvenance.h"

using namespace FlappedEar;

namespace {

QVariantMap provenanceValue(
    const std::optional<double> value, const MetricProvenance provenance, const QString &unavailableReason = {})
{
    QVariantMap map{{"provenance", metricProvenanceName(provenance)}};
    if (value) map.insert("value", *value);
    else if (!unavailableReason.isEmpty()) map.insert("unavailableReason", unavailableReason);
    return map;
}

QVariantMap deltaValue(const std::optional<double> value, const QString &unavailableReason = {})
{
    QVariantMap map;
    if (value) map.insert("value", *value);
    else if (!unavailableReason.isEmpty()) map.insert("unavailableReason", unavailableReason);
    return map;
}

MetricProvenance cornerSpeedPhaseProvenance(const CornerSpeeds &speeds, const CornerSpeedValue &phase)
{
    if (!phase.value) return MetricProvenance::Unavailable;
    return speeds.provenance == QLatin1String("measured") ? MetricProvenance::Measured : MetricProvenance::Unavailable;
}

MetricProvenance namedProvenance(const QString &provenance, const bool hasValue)
{
    if (!hasValue) return MetricProvenance::Unavailable;
    if (provenance == QLatin1String("measured")) return MetricProvenance::Measured;
    if (provenance == QLatin1String("inferred")) return MetricProvenance::Inferred;
    return MetricProvenance::Unavailable;
}

} // namespace

ApprovedSegmentation AppController::comparisonApprovedSegmentation(const int slot) const
{
    if (slot < 0 || slot > 1) return {};
    const auto &row = m_comparisonSlots[slot].row;
    const auto runId = row.value("runId").toString();
    const auto configuration = row.value("compatibilityGroupId").toString();
    for (const auto &value : currentProjectObject().value("event").toObject().value("runs").toArray()) {
        const auto run = value.toObject();
        if (run.value("id").toString() == runId)
            return approvedSegmentation(run.value("trackSegments"), configuration);
    }
    return {};
}

std::optional<ApprovedSegmentation> AppController::comparisonSharedSegmentation() const
{
    if (!comparisonPairReady()) return std::nullopt;
    const auto approvedA = comparisonApprovedSegmentation(0);
    const auto approvedB = comparisonApprovedSegmentation(1);
    if (approvedA.valid && approvedB.valid && !approvedA.revision.isEmpty()
        && approvedA.revision == approvedB.revision
        && approvedA.trackConfigurationReference == approvedB.trackConfigurationReference)
        return approvedA;
    // KAN-57: opened from a theoretical-best sector -- use the same canonical
    // segmentation the theoretical best measured every lap against, but only
    // when both laps belong to the group it was approved for.
    if (m_comparisonSegmentationRunId.isEmpty()) return std::nullopt;
    const auto group = m_comparisonSlots[0].row.value("compatibilityGroupId").toString();
    if (group.isEmpty() || m_comparisonSlots[1].row.value("compatibilityGroupId").toString() != group)
        return std::nullopt;
    for (const auto &value : currentProjectObject().value("event").toObject().value("runs").toArray()) {
        const auto run = value.toObject();
        if (run.value("id").toString() != m_comparisonSegmentationRunId) continue;
        auto canonical = approvedSegmentation(run.value("trackSegments"), group);
        if (canonical.valid && !canonical.revision.isEmpty() && !canonical.segments.isEmpty()) return canonical;
    }
    return std::nullopt;
}

QString AppController::comparisonSegmentationNote() const
{
    if (!comparisonPairReady() || m_comparisonSegmentationRunId.isEmpty()) return {};
    const auto approvedA = comparisonApprovedSegmentation(0);
    const auto approvedB = comparisonApprovedSegmentation(1);
    if (approvedA.valid && !approvedA.revision.isEmpty() && approvedA.revision == approvedB.revision) return {};
    if (!comparisonSharedSegmentation()) return {};
    QString runName = m_comparisonSegmentationRunId;
    for (const auto &value : currentProjectObject().value("event").toObject().value("runs").toArray())
        if (value.toObject().value("id").toString() == m_comparisonSegmentationRunId)
            runName = value.toObject().value("name").toString();
    return tr("Segments approved on %1, as used by the sector theoretical best. Boundaries are distances along "
              "that run's axis, so they can shift by a few metres on these laps.").arg(runName);
}

QVariantList AppController::comparisonApprovedSegments() const
{
    const auto shared = comparisonSharedSegmentation();
    if (!shared) return {};
    QVariantList rows;
    for (const auto &value : shared->segments) {
        const auto segment = value.toObject();
        rows.append(QVariantMap{{"id", segment.value("id").toString()}, {"name", segment.value("name").toString()},
            {"type", segment.value("type").toString()},
            {"startMeters", segment.value("startProgressMeters").toDouble()},
            {"endMeters", segment.value("endProgressMeters").toDouble()}});
    }
    return rows;
}

QVariantMap AppController::comparisonSegmentMetrics(const QString &segmentId) const
{
    if (!comparisonPairReady() || segmentId.isEmpty()) return {};
    ensureComparisonProgressAxis();
    if (!m_comparisonProgressAxis.valid) return {};
    const auto shared = comparisonSharedSegmentation();
    if (!shared) return {};
    const auto &approvedA = *shared;
    const auto &approvedB = *shared;
    QString segmentType;
    for (const auto &value : approvedA.segments) {
        const auto segment = value.toObject();
        if (segment.value("id").toString() == segmentId) { segmentType = segment.value("type").toString(); break; }
    }
    if (segmentType.isEmpty()) return {};

    const auto &slotA = m_comparisonSlots[0];
    const auto &slotB = m_comparisonSlots[1];
    if (!slotA.session || !slotB.session) return {};

    QVariantMap result{{"segmentId", segmentId}, {"type", segmentType}};

    // Sector time: every segment type, not just corners.
    {
        const auto timesA = computeLapSectorTimes(approvedA, m_comparisonProgressAxis.lengthMeters,
            m_comparisonProgressTraceCache[0], slotA.row.value("startTime").toDouble(),
            slotA.row.value("endTime").toDouble(), QJsonObject::fromVariantMap(slotA.row.value("reference").toMap()));
        const auto timesB = computeLapSectorTimes(approvedB, m_comparisonProgressAxis.lengthMeters,
            m_comparisonProgressTraceCache[1], slotB.row.value("startTime").toDouble(),
            slotB.row.value("endTime").toDouble(), QJsonObject::fromVariantMap(slotB.row.value("reference").toMap()));
        const SectorTime *sectorA = nullptr;
        const SectorTime *sectorB = nullptr;
        for (const auto &sector : timesA.sectors) if (sector.segmentId == segmentId) sectorA = &sector;
        for (const auto &sector : timesB.sectors) if (sector.segmentId == segmentId) sectorB = &sector;
        if (sectorA && sectorB) {
            const auto comparison = compareSectorTimes(timesA, timesB, segmentId);
            result.insert("sectorTime", QVariantMap{
                {"a", provenanceValue(sectorA->seconds,
                     sectorA->seconds ? MetricProvenance::Calculated : MetricProvenance::Unavailable, sectorA->unavailableReason)},
                {"b", provenanceValue(sectorB->seconds,
                     sectorB->seconds ? MetricProvenance::Calculated : MetricProvenance::Unavailable, sectorB->unavailableReason)},
                {"delta", deltaValue(comparison.secondsDelta, comparison.unavailableReason)}});
        }
    }

    // Corner-only metrics (KAN-52/53/54's own scope, mirroring outingLap*Metrics).
    if (segmentType != trackSegmentTypeName(TrackSegmentType::Corner)) return result;
    const auto features = computeTrackFeatures(m_comparisonProgressAxis, segmentReviewSmoothingMeters);

    {
        const auto speedsA = computeCornerSpeeds(m_comparisonProgressAxis, features, approvedA, segmentId,
            m_comparisonProgressTraceCache[0], *slotA.session);
        const auto speedsB = computeCornerSpeeds(m_comparisonProgressAxis, features, approvedB, segmentId,
            m_comparisonProgressTraceCache[1], *slotB.session);
        if (speedsA.valid && speedsB.valid) {
            const auto comparison = compareCornerSpeeds(speedsA, speedsB);
            const auto phase = [&](const CornerSpeedValue &a, const CornerSpeedValue &b, const std::optional<double> &delta) {
                return QVariantMap{
                    {"a", provenanceValue(a.value, cornerSpeedPhaseProvenance(speedsA, a), a.unavailableReason)},
                    {"b", provenanceValue(b.value, cornerSpeedPhaseProvenance(speedsB, b), b.unavailableReason)},
                    {"delta", deltaValue(delta, comparison.unavailableReason)}};
            };
            result.insert("corner", QVariantMap{{"channel", speedsA.channel}, {"unit", speedsA.unit},
                {"entry", phase(speedsA.entry, speedsB.entry, comparison.entryDelta)},
                {"apex", phase(speedsA.apex, speedsB.apex, comparison.apexDelta)},
                {"minimum", phase(speedsA.minimum, speedsB.minimum, comparison.minimumDelta)},
                {"exit", phase(speedsA.exit, speedsB.exit, comparison.exitDelta)}});
        }
    }

    {
        const auto brakingA = computeBrakingMetrics(m_comparisonProgressAxis.lengthMeters, approvedA, segmentId,
            m_comparisonProgressTraceCache[0], *slotA.session, slotA.row.value("startTime").toDouble(),
            slotA.row.value("endTime").toDouble());
        const auto brakingB = computeBrakingMetrics(m_comparisonProgressAxis.lengthMeters, approvedB, segmentId,
            m_comparisonProgressTraceCache[1], *slotB.session, slotB.row.value("startTime").toDouble(),
            slotB.row.value("endTime").toDouble());
        if (brakingA.valid && brakingB.valid) {
            const auto comparison = compareBrakingMetrics(brakingA, brakingB);
            result.insert("braking", QVariantMap{
                {"point", QVariantMap{
                     {"a", provenanceValue(brakingA.brakingPointMeters,
                          namedProvenance(brakingA.provenance, brakingA.brakingPointMeters.has_value()), brakingA.unavailableReason)},
                     {"b", provenanceValue(brakingB.brakingPointMeters,
                          namedProvenance(brakingB.provenance, brakingB.brakingPointMeters.has_value()), brakingB.unavailableReason)},
                     {"delta", deltaValue(comparison.brakingPointDeltaMeters, comparison.unavailableReason)}}},
                {"seconds", QVariantMap{
                     {"a", provenanceValue(brakingA.brakingSeconds,
                          namedProvenance(brakingA.provenance, brakingA.brakingSeconds.has_value()), brakingA.unavailableReason)},
                     {"b", provenanceValue(brakingB.brakingSeconds,
                          namedProvenance(brakingB.provenance, brakingB.brakingSeconds.has_value()), brakingB.unavailableReason)},
                     {"delta", deltaValue(comparison.brakingSecondsDelta, comparison.unavailableReason)}}},
                {"peakDeceleration", QVariantMap{
                     {"a", provenanceValue(brakingA.peakDeceleration,
                          namedProvenance(brakingA.provenance, brakingA.peakDeceleration.has_value()), brakingA.decelerationUnavailableReason)},
                     {"b", provenanceValue(brakingB.peakDeceleration,
                          namedProvenance(brakingB.provenance, brakingB.peakDeceleration.has_value()), brakingB.decelerationUnavailableReason)},
                     {"delta", deltaValue(comparison.peakDecelerationDelta, comparison.unavailableReason)}}}});
        }
    }

    {
        const auto exitA = computeExitMetrics(m_comparisonProgressAxis.lengthMeters, approvedA, segmentId,
            m_comparisonProgressTraceCache[0], *slotA.session, slotA.row.value("endTime").toDouble());
        const auto exitB = computeExitMetrics(m_comparisonProgressAxis.lengthMeters, approvedB, segmentId,
            m_comparisonProgressTraceCache[1], *slotB.session, slotB.row.value("endTime").toDouble());
        if (exitA.valid && exitB.valid) {
            const auto comparison = compareExitMetrics(exitA, exitB);
            result.insert("exitEffects", QVariantMap{
                {"pickup", QVariantMap{
                     {"a", provenanceValue(exitA.pickup.progressMeters,
                          namedProvenance(exitA.pickup.provenance, exitA.pickup.progressMeters.has_value()), exitA.pickup.unavailableReason)},
                     {"b", provenanceValue(exitB.pickup.progressMeters,
                          namedProvenance(exitB.pickup.provenance, exitB.pickup.progressMeters.has_value()), exitB.pickup.unavailableReason)},
                     {"delta", deltaValue(comparison.pickupDeltaMeters, comparison.pickupUnavailableReason)}}},
                {"exitSpeed", QVariantMap{
                     {"a", provenanceValue(exitA.exitSpeed,
                          exitA.exitSpeed ? MetricProvenance::Measured : MetricProvenance::Unavailable, exitA.downstreamUnavailableReason)},
                     {"b", provenanceValue(exitB.exitSpeed,
                          exitB.exitSpeed ? MetricProvenance::Measured : MetricProvenance::Unavailable, exitB.downstreamUnavailableReason)},
                     {"delta", deltaValue(comparison.exitSpeedDelta)}}}});
        }
    }

    return result;
}

QVariantMap AppController::comparisonTimeLossObservations() const
{
    const auto shared = comparisonSharedSegmentation();
    if (!shared) return {{"valid", false}};
    ensureComparisonProgressAxis();
    if (!m_comparisonProgressAxis.valid) return {{"valid", false}};
    const auto &slotA = m_comparisonSlots[0];
    const auto &slotB = m_comparisonSlots[1];
    const auto startA = slotA.row.value("startTime").toDouble(), endA = slotA.row.value("endTime").toDouble();
    const auto startB = slotB.row.value("startTime").toDouble(), endB = slotB.row.value("endTime").toDouble();
    const auto timesA = computeLapSectorTimes(*shared, m_comparisonProgressAxis.lengthMeters,
        m_comparisonProgressTraceCache[0], startA, endA, QJsonObject::fromVariantMap(slotA.row.value("reference").toMap()));
    const auto timesB = computeLapSectorTimes(*shared, m_comparisonProgressAxis.lengthMeters,
        m_comparisonProgressTraceCache[1], startB, endB, QJsonObject::fromVariantMap(slotB.row.value("reference").toMap()));
    const auto observations = computeTimeLossObservations(
        *shared, m_comparisonProgressAxis.lengthMeters, timesA, startA, timesB, startB);
    if (!observations.valid) return {{"valid", false}, {"unavailableReason", observations.unavailableReason}};
    QVariantList windows;
    for (const auto &window : observations.windows) {
        QVariantMap row{{"segmentId", window.segmentId}, {"name", window.name}, {"type", window.type},
            {"role", window.role}, {"startMeters", window.startProgressMeters}, {"endMeters", window.endProgressMeters}};
        if (!window.cornerSegmentId.isEmpty()) row.insert("cornerSegmentId", window.cornerSegmentId);
        if (window.incrementSeconds) row.insert("incrementSeconds", *window.incrementSeconds);
        else row.insert("unavailableReason", window.unavailableReason);
        if (window.cumulativeAtStartSeconds) row.insert("cumulativeAtStartSeconds", *window.cumulativeAtStartSeconds);
        if (window.cumulativeAtEndSeconds) row.insert("cumulativeAtEndSeconds", *window.cumulativeAtEndSeconds);
        windows.append(row);
    }
    return {{"valid", true}, {"algorithm", QString::fromLatin1(timeLossAlgorithm)},
        {"revision", observations.stamp.revision}, {"windows", windows},
        {"allWindowsTimed", observations.allWindowsTimed},
        {"timedIncrementSumSeconds", observations.timedIncrementSumSeconds},
        {"lapDeltaSeconds", (endA - startA) - (endB - startB)}};
}
