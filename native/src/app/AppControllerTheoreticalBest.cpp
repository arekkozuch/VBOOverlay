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

#include <QCryptographicHash>
#include <QJsonDocument>
#include <QtConcurrent/QtConcurrentRun>

#include <algorithm>

using namespace FlappedEar;

namespace {
QString theoreticalBestReasonText(const QString &reason)
{
    if (reason == theoreticalBestNoApprovedSegmentation)
        return QStringLiteral("No approved segments to measure sectors against.");
    if (reason == theoreticalBestIncompleteCoverage)
        return QStringLiteral("At least one sector has no fully covered time on any eligible lap, so no total is shown.");
    return reason;
}
}

void AppController::initializeOutingTheoreticalBest()
{
    connect(&m_theoreticalBestWatcher, &QFutureWatcher<TheoreticalBestResult>::finished, this, [this] {
        auto result = m_theoreticalBestWatcher.future().takeResult();
        if (result.request != m_theoreticalBestRequest) return; // stale: outing laps changed or a new request started
        m_theoreticalBestBest = {};
        m_theoreticalBestActual.reset();
        m_theoreticalBestPopulation.clear();
        if (!result.error.isEmpty()) {
            m_theoreticalBestState = QStringLiteral("error");
            m_theoreticalBestMessage = result.error;
        } else if (!result.best.valid) {
            m_theoreticalBestState = QStringLiteral("unavailable");
            m_theoreticalBestMessage = theoreticalBestReasonText(result.best.unavailableReason);
        } else {
            m_theoreticalBestState = QStringLiteral("ready");
            m_theoreticalBestMessage = result.best.totalSeconds
                ? QString() : theoreticalBestReasonText(result.best.unavailableReason);
            m_theoreticalBestBest = std::move(result.best);
            m_theoreticalBestActual = std::move(result.actualBest);
            m_theoreticalBestCanonicalRunId = result.canonicalRunId;
            m_theoreticalBestPopulation = std::move(result.population);
            m_theoreticalBestApproved = std::move(result.approved);
            m_theoreticalBestAxisLength = result.axisLengthMeters;
            m_theoreticalBestAxis = std::move(result.axis);
        }
        emit outingTheoreticalBestChanged();
    });
    // The eligible population, its approved segmentation or the document
    // itself can change under an in-flight or displayed result; never show a
    // theoretical best computed from laps that no longer apply.
    const auto invalidate = [this] {
        if (m_theoreticalBestState == "idle") return;
        if (!m_theoreticalBestKey.isEmpty() && theoreticalBestInputKey() == m_theoreticalBestKey) return;
        ++m_theoreticalBestRequest;
        if (m_theoreticalBestCancellation) m_theoreticalBestCancellation->store(true);
        m_theoreticalBestState = QStringLiteral("idle");
        m_theoreticalBestMessage.clear();
        m_theoreticalBestBest = {};
        m_theoreticalBestActual.reset();
        m_theoreticalBestPopulation.clear();
        emit outingTheoreticalBestChanged();
    };
    connect(this, &AppController::outingLapsChanged, this, invalidate);
    connect(this, &AppController::documentStateChanged, this, invalidate);
}

QByteArray AppController::theoreticalBestInputKey() const
{
    const auto event = currentProjectObject().value("event").toObject();
    QJsonArray runs;
    for (const auto &value : event.value("runs").toArray()) {
        const auto run = value.toObject();
        runs.append(QJsonObject{{"id", run.value("id")}, {"trackSegments", run.value("trackSegments")},
            {"trackConfiguration", run.value("trackConfiguration")}});
    }
    QJsonArray population;
    try {
        for (const auto *row : eligibleOutingLaps(m_outingRawLapRows, m_outingComparisonGroupId,
                 m_outingRunConfigurations, event.value("lapExclusions").toArray(), m_outingStaleRunIds))
            population.append(row->reference);
    } catch (const std::exception &) {
        population = {};
    }
    const QJsonObject key{{"group", m_outingComparisonGroupId}, {"runs", runs}, {"population", population},
        {"exclusions", event.value("lapExclusions")}, {"best", QJsonObject::fromVariantMap(
            m_outingRanking.value("bestOfDay").toMap().value("reference").toMap())}};
    return QCryptographicHash::hash(QJsonDocument(key).toJson(QJsonDocument::Compact), QCryptographicHash::Sha256);
}

QString AppController::outingLapLabel(const QJsonObject &reference) const
{
    const auto resolved = resolveOutingLapReference(reference.toVariantMap());
    if (resolved.value("state") != "resolved") return {};
    const auto row = m_outingLapRows[resolved.value("index").toInt()].toMap();
    return QStringLiteral("%1 · LAP %2").arg(row.value("runName").toString()).arg(row.value("lapNumber").toInt());
}

