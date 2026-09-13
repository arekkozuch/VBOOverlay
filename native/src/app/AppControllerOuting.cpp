#include "app/AppController.h"
#include "project/EventProjectCodec.h"
#include "project/ProjectLimits.h"
#include "telemetry/TelemetrySource.h"

#include <QDateTime>
#include <QTimeZone>
#include <QFileInfo>
#include <QJsonDocument>
#include <QMap>
#include <QtConcurrent>
#include <algorithm>
#include <cmath>
#include <limits>

namespace FlappedEar {

QVariantMap AppController::runTrackConfiguration(const QString &runId) const
{
    for (const auto &value : outingLapSources()) {
        const auto source = value.toObject();
        if (source.value("runId").toString() != runId) continue;
        auto config = source.value("trackConfiguration").toObject();
        config.insert("derivationKey", source.value("derivationKey"));
        return config.toVariantMap();
    }
    return {};
}

bool AppController::confirmRunTrackConfiguration(const QString &runId, const QString &expectedDerivationKey,
    const QString &layoutId, const QString &direction)
{
    const auto current = runTrackConfiguration(runId);
    if (current.isEmpty() || expectedDerivationKey.isEmpty()
        || current.value("derivationKey").toString() != expectedDerivationKey) return false;
    return setRunTrackConfiguration(runId, layoutId.trimmed(), direction);
}

QVariantMap AppController::outingRanking() const
{
    if (projectLoading() || m_outingLapsLoading || m_outingLapRequestedKey != outingLapKey()
        || m_outingLapGeneration != m_sourceGeneration) return {{"state", "loading"}};
    return m_outingRanking;
}

QVariantList AppController::outingCompatibilityGroups() const
{
    if (projectLoading() || m_outingLapsLoading || m_outingLapRequestedKey != outingLapKey()
        || m_outingLapGeneration != m_sourceGeneration) return {};
    return m_outingCompatibilityGroups;
}

QString AppController::outingComparisonGroupId() const
{
    for (const auto &value : outingCompatibilityGroups())
        if (value.toMap().value("id").toString() == m_outingComparisonGroupId) return m_outingComparisonGroupId;
    return {};
}

bool AppController::selectOutingComparisonGroup(const QString &groupId)
{
    if (projectLoading() || m_outingLapsLoading || m_outingLapRequestedKey != outingLapKey()
        || m_outingLapGeneration != m_sourceGeneration) return false;
    if (!groupId.isEmpty()) {
        bool found = false;
        for (const auto &value : m_outingCompatibilityGroups) {
            const auto group = value.toMap();
            found |= group.value("id").toString() == groupId && group.value("resolved").toBool();
        }
        if (!found) return false;
    }
    m_outingComparisonGroupId = groupId;
    refreshOutingCompatibility();
    emit outingLapsChanged();
    return true;
}

void AppController::refreshOutingCompatibility()
{
    if (m_outingLapRequestedKey != outingLapKey() || m_outingLapGeneration != m_sourceGeneration) return;
    if (m_outingCompatibilityDocumentId != m_documentId) {
        m_outingComparisonGroupId.clear(); m_outingCompatibilityDocumentId = m_documentId;
    }
    QHash<QString, QJsonObject> configurations;
    for (const auto &value : outingLapSources()) {
        const auto source = value.toObject();
        configurations.insert(source.value("runId").toString(), source.value("trackConfiguration").toObject());
    }
    QMap<QString, QVariantMap> groups;
    QHash<QString, QVariantList> membersByGroup, eligibleByGroup;
    for (const auto &value : m_outingLapRows) {
        const auto row = value.toMap(); const auto runId = row.value("runId").toString();
        const auto config = configurations.value(runId);
        const auto resolvedId = lapCompatibilityGroupId(config);
        const auto id = resolvedId.isEmpty() ? "unresolved:" + runId : resolvedId;
        auto &group = groups[id];
        if (group.isEmpty()) group = {{"id", id}, {"resolved", !resolvedId.isEmpty()},
            {"configuration", config.toVariantMap()}, {"runName", row.value("runName")},
            {"members", QVariantList{}}, {"eligibleMembers", QVariantList{}}};
        if (row.value("type") != "LAP") continue;
        membersByGroup[id].append(row.value("reference"));
        if (!resolvedId.isEmpty() && row.value("referenceEligible").toBool() && !m_outingStaleRunIds.contains(runId)) {
            eligibleByGroup[id].append(row.value("reference"));
        }
    }
    if (!groups.contains(m_outingComparisonGroupId)
        || !groups.value(m_outingComparisonGroupId).value("resolved").toBool()) m_outingComparisonGroupId.clear();
    const auto referenceConfig = QJsonObject::fromVariantMap(groups.value(m_outingComparisonGroupId).value("configuration").toMap());
    m_outingCompatibilityGroups.clear();
    int number = 0;
    for (auto it = groups.begin(); it != groups.end(); ++it) {
        auto &group = it.value(); const auto config = group.value("configuration").toMap();
        group.insert("members", membersByGroup.value(it.key()));
        group.insert("eligibleMembers", eligibleByGroup.value(it.key()));
        group.insert("lapCount", group.value("members").toList().size());
        group.insert("eligibleLapCount", group.value("eligibleMembers").toList().size());
        const auto label = group.value("resolved").toBool()
            ? QStringLiteral("Group %1 · %2 · %3").arg(++number).arg(config.value("layoutId").toString(),
                config.value("direction") == "clockwise" ? QStringLiteral("Clockwise") : QStringLiteral("Counterclockwise"))
            : QStringLiteral("Unresolved · %1").arg(group.value("runName").toString());
        group.insert("label", label);
        group.insert("summary", QStringLiteral("%1 · %2/%3 eligible laps").arg(label)
            .arg(group.value("eligibleLapCount").toLongLong()).arg(group.value("lapCount").toLongLong()));
        m_outingCompatibilityGroups.append(group);
    }
    m_outingRanking = rankOutingLaps(m_outingRawLapRows, m_outingComparisonGroupId, configurations,
        currentProjectObject().value("event").toObject().value("lapExclusions").toArray(), m_outingStaleRunIds).toVariantMap();
    m_outingRanking.insert("groupLabel", groups.value(m_outingComparisonGroupId).value("label"));
    const auto bestReference = m_outingRanking.value("bestOfDay").toMap().value("reference").toMap();
    QHash<QString, QVariantMap> bestRunReferences;
    for (const auto &value : m_outingRanking.value("runs").toList()) {
        const auto run = value.toMap();
        bestRunReferences.insert(run.value("runId").toString(), run.value("bestLap").toMap().value("reference").toMap());
    }
    for (auto &value : m_outingLapRows) {
        auto row = value.toMap(); const auto runId = row.value("runId").toString();
        const auto config = configurations.value(runId); const auto resolvedId = lapCompatibilityGroupId(config);
        const auto id = resolvedId.isEmpty() ? "unresolved:" + runId : resolvedId;
        const auto issue = row.value("referenceIssue") == "GPS gap" ? LapReferenceIssue::GpsGap
            : row.value("referenceIssue") == "Invalid GPS" ? LapReferenceIssue::InvalidGps : LapReferenceIssue::None;
        auto reasons = lapCompatibilityReasons(config, referenceConfig, issue, row.value("excluded").toBool());
        if (row.value("type") != "LAP") reasons.append("not-timed-lap");
        if (m_outingStaleRunIds.contains(runId)) reasons.append("stale-source");
        QStringList labels;
        for (const auto &reason : reasons) labels.append(lapCompatibilityReasonText(reason));
        row.insert("compatibilityGroupId", id);
        row.insert("compatibilityGroupLabel", groups.value(id).value("label"));
        row.insert("compatibilityResolved", !resolvedId.isEmpty());
        row.insert("compatibilityReasons", reasons);
        row.insert("compatibilityReasonLabels", labels);
        row.insert("comparisonEligible", !referenceConfig.isEmpty() && reasons.isEmpty());
        row.insert("bestOfDay", !bestReference.isEmpty() && row.value("reference").toMap() == bestReference);
        if (id == m_outingComparisonGroupId) {
            const auto runBest = bestRunReferences.value(runId);
            row.insert("bestOfRun", !runBest.isEmpty() && row.value("reference").toMap() == runBest);
        }
        if (m_outingStaleRunIds.contains(runId)) row.insert("bestOfRun", false);
        value = row;
        if (!m_selectedOutingLap.isEmpty() && m_selectedOutingLap.value("reference") == row.value("reference")) {
            m_selectedOutingLap = row; emit outingLapDetailChanged();
        }
    }
}

QJsonObject AppController::activeLapBinding() const
{
    for (auto value : outingLapSources()) {
        auto source = value.toObject();
        if (source.value("runId").toString() != activeRunId()) continue;
        source.insert("sourceRevision", QString::fromLatin1(m_loadedSourceRevision));
        source.remove("reference"); source.remove("name"); source.remove("trackConfiguration");
        return source;
    }
    return {};
}

bool AppController::setOutingLapExcluded(const QVariantMap &referenceMap, bool excluded, const QString &reason)
{
    if (!EventProjectCodec::isEvent(m_projectTemplate) || projectLoading() || exporting()
        || recoveryPending() || m_batchPending
        || m_documentState.pendingAction() != ProjectDocumentState::DestructiveAction::None
        || m_documentState.revision() == std::numeric_limits<quint64>::max()) return false;
    const auto reference = QJsonObject::fromVariantMap(referenceMap);
    if (!validLapReference(reference) || reference.value("type") != "LAP") return false;
    if (excluded && (resolveOutingLapReference(referenceMap).value("state").toString() != "resolved"
        || reason.trimmed().isEmpty() || reason.size() > 256 || reason.contains(QChar::Null))) return false;
    auto project = currentProjectObject();
    auto event = project.value("event").toObject();
    QJsonArray exclusions;
    for (const auto &item : event.value("lapExclusions").toArray())
        if (item.toObject().value("reference").toObject() != reference) exclusions.append(item);
    if (excluded) exclusions.append(QJsonObject{{"reference", reference}, {"reason", reason.trimmed()}});
    if (exclusions == event.value("lapExclusions").toArray()) return true;
    event.insert("lapExclusions", exclusions); project.insert("event", event);
    QString error;
    if (!ProjectLimits::validateProject(project, &error)) return false;
    m_projectTemplate = project;
    markPersistentChange();
    return true;
}

void AppController::refreshLapExclusionPolicy()
{
    const auto event = currentProjectObject().value("event").toObject();
    const auto exclusions = event.value("lapExclusions").toArray();
    applyLapExclusions(m_lapSession, activeLapBinding(), exclusions);
    m_previewRenderContext.setLapSession(m_lapSession);
    emit lapNavigationChanged();
    emit liveValuesChanged();
    if (m_outingLapRequestedKey != outingLapKey() || m_outingLapGeneration != m_sourceGeneration) return;
    const auto reasons = lapExclusionReasons(exclusions);
    QSet<QByteArray> matched;
    QHash<QString, LapSession> runs;
    for (const auto &row : m_outingRawLapRows) {
        if (row.type != LapSectionType::Lap) continue;
        TimedLap lap;
        lap.number = row.lapNumber; lap.startTelemetryTime = row.start; lap.endTelemetryTime = row.end;
        lap.durationSeconds = row.end - row.start; lap.referenceIssue = row.referenceIssue;
        lap.userExclusionReason = reasons.value(lapReferenceKey(row.reference));
        if (!lap.userExclusionReason.isEmpty()) matched.insert(lapReferenceKey(row.reference));
        runs[row.runId].timedLaps.append(lap);
    }
    QHash<QString, int> bestNumbers;
    for (auto it = runs.begin(); it != runs.end(); ++it) {
        recomputeLapRanking(it.value());
        if (it->fastestLapIndex) bestNumbers.insert(it.key(), it->timedLaps[*it->fastestLapIndex].number);
    }
    m_outingLapRows.clear();
    m_outingLapMessages = m_outingSourceMessages;
    const auto unmatched = exclusions.size() - matched.size();
    if (unmatched > 0) m_outingLapMessages.append(QStringLiteral(
        "%1 saved lap exclusion(s) could not be matched to the current recordings; retained without applying.").arg(unmatched));
    for (const auto &row : m_outingRawLapRows) {
        const auto reason = reasons.value(lapReferenceKey(row.reference));
        const QString clock = row.timestampMilliseconds
            ? QDateTime::fromMSecsSinceEpoch(*row.timestampMilliseconds, QTimeZone::UTC).toString("yyyy-MM-dd HH:mm:ss.zzz")
            : QStringLiteral("Time unavailable");
        const QVariantMap item{{"runId", row.runId}, {"runName", row.runName},
            {"type", lapSectionName(row.type)}, {"reference", row.reference.toVariantMap()}, {"lapNumber", row.lapNumber},
            {"startTime", row.start}, {"endTime", row.end}, {"durationSeconds", row.end - row.start}, {"clock", clock},
            {"excluded", !reason.isEmpty()}, {"exclusionReason", reason},
            {"referenceEligible", row.referenceEligible && reason.isEmpty()},
            {"bestOfRun", row.type == LapSectionType::Lap && bestNumbers.value(row.runId, -1) == row.lapNumber},
            {"referenceIssue", row.referenceIssue == LapReferenceIssue::GpsGap ? QStringLiteral("GPS gap")
                : row.referenceIssue == LapReferenceIssue::InvalidGps ? QStringLiteral("Invalid GPS") : QString()},
            {"chronologyKnown", row.timestampMilliseconds.has_value()}};
        m_outingLapRows.append(item);
        if (!m_selectedOutingLap.isEmpty() && m_selectedOutingLap.value("reference") == item.value("reference")) {
            m_selectedOutingLap = item;
            emit outingLapDetailChanged();
        }
    }
    refreshOutingCompatibility();
    emit outingLapsChanged();
}

bool AppController::setRunTrackConfiguration(
    const QString &runId, const QString &layoutId, const QString &direction)
{
    if (!EventProjectCodec::isEvent(m_projectTemplate) || projectLoading() || exporting()
        || recoveryPending() || m_batchPending
        || m_documentState.pendingAction() != ProjectDocumentState::DestructiveAction::None
        || m_documentState.revision() == std::numeric_limits<quint64>::max()) return false;
    auto project = currentProjectObject();
    auto event = project.value("event").toObject();
    auto runs = event.value("runs").toArray();
    for (qsizetype i = 0; i < runs.size(); ++i) {
        auto run = runs[i].toObject();
        if (run.value("id").toString() != runId) continue;
        auto config = EventProjectCodec::trackConfiguration(run);
        config.insert("layoutId", layoutId.isEmpty() ? QJsonValue(QJsonValue::Null) : QJsonValue(layoutId));
        config.insert("direction", direction);
        run.insert("trackConfiguration", config);
        if (run == runs[i].toObject()) return true;
        runs[i] = run; event.insert("runs", runs); project.insert("event", event);
        QString error;
        if (!ProjectLimits::validateProject(project, &error)) return false;
        m_projectTemplate = project;
        markPersistentChange();
        return true;
    }
    return false;
}

QJsonArray AppController::outingLapSources() const
{
    QJsonArray sources;
    const auto project = currentProjectObject();
    for (const auto &value : project.value("event").toObject().value("runs").toArray()) {
        const auto run = value.toObject();
        for (const auto &item : run.value("sources").toObject().value("telemetry").toArray()) {
            const auto source = item.toObject();
            if (source.value("id") != run.value("primaryTelemetrySourceId")) continue;
            sources.append(QJsonObject{{"eventId", project.value("event").toObject().value("id")}, {"runId", run.value("id")}, {"name", run.value("name")},
                {"sourceId", source.value("id")}, {"reference", source.value("reference")},
                {"trackConfiguration", EventProjectCodec::trackConfiguration(run)},
                {"derivationKey", QString::fromLatin1(EventProjectCodec::lapDerivationKey(run))}});
        }
    }
    return sources;
}

QByteArray AppController::outingLapKey() const
{
    return QJsonDocument(QJsonObject{{"document", m_documentId}, {"path", m_documentState.projectPath()},
        {"sources", outingLapSources()}}).toJson(QJsonDocument::Compact);
}

void AppController::initializeOutingLaps()
{
    m_outingLapTimer.setSingleShot(true);
    m_outingLapTimer.setInterval(0);
    const auto schedule = [this] { m_outingLapTimer.start(); };
    connect(this, &AppController::documentStateChanged, this, &AppController::refreshLapExclusionPolicy);
    connect(this, &AppController::documentStateChanged, this, schedule);
    connect(this, &AppController::sourceLoadStateChanged, this, schedule);
    connect(&m_outingLapTimer, &QTimer::timeout, this, &AppController::refreshOutingLaps);
    connect(&m_outingLapWatcher, &QFutureWatcher<OutingLapResult>::finished, this, [this] {
        const auto result = m_outingLapWatcher.future().takeResult();
        if (result.cancelled || result.generation != m_sourceGeneration || result.key != outingLapKey()) {
            m_outingLapRequestedKey.clear();
            m_outingLapTimer.start();
            return;
        }
        m_outingStaleRunIds.clear();
        m_outingRawLapRows = result.rows;
        m_outingSourceMessages = result.messages;
        refreshLapExclusionPolicy();
        m_outingLapsLoading = false;
        emit outingLapsChanged();
    });
}

void AppController::refreshOutingLaps()
{
    const auto key = outingLapKey();
    if (key == m_outingLapRequestedKey && m_sourceGeneration == m_outingLapGeneration) return;
    if (m_outingLapCancellation) m_outingLapCancellation->store(true);
    m_outingLapRows.clear();
    m_outingRawLapRows.clear();
    m_outingRanking = {{"state", "selection-required"}};
    m_outingCompatibilityGroups.clear();
    m_outingSourceMessages.clear();
    m_outingLapMessages.clear();
    const auto sources = outingLapSources();
    m_outingLapsLoading = !sources.isEmpty();
    emit outingLapsChanged();
    // One worker at a time. Its completion schedules the latest source set.
    if (m_outingLapWatcher.isRunning()) return;
    m_outingLapRequestedKey = key;
    m_outingLapGeneration = m_sourceGeneration;
    if (sources.isEmpty()) return;
    m_outingLapCancellation = std::make_shared<std::atomic_bool>(false);
    const auto cancellation = m_outingLapCancellation;
    const auto generation = m_sourceGeneration;
    const auto projectPath = m_documentState.projectPath();
    m_outingLapWatcher.setFuture(QtConcurrent::run([sources, key, generation, projectPath, cancellation] {
        OutingLapResult result;
        result.key = key;
        result.generation = generation;
        const auto cancelled = [cancellation] { return cancellation->load(); };
        qint64 bytes = 0;
        try {
            if (sources.size() > TelemetryImportLimits{}.maximumFiles)
                throw ResourceLimitError("Too many recordings in this outing.");
            for (qsizetype index = 0; index < sources.size(); ++index) {
                throwIfCancelled(cancelled);
                const auto source = sources[index].toObject();
                try {
                    const auto json = source.value("reference").toObject();
                    const ProjectSourceReference reference{json.value("relativePath").toString(),
                        json.value("absolutePath").toString(), json.value("fingerprint").toObject()};
                    const auto path = ProjectSourceReferenceCodec::resolve(reference, projectPath);
                    if (path.isEmpty()) throw std::runtime_error("Recording is missing; locate its source to list laps.");
                    const auto size = QFileInfo(path).size();
                    if (size <= 0 || size > TelemetryImportLimits{}.maximumFileBytes
                        || size > TelemetryImportLimits{}.maximumBatchBytes - bytes)
                        throw ResourceLimitError("Recording exceeds the outing analysis size limit.");
                    bytes += size;
                    const auto contentRevision = TelemetrySource::contentSha256(path, size, cancelled).toHex();
                    const auto session = TelemetrySource::load(path, cancelled);
                    throwIfCancelled(cancelled);
                    const auto fingerprint = ProjectSourceReferenceCodec::telemetryFingerprint(path, session);
                    if (ProjectSourceReferenceCodec::compareFingerprints(reference.fingerprint, fingerprint)
                        != SourceFingerprintMatch::Match)
                        throw std::runtime_error("Source identity changed or is unknown; verify/relink this recording.");
                    const auto gateRevision = source.value("trackConfiguration").toObject().value("gateRevision");
                    if (gateRevision.isString() && gateRevision.toString() != timingGateRevision(session, cancelled))
                        throw std::runtime_error("Timing-gate revision changed; verify this recording before analysis.");
                    const auto laps = deriveSourceLapSession(session, {}, cancelled);
                    auto rows = outingLapRows(session, laps, source.value("runId").toString(),
                        source.value("name").toString(), index, cancelled);
                    if (rows.size() > maximumOutingLapRows - result.rows.size())
                        throw ResourceLimitError("Outing exceeds the 20,000 lap-section limit.");
                    if (TelemetrySource::contentSha256(path, size, cancelled).toHex() != contentRevision)
                        throw std::runtime_error("Recording changed during lap derivation; reload this source.");
                    for (auto &row : rows) {
                        throwIfCancelled(cancelled);
                        row.reference = makeLapReference(row, source.value("eventId").toString(),
                            source.value("sourceId").toString(), contentRevision,
                            source.value("derivationKey").toString().toLatin1());
                        if (row.reference.isEmpty()) throw std::runtime_error("Cannot identify this lap section.");
                    }
                    result.rows.append(rows);
                    if (!recordingTimestamp(session)) result.messages.append(source.value("name").toString()
                        + ": recording date/time unavailable; listed after chronological records in import order.");
                    if (laps.acceptedPasses.isEmpty()) result.messages.append(source.value("name").toString()
                        + ": no reliable start/finish passages; lap type is unknown.");
                } catch (const OperationCancelled &) { throw; }
                catch (const std::exception &error) {
                    result.messages.append(source.value("name").toString() + ": " + QString::fromUtf8(error.what()));
                }
            }
            throwIfCancelled(cancelled);
            sortOutingLaps(result.rows);
        } catch (const OperationCancelled &) { result.cancelled = true; result.rows.clear(); }
        catch (const std::exception &error) { result.rows.clear(); result.messages.append(QString::fromUtf8(error.what())); }
        return result;
    }));
}

void AppController::initializeOutingLapDetail()
{
    m_outingLapDetailTimer.setSingleShot(true);
    m_outingLapDetailTimer.setInterval(0);
    connect(&m_outingLapDetailTimer, &QTimer::timeout, this, &AppController::loadOutingLapDetail);
    const auto invalidate = [this] {
        if (!m_selectedOutingLap.isEmpty() && (m_outingLapDetailKey != outingLapKey()
            || m_outingLapDetailGeneration != m_sourceGeneration)) closeOutingLap();
    };
    connect(this, &AppController::documentStateChanged, this, invalidate);
    connect(this, &AppController::sourceLoadStateChanged, this, invalidate);
    connect(&m_outingLapDetailWatcher, &QFutureWatcher<OutingLapDetailResult>::finished, this, [this] {
        auto result = m_outingLapDetailWatcher.future().takeResult();
        m_outingLapDetailPending = false;
        if (result.request != m_outingLapDetailRequest) {
            if (m_outingLapDetailState == "loading") m_outingLapDetailTimer.start();
            return;
        }
        if (m_selectedOutingLap.isEmpty() || m_outingLapDetailKey != outingLapKey()
            || m_outingLapDetailGeneration != m_sourceGeneration) { closeOutingLap(); return; }
        m_outingLapDetailSession = std::move(result.session);
        m_outingLapDetailGeometry = std::move(result.geometry);
        m_outingLapTrack = std::move(result.track);
        m_outingLapDetailError = result.error;
        if (result.staleReference) {
            m_outingStaleRunIds.insert(m_selectedOutingLap.value("runId").toString());
            refreshOutingCompatibility();
            emit outingLapsChanged();
        }
        m_outingLapDetailState = m_outingLapDetailSession ? "ready" : "error";
        m_outingLapChannels.clear();
        if (m_outingLapDetailSession) {
            if (m_settings.contains("analysis/lapChannels")) {
                const auto preferred = m_settings.value("analysis/lapChannels").toStringList();
                for (const auto &name : preferred) {
                    if (m_outingLapDetailSession->channels.contains(name) && !m_outingLapChannels.contains(name))
                        m_outingLapChannels.append(name);
                    if (m_outingLapChannels.size() == 4) break;
                }
            } else {
                for (const auto *alias : {"speed", "lateralAcceleration", "longitudinalAcceleration"}) {
                    const auto name = m_outingLapDetailSession->aliases.value(alias, alias);
                    if (m_outingLapDetailSession->channels.contains(name) && !m_outingLapChannels.contains(name))
                        m_outingLapChannels.append(name);
                }
            }
        }
        emit outingLapDetailChanged();
        emit outingLapCursorChanged();
    });
}

QVariantMap AppController::resolveOutingLapReference(const QVariantMap &value) const
{
    const auto reference = QJsonObject::fromVariantMap(value);
    const auto result = [](const char *state, const char *reason) {
        return QVariantMap{{"state", state}, {"reason", reason}};
    };
    if (!validLapReference(reference)) return result("invalid", "Malformed or unsupported lap reference.");
    if (reference.value("algorithm").toString() != lapReferenceAlgorithm)
        return result("stale", "Lap derivation algorithm changed.");
    const auto project = currentProjectObject();
    const auto event = project.value("event").toObject();
    if (reference.value("eventId") != event.value("id"))
        return result("stale", "Lap reference belongs to another event.");
    QJsonObject run;
    for (const auto &candidate : event.value("runs").toArray())
        if (candidate.toObject().value("id") == reference.value("runId")) run = candidate.toObject();
    if (run.isEmpty()) return result("stale", "Referenced run no longer exists.");
    if (run.value("primaryTelemetrySourceId") != reference.value("sourceId"))
        return result("stale", "Primary telemetry source changed.");
    if (QString::fromLatin1(EventProjectCodec::lapDerivationKey(run)) != reference.value("derivationKey").toString())
        return result("stale", "Source or track/gate configuration changed.");
    if (m_outingStaleRunIds.contains(reference.value("runId").toString()))
        return result("stale", "Recording content changed since the last lap derivation.");
    // A document edit can precede the refresh timer: never search yesterday's rows.
    if (projectLoading() || m_outingLapsLoading || m_outingLapRequestedKey != outingLapKey()
        || m_outingLapGeneration != m_sourceGeneration)
        return result("loading", "Current lap derivation is not ready.");
    int match = -1;
    bool runAvailable = false;
    for (qsizetype i = 0; i < m_outingLapRows.size(); ++i) {
        const auto row = m_outingLapRows[i].toMap();
        runAvailable |= row.value("runId").toString() == reference.value("runId").toString();
        if (QJsonObject::fromVariantMap(row.value("reference").toMap()) != reference) continue;
        if (match >= 0) return result("stale", "Lap reference is ambiguous in this derivation.");
        match = static_cast<int>(i);
    }
    if (match >= 0) return {{"state", "resolved"}, {"index", match}};
    return runAvailable ? result("stale", "Source content or lap boundaries changed.")
                        : result("unavailable", "Referenced recording has no available lap derivation.");
}

bool AppController::selectOutingLapReference(const QVariantMap &reference)
{
    const auto resolved = resolveOutingLapReference(reference);
    return resolved.value("state").toString() == "resolved" && selectOutingLap(resolved.value("index").toInt());
}

bool AppController::selectOutingLap(int index)
{
    if (index < 0 || index >= m_outingLapRows.size() || m_outingLapsLoading || projectLoading()
        || m_outingLapRequestedKey != outingLapKey() || m_outingLapGeneration != m_sourceGeneration
        || recoveryPending() || m_documentState.pendingAction() != ProjectDocumentState::DestructiveAction::None)
        return false;
    const auto row = m_outingLapRows[index].toMap();
    QJsonObject source;
    for (const auto &value : outingLapSources())
        if (value.toObject().value("runId").toString() == row.value("runId").toString()) source = value.toObject();
    if (source.isEmpty()) return false;
    closeOutingLap();
    m_selectedOutingLap = row;
    m_outingLapDetailSource = source;
    m_outingLapDetailKey = outingLapKey();
    m_outingLapDetailGeneration = m_sourceGeneration;
    m_outingLapCursor = row.value("startTime").toDouble();
    m_outingLapDetailState = "loading";
    m_outingLapDetailTimer.start();
    emit outingLapDetailChanged();
    emit outingLapCursorChanged();
    return true;
}

void AppController::closeOutingLap()
{
    ++m_outingLapDetailRequest;
    if (m_outingLapDetailCancellation) m_outingLapDetailCancellation->store(true);
    m_outingLapDetailTimer.stop();
    m_selectedOutingLap.clear();
    m_outingLapDetailSession.reset();
    m_outingLapDetailGeometry = {};
    m_outingLapTrack.clear();
    m_outingLapChannels.clear();
    m_outingLapDetailState = "idle";
    m_outingLapDetailError.clear();
    emit outingLapDetailChanged();
    emit outingLapCursorChanged();
}

void AppController::loadOutingLapDetail()
{
    // A rapid second click cancels the current parse and waits for it to finish;
    // it cannot create concurrent parsers with multiplied source-memory budgets.
    if (m_selectedOutingLap.isEmpty() || m_outingLapDetailPending) return;
    const auto source = m_outingLapDetailSource;
    const auto projectPath = m_documentState.projectPath();
    const auto start = m_selectedOutingLap.value("startTime").toDouble();
    const auto end = m_selectedOutingLap.value("endTime").toDouble();
    const auto lapReference = QJsonObject::fromVariantMap(m_selectedOutingLap.value("reference").toMap());
    const auto request = m_outingLapDetailRequest;
    m_outingLapDetailCancellation = std::make_shared<std::atomic_bool>(false);
    const auto cancellation = m_outingLapDetailCancellation;
    m_outingLapDetailPending = true;
    m_outingLapDetailWatcher.setFuture(QtConcurrent::run([source, projectPath, start, end, lapReference, request, cancellation] {
        OutingLapDetailResult result; result.request = request;
        const auto cancelled = [cancellation] { return cancellation->load(); };
        try {
            throwIfCancelled(cancelled);
            const auto json = source.value("reference").toObject();
            const ProjectSourceReference reference{json.value("relativePath").toString(),
                json.value("absolutePath").toString(), json.value("fingerprint").toObject()};
            const auto path = ProjectSourceReferenceCodec::resolve(reference, projectPath);
            if (path.isEmpty()) throw std::runtime_error("Recording is missing. Relink its source and try again.");
            const auto bytes = QFileInfo(path).size();
            if (bytes <= 0 || bytes > TelemetryImportLimits{}.maximumFileBytes)
                throw ResourceLimitError("Recording exceeds the analysis size limit.");
            const auto contentRevision = TelemetrySource::contentSha256(path, bytes, cancelled).toHex();
            if (!validLapReference(lapReference)
                || contentRevision != lapReference.value("sourceRevision").toString().toLatin1()) {
                result.staleReference = true;
                throw std::runtime_error("Lap reference is stale: recording content changed. Reload this source.");
            }
            auto session = std::make_shared<TelemetrySession>(TelemetrySource::load(path, cancelled));
            if (TelemetrySource::contentSha256(path, bytes, cancelled).toHex() != contentRevision) {
                result.staleReference = true;
                throw std::runtime_error("Lap reference is stale: recording changed while opening it.");
            }
            throwIfCancelled(cancelled);
            if (ProjectSourceReferenceCodec::compareFingerprints(reference.fingerprint,
                ProjectSourceReferenceCodec::telemetryFingerprint(path, *session)) != SourceFingerprintMatch::Match)
                throw std::runtime_error("Recording changed. Relink its source before opening this lap.");
            if (!std::isfinite(start) || !std::isfinite(end) || start < 0 || end <= start || end > session->duration)
                throw std::runtime_error("Lap range is no longer valid for this recording.");
            // Build map bounds only from this section. Keep gaps as separate
            // polylines; the dynamic marker shares the same normalization.
            const auto latitude = session->sampledSegments("latitude", start, end, 2000);
            TelemetrySession mapSession;
            mapSession.metadata.insert("gpsLongitudeConvention", session->metadata.value("gpsLongitudeConvention"));
            mapSession.aliases = {{"latitude", "lat"}, {"longitude", "lon"}};
            mapSession.channels.insert("lat", {}); mapSession.channels.insert("lon", {});
            auto &lat = mapSession.channels["lat"]; auto &lon = mapSession.channels["lon"];
            QVector<QVector<double>> times;
            for (const auto &segment : latitude) {
                QVector<double> current;
                for (const auto &point : segment) {
                    throwIfCancelled(cancelled);
                    const auto longitude = session->valueAt("longitude", point.x());
                    if (!longitude) {
                        if (!current.isEmpty()) times.append(std::exchange(current, {}));
                        continue;
                    }
                    lat.values.append(static_cast<float>(point.y()));
                    lon.values.append(static_cast<float>(*longitude));
                    current.append(point.x());
                }
                if (!current.isEmpty()) times.append(std::move(current));
            }
            result.geometry = buildTrackGeometry(mapSession, cancelled);
            for (const auto &segment : times) {
                QVariantList points;
                for (const auto time : segment) {
                    throwIfCancelled(cancelled);
                    const auto point = FlappedEar::currentTrackPoint(*session, time, result.geometry);
                    if (point) points.append(QVariantMap{{"x", point->x()}, {"y", point->y()}});
                }
                if (!points.isEmpty()) result.track.append(QVariant::fromValue(points));
            }
            throwIfCancelled(cancelled);
            result.session = std::move(session);
        } catch (const std::exception &error) {
            result.error = QString::fromUtf8(error.what()); result.track.clear(); result.geometry = {};
        }
        return result;
    }));
}

QStringList AppController::outingLapAvailableChannels() const
{
    return m_outingLapDetailSession ? m_outingLapDetailSession->channelNames() : QStringList{};
}

void AppController::setOutingLapChannels(const QStringList &channels)
{
    if (!m_outingLapDetailSession || m_outingLapDetailState != "ready") return;
    QStringList selected;
    for (const auto &name : channels) {
        if (m_outingLapDetailSession->channels.contains(name) && !selected.contains(name))
            selected.append(name);
        if (selected.size() == 4) break;
    }
    // Analysis preferences do not alter the editor document or recording data.
    m_settings.setValue("analysis/lapChannels", selected);
    if (selected == m_outingLapChannels) return;
    m_outingLapChannels = selected;
    emit outingLapDetailChanged();
}

QVariantMap AppController::outingLapSeries(const QString &channel, int maximumPoints) const
{
    if (!m_outingLapDetailSession || maximumPoints < 2) return {};
    return sessionSeries(*m_outingLapDetailSession, channel,
        m_selectedOutingLap.value("startTime").toDouble(), m_selectedOutingLap.value("endTime").toDouble(), maximumPoints);
}

QString AppController::outingLapValueText(const QString &channel) const
{
    const auto value = m_outingLapDetailSession ? m_outingLapDetailSession->valueAt(channel, m_outingLapCursor) : std::nullopt;
    return value ? QString::number(*value, 'f', 2) : QStringLiteral("—");
}

QVariantMap AppController::outingLapTrackPoint() const
{
    if (!m_outingLapDetailSession) return {};
    const auto point = FlappedEar::currentTrackPoint(*m_outingLapDetailSession, m_outingLapCursor, m_outingLapDetailGeometry);
    return point ? QVariantMap{{"x", point->x()}, {"y", point->y()}} : QVariantMap{};
}

void AppController::setOutingLapCursor(double seconds)
{
    if (!m_outingLapDetailSession || !std::isfinite(seconds)) return;
    seconds = std::clamp(seconds, m_selectedOutingLap.value("startTime").toDouble(), m_selectedOutingLap.value("endTime").toDouble());
    if (seconds == m_outingLapCursor) return;
    m_outingLapCursor = seconds;
    emit outingLapCursorChanged();
}

} // namespace FlappedEar
