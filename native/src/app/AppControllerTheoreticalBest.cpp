// KAN-56: theoretical best sector times across a whole compatible population
// of laps (not just the two comparison slots or the single open lap).
// Segments are approved strictly per run (KAN-48-50); there is no cross-run
// merge mechanism, so one run's approved segmentation stands in as canonical
// for the whole population, the same way KAN-55's comparison view requires an
// exact revision match rather than guessing a correspondence between two
// independently-approved sets.

#include "app/AppController.h"
#include "telemetry/OutingLaps.h"
#include "telemetry/SectorTiming.h"
#include "telemetry/TheoreticalBest.h"
#include "telemetry/TrackProgress.h"

#include <QtConcurrent/QtConcurrentRun>

#include <algorithm>

using namespace FlappedEar;

void AppController::initializeOutingTheoreticalBest()
{
    connect(&m_theoreticalBestWatcher, &QFutureWatcher<TheoreticalBestResult>::finished, this, [this] {
        auto result = m_theoreticalBestWatcher.future().takeResult();
        if (result.request != m_theoreticalBestRequest) return; // stale: outing laps changed or a new request started
        if (!result.error.isEmpty()) {
            m_theoreticalBestState = QStringLiteral("error");
            m_theoreticalBestMessage = result.error;
            m_theoreticalBestBest = {};
        } else if (!result.best.valid) {
            m_theoreticalBestState = QStringLiteral("unavailable");
            m_theoreticalBestMessage = result.best.unavailableReason;
            m_theoreticalBestBest = {};
        } else {
            m_theoreticalBestState = QStringLiteral("ready");
            m_theoreticalBestMessage = result.best.totalSeconds ? QString() : result.best.unavailableReason;
            m_theoreticalBestBest = std::move(result.best);
        }
        emit outingTheoreticalBestChanged();
    });
    // The eligible population, its approved segmentation or the document
    // itself can change under an in-flight or displayed result; never show a
    // theoretical best computed from laps that no longer apply.
    const auto invalidate = [this] {
        if (m_theoreticalBestState == "idle") return;
        ++m_theoreticalBestRequest;
        if (m_theoreticalBestCancellation) m_theoreticalBestCancellation->store(true);
        m_theoreticalBestState = QStringLiteral("idle");
        m_theoreticalBestMessage.clear();
        m_theoreticalBestBest = {};
        emit outingTheoreticalBestChanged();
    };
    connect(this, &AppController::outingLapsChanged, this, invalidate);
    connect(this, &AppController::documentStateChanged, this, invalidate);
}

QVariantMap AppController::outingTheoreticalBest() const
{
    QVariantMap result{{"state", m_theoreticalBestState}, {"message", m_theoreticalBestMessage}};
    if (m_theoreticalBestState != "ready") return result;
    QVariantList sectors;
    for (const auto &sector : m_theoreticalBestBest.sectors) {
        QVariantMap row{{"segmentId", sector.segmentId}, {"name", sector.name}, {"type", sector.type}};
        if (sector.seconds) {
            row.insert("seconds", *sector.seconds);
            QString label;
            const auto resolved = resolveOutingLapReference(sector.sourceLapReference.toVariantMap());
            if (resolved.value("state") == "resolved") {
                const auto sourceRow = m_outingLapRows[resolved.value("index").toInt()].toMap();
                label = QStringLiteral("%1 · Lap %2").arg(sourceRow.value("runName").toString())
                    .arg(sourceRow.value("lapNumber").toInt());
            }
            row.insert("sourceLapLabel", label);
        } else {
            row.insert("unavailableReason", sector.unavailableReason);
        }
        sectors.append(row);
    }
    result.insert("sectors", sectors);
    if (m_theoreticalBestBest.totalSeconds) result.insert("totalSeconds", *m_theoreticalBestBest.totalSeconds);
    result.insert("revision", m_theoreticalBestBest.stamp.revision);
    result.insert("trackConfigurationReference", m_theoreticalBestBest.stamp.trackConfigurationReference);
    return result;
}