QVariantMap AppController::outingTheoreticalBest() const
{
    QVariantMap result{{"state", m_theoreticalBestState}, {"message", m_theoreticalBestMessage}};
    if (m_theoreticalBestState != "ready") return result;
    result.insert("algorithm", QString::fromLatin1(theoreticalBestAlgorithm));
    const auto *actual = m_theoreticalBestActual ? &*m_theoreticalBestActual : nullptr;
    QVariantList sectors;
    double actualSum = 0.0;
    bool actualComplete = actual != nullptr;
    for (const auto &sector : m_theoreticalBestBest.sectors) {
        QVariantMap row{{"segmentId", sector.segmentId}, {"name", sector.name}, {"type", sector.type}};
        if (sector.seconds) {
            row.insert("seconds", *sector.seconds);
            row.insert("sourceLapLabel", outingLapLabel(sector.sourceLapReference));
            row.insert("sourceLapReference", sector.sourceLapReference.toVariantMap());
        } else {
            row.insert("unavailableReason", theoreticalBestReasonText(sector.unavailableReason));
        }
        std::optional<double> actualSeconds;
        if (actual) {
            for (const auto &candidate : actual->sectors)
                if (candidate.segmentId == sector.segmentId) actualSeconds = candidate.seconds;
        }
        if (actualSeconds) {
            row.insert("actualSeconds", *actualSeconds);
            actualSum += *actualSeconds;
            if (sector.seconds) row.insert("lossSeconds", *actualSeconds - *sector.seconds);
        } else {
            actualComplete = false;
        }
        sectors.append(row);
    }
    result.insert("sectors", sectors);
    if (m_theoreticalBestBest.totalSeconds) result.insert("totalSeconds", *m_theoreticalBestBest.totalSeconds);
    if (actual) {
        QVariantMap best{{"label", outingLapLabel(actual->lapReference)},
            {"reference", actual->lapReference.toVariantMap()}, {"lapSeconds", actual->lapSeconds},
            {"coversWholeLap", actual->completePartition}};
        // Over the same sectors, so a partition with gaps or a gate-crossing
        // segment still compares like with like.
        if (actualComplete) best.insert("sectorSumSeconds", actualSum);
        result.insert("actualBest", best);
        if (actualComplete && m_theoreticalBestBest.totalSeconds)
            result.insert("differenceSeconds", actualSum - *m_theoreticalBestBest.totalSeconds);
    }
    result.insert("revision", m_theoreticalBestBest.stamp.revision);
    result.insert("trackConfigurationReference", m_theoreticalBestBest.stamp.trackConfigurationReference);

    // KAN-120: where the best lap loses time, largest first ...
    auto gains = sectors;
    std::stable_sort(gains.begin(), gains.end(), [](const QVariant &left, const QVariant &right) {
        return left.toMap().value("lossSeconds", -1.0).toDouble() > right.toMap().value("lossSeconds", -1.0).toDouble();
    });
    result.insert("gains", gains);
    // ... and each segment's line on the track map (north up, fitted to a
    // unit square with the aspect ratio kept).
    const auto &axis = m_theoreticalBestAxis;
    if (axis.valid && axis.points.size() == axis.cumulative.size() && !axis.points.isEmpty()) {
        double minX = axis.points.first().x(), maxX = minX, minY = axis.points.first().y(), maxY = minY;
        for (const auto &point : axis.points) {
            minX = std::min(minX, point.x()); maxX = std::max(maxX, point.x());
            minY = std::min(minY, point.y()); maxY = std::max(maxY, point.y());
        }
        const double span = std::max({maxX - minX, maxY - minY, 1.0});
        const double offsetX = (span - (maxX - minX)) / 2.0, offsetY = (span - (maxY - minY)) / 2.0;
        const auto normalized = [&](const QPointF &point) {
            return QVariantMap{{"x", (point.x() - minX + offsetX) / span}, {"y", (maxY - point.y() + offsetY) / span}};
        };
        const auto line = [&](const double from, const double to) {
            QVariantList points;
            for (qsizetype i = 0; i < axis.points.size(); ++i)
                if (axis.cumulative[i] >= from - 1e-6 && axis.cumulative[i] <= to + 1e-6) points.append(normalized(axis.points[i]));
            return points;
        };
        QVariantList mapSegments;
        for (const auto &value : sectors) {
            auto row = value.toMap();
            const auto id = row.value("segmentId").toString();
            double start = 0.0, end = 0.0;
            for (const auto &segmentValue : m_theoreticalBestApproved.segments) {
                const auto segment = segmentValue.toObject();
                if (segment.value("id").toString() != id) continue;
                start = segment.value("startProgressMeters").toDouble();
                end = segment.value("endProgressMeters").toDouble();
            }
            QVariantList parts;
            if (end >= start) parts.append(QVariant(line(start, end)));
            else { parts.append(QVariant(line(start, axis.lengthMeters))); parts.append(QVariant(line(0.0, end))); }
            row.insert("parts", parts);
            mapSegments.append(row);
        }
        QVariantList outline;
        for (qsizetype i = 0; i < axis.points.size(); i += std::max<qsizetype>(1, axis.points.size() / 800))
            outline.append(normalized(axis.points[i]));
        result.insert("map", QVariantMap{{"segments", mapSegments}, {"outline", outline}});
    }
    return result;
}

