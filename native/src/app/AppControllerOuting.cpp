#include "app/AppController.h"
#include "project/EventProjectCodec.h"
#include "project/ProjectLimits.h"
#include "telemetry/TelemetrySource.h"

#include <QDateTime>
#include <QTimeZone>
#include <QFileInfo>
#include <QJsonDocument>
#include <QtConcurrent>
#include <algorithm>
#include <cmath>
#include <limits>

namespace FlappedEar {

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
            sources.append(QJsonObject{{"runId", run.value("id")}, {"name", run.value("name")},
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
        m_outingLapRows.clear();
        m_outingLapMessages = result.messages;
        for (const auto &row : result.rows) {
            const QString type = row.type == LapSectionType::Out ? "OUT" : row.type == LapSectionType::In ? "IN"
                : row.type == LapSectionType::Lap ? "LAP" : "UNKNOWN";
            const QString clock = row.timestampMilliseconds
                ? QDateTime::fromMSecsSinceEpoch(*row.timestampMilliseconds, QTimeZone::UTC).toString("yyyy-MM-dd HH:mm:ss.zzz")
                : QStringLiteral("Time unavailable");
            m_outingLapRows.append(QVariantMap{{"runId", row.runId}, {"runName", row.runName},
                {"type", type}, {"lapNumber", row.lapNumber}, {"startTime", row.start},
                {"endTime", row.end}, {"durationSeconds", row.end - row.start}, {"clock", clock},
                {"referenceEligible", row.referenceEligible}, {"bestOfRun", row.bestOfRun},
                {"referenceIssue", row.referenceIssue == LapReferenceIssue::GpsGap ? QStringLiteral("GPS gap")
                    : row.referenceIssue == LapReferenceIssue::InvalidGps ? QStringLiteral("Invalid GPS") : QString()},
                {"chronologyKnown", row.timestampMilliseconds.has_value()}});
        }
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
                    const auto rows = outingLapRows(session, laps, source.value("runId").toString(),
                        source.value("name").toString(), index, cancelled);
                    if (rows.size() > maximumOutingLapRows - result.rows.size())
                        throw ResourceLimitError("Outing exceeds the 20,000 lap-section limit.");
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

bool AppController::selectOutingLap(int index)
{
    if (index < 0 || index >= m_outingLapRows.size() || m_outingLapsLoading || projectLoading()
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
    const auto request = m_outingLapDetailRequest;
    m_outingLapDetailCancellation = std::make_shared<std::atomic_bool>(false);
    const auto cancellation = m_outingLapDetailCancellation;
    m_outingLapDetailPending = true;
    m_outingLapDetailWatcher.setFuture(QtConcurrent::run([source, projectPath, start, end, request, cancellation] {
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
            auto session = std::make_shared<TelemetrySession>(TelemetrySource::load(path, cancelled));
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