void AppController::requestOutingTheoreticalBest()
{
    if (m_theoreticalBestState == "loading") return;
    ++m_theoreticalBestRequest;
    if (m_theoreticalBestCancellation) m_theoreticalBestCancellation->store(true);
    m_theoreticalBestBest = {};
    if (m_outingComparisonGroupId.isEmpty()) {
        m_theoreticalBestState = QStringLiteral("unavailable");
        m_theoreticalBestMessage = QStringLiteral("Confirm a compatible track configuration before calculating a theoretical best.");
        emit outingTheoreticalBestChanged();
        return;
    }
    QVector<OutingLapRow> population;
    try {
        const auto eligible = eligibleOutingLaps(m_outingRawLapRows, m_outingComparisonGroupId,
            m_outingRunConfigurations, currentProjectObject().value("event").toObject().value("lapExclusions").toArray(),
            m_outingStaleRunIds);
        population.reserve(eligible.size());
        for (const auto *row : eligible) population.append(*row);
    } catch (const std::exception &error) {
        m_theoreticalBestState = QStringLiteral("error");
        m_theoreticalBestMessage = QString::fromUtf8(error.what());
        emit outingTheoreticalBestChanged();
        return;
    }
    if (population.isEmpty()) {
        m_theoreticalBestState = QStringLiteral("unavailable");
        m_theoreticalBestMessage = QStringLiteral("No eligible laps in this group to calculate a theoretical best from.");
        emit outingTheoreticalBestChanged();
        return;
    }
    // Canonical run: the first eligible run (by id, for determinism) whose own
    // approved segmentation is non-empty for this configuration.
    QStringList runOrder;
    for (const auto &row : population) if (!runOrder.contains(row.runId)) runOrder.append(row.runId);
    std::sort(runOrder.begin(), runOrder.end());
    const auto runs = currentProjectObject().value("event").toObject().value("runs").toArray();
    QString canonicalRunId;
    ApprovedSegmentation approved;
    for (const auto &runId : runOrder) {
        QJsonObject run;
        for (const auto &value : runs) if (value.toObject().value("id").toString() == runId) run = value.toObject();
        auto candidate = approvedSegmentation(run.value("trackSegments"), m_outingComparisonGroupId);
        if (candidate.valid && !candidate.revision.isEmpty() && !candidate.segments.isEmpty()) {
            canonicalRunId = runId;
            approved = candidate;
            break;
        }
    }
    if (canonicalRunId.isEmpty()) {
        m_theoreticalBestState = QStringLiteral("unavailable");
        m_theoreticalBestMessage = QStringLiteral(
            "No run in this group has an approved segment review yet. Approve segments for at least one run first.");
        emit outingTheoreticalBestChanged();
        return;
    }
    QHash<QString, QJsonObject> sourcesByRunId;
    for (const auto &value : outingLapSources())
        sourcesByRunId.insert(value.toObject().value("runId").toString(), value.toObject());

    m_theoreticalBestCancellation = std::make_shared<std::atomic_bool>(false);
    m_theoreticalBestState = QStringLiteral("loading");
    m_theoreticalBestMessage.clear();
    emit outingTheoreticalBestChanged();
    m_theoreticalBestWatcher.setFuture(QtConcurrent::run(
        [population, sourcesByRunId, projectPath = m_documentState.projectPath(), approved, canonicalRunId,
            request = m_theoreticalBestRequest, cancellation = m_theoreticalBestCancellation] {
            return computeOutingTheoreticalBest(
                population, sourcesByRunId, projectPath, approved, canonicalRunId, request, cancellation);
        }));
}

AppController::TheoreticalBestResult AppController::computeOutingTheoreticalBest(QVector<OutingLapRow> population,
    const QHash<QString, QJsonObject> sourcesByRunId, const QString projectPath, const ApprovedSegmentation approved,
    const QString canonicalRunId, const quint64 request, const std::shared_ptr<std::atomic_bool> &cancellation)
{
    TheoreticalBestResult result;
    result.request = request;
    const auto cancelled = [cancellation] { return cancellation->load(); };
    try {
        // Grouped by run so the fresh, small cache below only ever needs to
        // hold the run currently being processed: every one of a run's own
        // eligible laps is handled before moving to the next run's file.
        std::sort(population.begin(), population.end(),
            [](const OutingLapRow &a, const OutingLapRow &b) { return a.runId < b.runId; });
        auto cache = std::make_shared<TelemetrySessionCache>();
        const auto rowVariant = [](const OutingLapRow &row) {
            return QVariantMap{{"startTime", row.start}, {"endTime", row.end}, {"lapNumber", row.lapNumber},
                {"reference", row.reference.toVariantMap()}};
        };
        const auto canonical = std::find_if(population.cbegin(), population.cend(),
            [&canonicalRunId](const OutingLapRow &row) { return row.runId == canonicalRunId; });
        if (canonical == population.cend()) throw std::runtime_error("The canonical run has no eligible lap in this population.");
        const auto axisSource = readOutingLapDetail(sourcesByRunId.value(canonicalRunId), projectPath,
            rowVariant(*canonical), request, cancellation, cache, /*deriveReferenceGate=*/true);
        if (!axisSource.session || !axisSource.hasReferenceGate)
            throw std::runtime_error(axisSource.error.isEmpty()
                ? "Could not build a shared track axis from the canonical run." : axisSource.error.toStdString());
        const GeoCoordinate origin{
            (axisSource.referenceGate.endpointA.latitudeDegrees + axisSource.referenceGate.endpointB.latitudeDegrees) / 2.0,
            (axisSource.referenceGate.endpointA.longitudeDegrees + axisSource.referenceGate.endpointB.longitudeDegrees) / 2.0};
        const auto axis = buildProgressAxis(axisSource.referenceTrace, origin, axisSource.referenceGate, cancelled);
        if (!axis.valid) throw std::runtime_error("The shared track axis could not be built from the canonical run's GPS trace.");

        QVector<LapSectorTimes> populationTimes;
        std::shared_ptr<const TelemetrySession> currentSession;
        QString currentRunId;
        for (const auto &row : population) {
            throwIfCancelled(cancelled);
            if (row.runId != currentRunId || !currentSession) {
                const auto detail = readOutingLapDetail(
                    sourcesByRunId.value(row.runId), projectPath, rowVariant(row), request, cancellation, cache,
                    /*deriveReferenceGate=*/false);
                currentSession = detail.session;
                currentRunId = row.runId;
                if (!currentSession) continue; // this lap's recording could not be decoded; skip its contribution
            }
            const auto trace = projectLapTrace(axis, *currentSession, row.start, row.end, cancelled);
            populationTimes.append(
                computeLapSectorTimes(approved, axis.lengthMeters, trace, row.start, row.end, row.reference));
        }
        result.best = computeTheoreticalBest(approved, populationTimes);
    } catch (const OperationCancelled &) {
        result.error = QStringLiteral("Theoretical best calculation was cancelled.");
    } catch (const std::exception &error) {
        result.error = QString::fromUtf8(error.what());
    }
    return result;
}