QVariantMap AppController::outingTimeLossRanking() const
{
    QVariantMap result{{"state", m_theoreticalBestState}, {"message", m_theoreticalBestMessage}};
    if (m_theoreticalBestState != "ready") return result;
    if (!m_theoreticalBestActual) {
        result.insert("state", QStringLiteral("unavailable"));
        result.insert("message", QStringLiteral("The group's best lap could not be timed against the approved segments."));
        return result;
    }
    const TimedLapSectors reference{*m_theoreticalBestActual,
        m_theoreticalBestActual->lapReference.value("startTime").toDouble()};
    QVector<TimedLapSectors> compared;
    if (m_timeLossAllLaps) {
        compared = m_theoreticalBestPopulation;
    } else {
        QHash<QString, qsizetype> fastestByRun;
        for (qsizetype i = 0; i < m_theoreticalBestPopulation.size(); ++i) {
            const auto &lap = m_theoreticalBestPopulation[i];
            const auto runId = lap.times.lapReference.value("runId").toString();
            const auto it = fastestByRun.constFind(runId);
            if (it == fastestByRun.cend() || lap.times.lapSeconds < m_theoreticalBestPopulation[*it].times.lapSeconds)
                fastestByRun.insert(runId, i);
        }
        for (const auto index : fastestByRun) compared.append(m_theoreticalBestPopulation[index]);
    }
    const auto ranking = rankTimeLosses(m_theoreticalBestApproved, m_theoreticalBestAxisLength, compared, reference);
    if (!ranking.valid) {
        result.insert("state", QStringLiteral("unavailable"));
        result.insert("message", ranking.unavailableReason);
        return result;
    }
    QVariantList losses;
    for (const auto &loss : ranking.losses) {
        const auto &window = loss.window;
        QVariantMap row{{"lossSeconds", loss.lossSeconds}, {"segmentId", window.segmentId}, {"name", window.name},
            {"type", window.type}, {"role", window.role}, {"startMeters", window.startProgressMeters},
            {"endMeters", window.endProgressMeters}, {"lapReference", loss.lapReference.toVariantMap()},
            {"lapLabel", outingLapLabel(loss.lapReference)}, {"coverageLap", loss.coverageLap},
            {"coverageReference", loss.coverageReference}};
        if (!window.cornerSegmentId.isEmpty()) {
            row.insert("cornerSegmentId", window.cornerSegmentId);
            for (const auto &candidate : m_theoreticalBestBest.sectors)
                if (candidate.segmentId == window.cornerSegmentId) row.insert("cornerName", candidate.name);
        }
        if (window.cumulativeAtStartSeconds) row.insert("cumulativeAtStartSeconds", *window.cumulativeAtStartSeconds);
        if (window.cumulativeAtEndSeconds) row.insert("cumulativeAtEndSeconds", *window.cumulativeAtEndSeconds);
        losses.append(row);
    }
    result.insert("losses", losses);
    result.insert("algorithm", QString::fromLatin1(timeLossAlgorithm));
    result.insert("referenceLabel", outingLapLabel(ranking.referenceLap));
    result.insert("referenceLap", ranking.referenceLap.toVariantMap());
    result.insert("scope", m_timeLossAllLaps ? QStringLiteral("allLaps") : QStringLiteral("runBests"));
    result.insert("observationCount", ranking.observationCount);
    result.insert("comparedLapCount", ranking.comparedLapCount);
    result.insert("untimedWindowCount", ranking.untimedWindowCount);
    result.insert("revision", ranking.stamp.revision);
    return result;
}

void AppController::setOutingTimeLossAllLaps(const bool allLaps)
{
    if (m_timeLossAllLaps == allLaps) return;
    m_timeLossAllLaps = allLaps;
    emit outingTheoreticalBestChanged();
}

