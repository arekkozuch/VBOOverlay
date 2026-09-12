#include "app/AppController.h"
#include "telemetry/TelemetrySource.h"

#include <QDateTime>
#include <QTimeZone>
#include <QFileInfo>
#include <QJsonDocument>
#include <QtConcurrent>

namespace FlappedEar {

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
                {"sourceId", source.value("id")}, {"reference", source.value("reference")}});
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

} // namespace FlappedEar