bool AppController::openTheoreticalBestSector(const QString &segmentId)
{
    if (m_theoreticalBestState != "ready" || !m_theoreticalBestActual) return false;
    const auto sector = std::find_if(m_theoreticalBestBest.sectors.cbegin(), m_theoreticalBestBest.sectors.cend(),
        [&segmentId](const TheoreticalBestSector &candidate) { return candidate.segmentId == segmentId; });
    if (sector == m_theoreticalBestBest.sectors.cend() || !sector->seconds) return false;
    // Donor lap as A against the group's actual best as B. When the donor is
    // the actual best, comparing it with itself shows nothing: B is then the
    // next-fastest lap through this sector (KAN-117).
    auto against = m_theoreticalBestActual->lapReference;
    if (against == sector->sourceLapReference) {
        std::optional<double> nextBest;
        QJsonObject nextLap;
        for (const auto &lap : m_theoreticalBestPopulation) {
            if (lap.times.lapReference == sector->sourceLapReference) continue;
            for (const auto &candidate : lap.times.sectors) {
                if (candidate.segmentId != segmentId || !candidate.seconds) continue;
                if (!nextBest || *candidate.seconds < *nextBest) { nextBest = candidate.seconds; nextLap = lap.times.lapReference; }
            }
        }
        if (nextLap.isEmpty()) return false;
        against = nextLap;
    }
    return openComparisonEvidence(sector->sourceLapReference.toVariantMap(), against.toVariantMap(), segmentId);
}

bool AppController::openTimeLoss(const QVariantMap &loss)
{
    if (m_theoreticalBestState != "ready" || !m_theoreticalBestActual) return false;
    const auto segmentId = loss.value("segmentId").toString();
    const auto known = std::any_of(m_theoreticalBestBest.sectors.cbegin(), m_theoreticalBestBest.sectors.cend(),
        [&segmentId](const TheoreticalBestSector &sector) { return sector.segmentId == segmentId; });
    if (!known) return false;
    return openComparisonEvidence(loss.value("lapReference").toMap(),
        m_theoreticalBestActual->lapReference.toVariantMap(), segmentId);
}

bool AppController::openComparisonEvidence(const QVariantMap &lapA, const QVariantMap &lapB, const QString &segmentId)
{
    closeOutingLap();
    if (!selectComparisonLap(0, lapA) || !selectComparisonLap(1, lapB)) return false;
    // Measured against the same canonical segments as the result that led here.
    m_comparisonSegmentationRunId = m_theoreticalBestCanonicalRunId;
    if (m_comparisonFocusSegmentId != segmentId) {
        m_comparisonFocusSegmentId = segmentId;
        emit comparisonFocusSegmentIdChanged();
    }
    setComparisonViewOpen(true);
    return true;
}

void AppController::clearComparisonFocusSegment()
{
    if (m_comparisonFocusSegmentId.isEmpty()) return;
    m_comparisonFocusSegmentId.clear();
    emit comparisonFocusSegmentIdChanged();
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

    m_theoreticalBestKey = theoreticalBestInputKey();
    m_theoreticalBestCancellation = std::make_shared<std::atomic_bool>(false);
    m_theoreticalBestState = QStringLiteral("loading");
    m_theoreticalBestMessage.clear();
    emit outingTheoreticalBestChanged();
    const auto actualBestReference = QJsonObject::fromVariantMap(
        m_outingRanking.value("bestOfDay").toMap().value("reference").toMap());
    m_theoreticalBestWatcher.setFuture(QtConcurrent::run(
        [population, sourcesByRunId, projectPath = m_documentState.projectPath(), approved, canonicalRunId,
            actualBestReference, request = m_theoreticalBestRequest, cancellation = m_theoreticalBestCancellation] {
            return computeOutingTheoreticalBest(population, sourcesByRunId, projectPath, approved, canonicalRunId,
                actualBestReference, request, cancellation);
        }));
}

AppController::TheoreticalBestResult AppController::computeOutingTheoreticalBest(QVector<OutingLapRow> population,
    const QHash<QString, QJsonObject> sourcesByRunId, const QString projectPath, const ApprovedSegmentation approved,
    const QString canonicalRunId, const QJsonObject actualBestReference, const quint64 request,
    const std::shared_ptr<std::atomic_bool> &cancellation)
{
    TheoreticalBestResult result;
    result.request = request;
    result.canonicalRunId = canonicalRunId;
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
        // The axis comes from the canonical run's fastest lap: the lap usually
        // reviewed, so segment bounds line up with the axis they were approved on.
        auto canonical = population.cend();
        for (auto it = population.cbegin(); it != population.cend(); ++it) {
            if (it->runId != canonicalRunId) continue;
            if (canonical == population.cend() || it->end - it->start < canonical->end - canonical->start) canonical = it;
        }
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
            result.population.append({populationTimes.last(), row.start});
            if (!actualBestReference.isEmpty() && row.reference == actualBestReference)
                result.actualBest = populationTimes.last();
        }
        result.best = computeTheoreticalBest(approved, populationTimes);
        result.approved = approved;
        result.axisLengthMeters = axis.lengthMeters;
        result.axis = axis;
    } catch (const OperationCancelled &) {
        result.error = QStringLiteral("Theoretical best calculation was cancelled.");
    } catch (const std::exception &error) {
        result.error = QString::fromUtf8(error.what());
    }
    return result;
}
