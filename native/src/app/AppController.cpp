#include "app/AppController.h"
#include "export/ExportFormat.h"
#include "export/ExportEngine.h"
#include "app/AppLog.h"

#include "gopro/GoProTelemetrySource.h"
#include "export/ExportArtifactManifest.h"
#include "sync/TelemetrySyncEngine.h"
#include "telemetry/VboParser.h"

#include <QFileInfo>
#include <QClipboard>
#include <QGuiApplication>
#include <QFontDatabase>
#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QDateTime>
#include <QScopedValueRollback>
#include <QTimer>
#include <QtConcurrent>
#include <QtGlobal>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <utility>

namespace FlappedEar {

AppController::AppController(QObject *parent)
    : QObject(parent)
    , m_settings()
    , m_previewRenderContext(this)
{
    m_sync.offset = m_settings.value("sync/offset", 0.0).toDouble();
    m_sync.timeScale = m_settings.value("sync/timeScale", 1.0).toDouble();
    m_previewRenderContext.setSyncTransform(m_sync);
    m_analysisChannels = m_settings.value("analysis/channels").toStringList();
    m_analysisVisible = m_settings.value("analysis/visible", true).toBool();
    const QJsonDocument savedWidgets =
        QJsonDocument::fromJson(m_settings.value("editor/widgets").toByteArray());
    if (!savedWidgets.isObject() || savedWidgets.object().value("schemaVersion").toInt() != 2
        || !m_widgetModel.fromJson(savedWidgets.object().value("widgets").toArray())) {
        m_widgetModel.resetDefaults();
    }
    connect(&m_widgetModel, &WidgetModel::revisionChanged, this, [this] {
        saveWidgetSettings();
        markPersistentChange();
    });
    connect(&m_syncWatcher, &QFutureWatcher<AutoSyncResult>::finished, this, [this] {
        const AutoSyncResult result = m_syncWatcher.result();
        emit syncingChanged();
        if (result.generation != m_sourceGeneration
            || result.videoPath != normalizedSourcePath(m_videoSource.toLocalFile())
            || result.vboPath != normalizedSourcePath(m_telemetryPath)) {
            AppLog::warn(QStringLiteral("Stale auto-sync result rejected"));
            return;
        }
        if (!result.success) {
            AppLog::error(QStringLiteral("Auto-sync failed: %1").arg(result.error));
            m_syncCandidate.clear();
            emit syncCandidateChanged();
            setStatus(QStringLiteral("Auto sync failed: %1").arg(result.error));
            return;
        }
        const bool automaticallyApplied = shouldAutoApplySyncCandidate(result.candidate);
        if (automaticallyApplied) {
            setSyncOffset(result.candidate.offset);
            setTimeScale(result.candidate.timeScale);
        }
        m_syncCandidate = {
            {"offset", result.candidate.offset},
            {"confidence", result.candidate.confidence},
            {"correlation", result.candidate.diagnostics.correlation},
            {"peakUniqueness", result.candidate.diagnostics.peakUniqueness},
            {"validSamples", result.candidate.diagnostics.validSamples},
            {"coarseOffset", result.candidate.diagnostics.coarseOffset},
            {"packetCount", result.packetCount},
            {"gpsSampleCount", result.gpsSampleCount},
            {"gpsStream", result.gpsStream},
            {"level", syncCandidateLevelName(result.candidate.confidence)},
            {"automaticallyApplied", automaticallyApplied},
            {"canApply", true},
        };
        emit syncCandidateChanged();
        AppLog::info(QStringLiteral("Auto-sync result: offset=%1 s, confidence=%2%, %3")
                         .arg(result.candidate.offset, 0, 'f', 3)
                         .arg(result.candidate.confidence * 100.0, 0, 'f', 0)
                         .arg(automaticallyApplied ? QStringLiteral("applied")
                                                   : QStringLiteral("review required")));
        setStatus(QStringLiteral("Auto sync %1: %2 s · correlation %3 · confidence %4% · %5 GPS samples")
                      .arg(automaticallyApplied ? QStringLiteral("applied")
                                                : QStringLiteral("candidate requires review"))
                      .arg(result.candidate.offset, 0, 'f', 3)
                      .arg(result.candidate.diagnostics.correlation, 0, 'f', 3)
                      .arg(result.candidate.confidence * 100.0, 0, 'f', 0)
                      .arg(result.gpsSampleCount));
    });
    connect(&m_videoProbeWatcher, &QFutureWatcher<VideoProbeResult>::finished, this, [this] {
        const VideoProbeResult result = m_videoProbeWatcher.result();
        if (result.generation != m_sourceGeneration) {
            AppLog::warn(QStringLiteral("Stale video probe result rejected: %1").arg(result.path));
            return;
        }
        if (!result.success) {
            AppLog::error(QStringLiteral("Video load failed: %1: %2").arg(result.path, result.error));
            m_videoLoadState = QStringLiteral("error");
            emit sourceLoadStateChanged();
            setStatus(QStringLiteral("Could not open video: %1\n%2").arg(result.path, result.error));
            return;
        }
        commitVideoProbe(result, m_videoLoadMarksDocumentDirty);
    });
    connect(&m_vboLoadWatcher, &QFutureWatcher<VboLoadResult>::finished, this, [this] {
        const VboLoadResult result = m_vboLoadWatcher.result();
        if (result.generation != m_sourceGeneration) {
            AppLog::warn(QStringLiteral("Stale VBO load result rejected: %1").arg(result.path));
            return;
        }
        if (!result.success) {
            AppLog::error(QStringLiteral("VBO load failed: %1: %2").arg(result.path, result.error));
            m_vboLoadState = QStringLiteral("error");
            emit sourceLoadStateChanged();
            setStatus(QStringLiteral("Could not parse VBO: %1\n%2").arg(result.path, result.error));
            return;
        }
        commitVboLoad(result, m_vboLoadMarksDocumentDirty);
    });
    connect(&m_projectLoadWatcher, &QFutureWatcher<ProjectLoadResult>::finished, this, [this] {
        const ProjectLoadResult result = m_projectLoadWatcher.result();
        if (result.generation != m_sourceGeneration) {
            AppLog::warn(QStringLiteral("Stale project load result rejected: %1")
                             .arg(result.projectPath));
            return;
        }
        if (!result.success) {
            AppLog::error(QStringLiteral("Project load failed: %1: %2")
                              .arg(result.projectPath, result.error));
            setProjectLoadState(false, {}, result.error);
            setStatus(QStringLiteral("Project could not be opened: %1").arg(result.error));
            return;
        }
        commitProjectLoad(result);
    });
    restoreSources();
    const QString restoredProjectPath = m_settings.value("project/path").toString();
    m_documentState.reset(QFileInfo::exists(restoredProjectPath) ? restoredProjectPath : QString());
}

AppController::~AppController()
{
    if (exporting()) {
        QFile cancellationFile(m_exportCancelPath);
        if (cancellationFile.open(QIODevice::WriteOnly)) {
            cancellationFile.close();
        }
        if (m_exportSupervisor) static_cast<void>(m_exportSupervisor->stopAndWait());
    }
    m_exportOutputTransaction.reset();
    // On abnormal destruction the manifest intentionally remains for startup
    // recovery. A normal finished callback performs the authorized cleanup.
    if (!exporting() && !m_exportCancelPath.isEmpty()) QFile::remove(m_exportCancelPath);
}

QUrl AppController::videoSource() const { return m_videoSource; }
QString AppController::videoName() const { return QFileInfo(m_videoSource.toLocalFile()).fileName(); }
QString AppController::telemetryName() const { return QFileInfo(m_telemetryPath).fileName(); }
QString AppController::statusText() const { return m_statusText; }
QStringList AppController::channelNames() const { return m_session ? m_session->channelNames() : QStringList(); }
qsizetype AppController::sampleCount() const { return m_session ? m_session->sampleCount : 0; }
double AppController::telemetryDuration() const { return m_session ? m_session->duration : 0.0; }
double AppController::playbackTime() const { return m_playbackTime; }
double AppController::syncOffset() const { return m_sync.offset; }
double AppController::timeScale() const { return m_sync.timeScale; }
bool AppController::syncing() const { return m_syncWatcher.isRunning(); }
bool AppController::exporting() const { return m_exportProcess && m_exportProcess->state() != QProcess::NotRunning; }
int AppController::exportProgress() const { return m_exportProgress; }
QString AppController::exportState() const { return m_exportState; }
QString AppController::exportError() const { return m_exportError; }
QVariantMap AppController::exportSourceInfo() const
{
    if (!m_exportSourceInfo.videoSize.isValid()) {
        return {};
    }
    const MediaRational rate = m_exportSourceInfo.averageFrameRate.isValid()
        ? m_exportSourceInfo.averageFrameRate
        : m_exportSourceInfo.frameRate;
    return {
        {"width", m_exportSourceInfo.videoSize.width()},
        {"height", m_exportSourceInfo.videoSize.height()},
        {"duration", m_exportSourceInfo.duration},
        {"frameRate", rate.value()},
        {"frameRateText", QStringLiteral("%1/%2 (%3 fps)")
                              .arg(rate.numerator)
                              .arg(rate.denominator)
                              .arg(rate.value(), 0, 'f', 3)},
        {"videoCodec", m_exportSourceInfo.videoCodec},
        {"audioCodecs", m_exportSourceInfo.audioCodecs.join(QStringLiteral(", "))},
        {"likelyVariableFrameRate", m_exportSourceInfo.likelyVariableFrameRate},
    };
}
QVariantMap AppController::exportFormatOptions() const
{
    const MediaRational sourceRate = m_exportSourceInfo.averageFrameRate.isValid()
        ? m_exportSourceInfo.averageFrameRate : m_exportSourceInfo.frameRate;
    QVariantList sizes, rates;
    for (const QSize &size : ExportFormat::resolutionOptions(m_exportSourceInfo.videoSize))
        sizes.append(QVariantMap{{"width", size.width()}, {"height", size.height()}});
    for (const MediaRational &rate : ExportFormat::frameRateOptions(sourceRate))
        rates.append(QVariantMap{{"numerator", rate.numerator}, {"denominator", rate.denominator},
                                 {"text", QString::number(rate.value(), 'f', 2) + QStringLiteral(" fps")}});
    return {{"sizes", sizes}, {"rates", rates}};
}
qint64 AppController::estimateExportSize(const qint64 videoBitrate, const bool audioEnabled, const double seconds) const
{
    return ExportFormat::estimatedBytes(videoBitrate, audioEnabled, seconds);
}
qint64 AppController::recommendedExportBitrate(const int width, const int height, const qint64 numerator,
                                               const qint64 denominator, const QString &quality) const
{
    return ExportFormat::bitrateForQuality(quality, {width, height}, {numerator, denominator});
}
QVariantMap AppController::exportMetrics() const { return m_exportMetrics; }
QVariantMap AppController::exportProgressInfo() const { return m_exportProgressInfo; }
bool AppController::exportProgressVisible() const { return m_exportProgressVisible; }
QString AppController::exportDiagnosticLog() const { return m_exportDiagnosticLog.text(); }
QString AppController::fixedFontFamily() const
{
    return QFontDatabase::systemFont(QFontDatabase::FixedFont).family();
}
QVariantMap AppController::syncCandidate() const { return m_syncCandidate; }
QVariant AppController::speed() const { return semanticValue("speed"); }
QVariant AppController::rpm() const { return semanticValue("rpm"); }
QVariant AppController::heartRate() const { return semanticValue("heartRate"); }
TelemetryRenderContext *AppController::renderContext() { return &m_previewRenderContext; }
WidgetModel *AppController::widgetModel() { return &m_widgetModel; }
QVariantList AppController::trackPoints() const { return m_trackPoints; }
QVariantMap AppController::currentTrackPoint() const
{
    return m_previewRenderContext.currentTrackPoint();
}
QStringList AppController::analysisChannels() const { return m_analysisChannels; }
bool AppController::analysisVisible() const { return m_analysisVisible; }
int AppController::analysisWindowX() const { return m_settings.value("analysis/windowX", -1).toInt(); }
int AppController::analysisWindowY() const { return m_settings.value("analysis/windowY", -1).toInt(); }
int AppController::analysisWindowWidth() const { return m_settings.value("analysis/windowWidth", 1240).toInt(); }
int AppController::analysisWindowHeight() const { return m_settings.value("analysis/windowHeight", 760).toInt(); }
int AppController::analysisSidebarWidth() const { return m_settings.value("analysis/sidebarWidth", 360).toInt(); }
int AppController::analysisVideoHeight() const { return m_settings.value("analysis/videoHeight", 360).toInt(); }
int AppController::windowX() const { return m_settings.value("window/x", -1).toInt(); }
int AppController::windowY() const { return m_settings.value("window/y", -1).toInt(); }
int AppController::windowWidth() const { return m_settings.value("window/width", 1440).toInt(); }
int AppController::windowHeight() const { return m_settings.value("window/height", 900).toInt(); }
QUrl AppController::projectPath() const
{
    return m_documentState.projectPath().isEmpty()
        ? QUrl() : QUrl::fromLocalFile(m_documentState.projectPath());
}
bool AppController::dirty() const { return m_documentState.dirty(); }
quint64 AppController::lastSavedRevision() const { return m_documentState.lastSavedRevision(); }
QString AppController::pendingDestructiveAction() const
{
    return ProjectDocumentState::actionName(m_documentState.pendingAction());
}

QString AppController::videoLoadState() const { return m_videoLoadState; }
QString AppController::vboLoadState() const { return m_vboLoadState; }
bool AppController::projectLoading() const { return m_projectLoading; }
QString AppController::projectLoadStage() const { return m_projectLoadStage; }
QString AppController::projectLoadError() const { return m_projectLoadError; }

QString AppController::normalizedSourcePath(const QString &path)
{
    const QFileInfo info(path);
    const QString canonical = info.canonicalFilePath();
    return canonical.isEmpty() ? info.absoluteFilePath() : canonical;
}

QVariantList AppController::trackPointsFor(const TrackGeometry &geometry)
{
    QVariantList points;
    points.reserve(geometry.points.size());
    for (const QPointF &point : geometry.points) {
        points.append(point);
    }
    return points;
}

quint64 AppController::beginSourceGeneration()
{
    ++m_sourceGeneration;
    cancelSourceJobs();
    if (m_videoProbeWatcher.isRunning()) {
        m_videoLoadState = QStringLiteral("idle");
    }
    if (m_vboLoadWatcher.isRunning()) {
        m_vboLoadState = QStringLiteral("idle");
    }
    emit sourceLoadStateChanged();
    setProjectLoadState(false);
    if (m_syncCancellation) {
        m_syncCancellation->store(true);
    }
    return m_sourceGeneration;
}

void AppController::cancelSourceJobs()
{
    for (const auto &cancellation : {m_videoProbeCancellation, m_vboLoadCancellation,
                                     m_projectLoadCancellation}) {
        if (cancellation) {
            cancellation->store(true);
        }
    }
}

void AppController::startVideoProbe(
    const QString &path, const quint64 generation, const bool markDocumentDirty)
{
    AppLog::info(QStringLiteral("Video load/probe started: %1").arg(path));
    m_videoProbeCancellation = std::make_shared<std::atomic_bool>(false);
    const std::shared_ptr<std::atomic_bool> cancellation = m_videoProbeCancellation;
    m_videoLoadMarksDocumentDirty = markDocumentDirty;
    m_videoLoadState = QStringLiteral("loading");
    emit sourceLoadStateChanged();
    m_videoProbeWatcher.setFuture(QtConcurrent::run([path, generation, cancellation] {
        VideoProbeResult result;
        result.path = path;
        result.generation = generation;
        try {
            result.mediaInfo = MediaProbe::probe(
                path, {}, false, -1, {}, [cancellation] { return cancellation->load(); });
            result.success = !cancellation->load();
            if (!result.success) {
                result.error = QStringLiteral("Video loading was cancelled.");
            }
        } catch (const std::exception &error) {
            result.error = QString::fromUtf8(error.what());
        }
        return result;
    }));
}

void AppController::startVboLoad(
    const QString &path, const quint64 generation, const bool markDocumentDirty)
{
    AppLog::info(QStringLiteral("VBO load started: %1").arg(path));
    m_vboLoadCancellation = std::make_shared<std::atomic_bool>(false);
    const std::shared_ptr<std::atomic_bool> cancellation = m_vboLoadCancellation;
    m_vboLoadMarksDocumentDirty = markDocumentDirty;
    m_vboLoadState = QStringLiteral("loading");
    emit sourceLoadStateChanged();
    m_vboLoadWatcher.setFuture(QtConcurrent::run([path, generation, cancellation] {
        VboLoadResult result;
        result.path = path;
        result.generation = generation;
        try {
            result.session = VboParser::parseFile(path);
            if (cancellation->load()) {
                result.error = QStringLiteral("Telemetry loading was cancelled.");
                return result;
            }
            result.geometry = buildTrackGeometry(result.session);
            result.success = !cancellation->load();
            if (!result.success) {
                result.error = QStringLiteral("Telemetry loading was cancelled.");
            }
        } catch (const std::exception &error) {
            result.error = QString::fromUtf8(error.what());
        }
        return result;
    }));
}

void AppController::commitVideoProbe(const VideoProbeResult &result, const bool markDocumentDirty)
{
    AppLog::info(QStringLiteral("Video load succeeded: %1").arg(result.path));
    m_videoSource = QUrl::fromLocalFile(result.path);
    m_exportSourceInfo = result.mediaInfo;
    m_videoLoadState = QStringLiteral("ready");
    m_pendingVideoPath.clear();
    m_syncCandidate.clear();
    m_settings.setValue("sources/video", result.path);
    emit videoSourceChanged();
    emit exportChanged();
    emit syncCandidateChanged();
    emit sourceLoadStateChanged();
    if (markDocumentDirty) {
        markPersistentChange();
    }
    setStatus(QStringLiteral("Video opened: %1").arg(QFileInfo(result.path).fileName()));
}

void AppController::commitVboLoad(const VboLoadResult &result, const bool markDocumentDirty)
{
    AppLog::info(QStringLiteral("VBO load succeeded: %1").arg(result.path));
    m_session = std::make_unique<TelemetrySession>(result.session);
    m_trackGeometry = result.geometry;
    m_trackPoints = trackPointsFor(m_trackGeometry);
    m_telemetryPath = result.path;
    m_vboLoadState = QStringLiteral("ready");
    m_pendingVboPath.clear();
    m_syncCandidate.clear();
    m_previewRenderContext.setSession(m_session.get());
    m_previewRenderContext.setTrackGeometry(&m_trackGeometry);
    m_settings.setValue("sources/vbo", m_telemetryPath);
    reconcileAnalysisChannels();
    emit telemetryChanged();
    emit liveValuesChanged();
    emit syncCandidateChanged();
    emit sourceLoadStateChanged();
    if (markDocumentDirty) {
        markPersistentChange();
    }
    setStatus(QStringLiteral("VBO opened: %1 samples, %2 numeric channels.")
                  .arg(m_session->sampleCount)
                  .arg(m_session->channels.size()));
}

void AppController::loadVideo(const QUrl &url)
{
    const QString path = url.toLocalFile();
    const QFileInfo info(path);
    const QString extension = info.suffix().toLower();
    if (!info.isFile() || (extension != "mp4" && extension != "mov")) {
        setStatus("Choose an existing MP4 or MOV video.");
        return;
    }
    const QString normalizedPath = normalizedSourcePath(info.absoluteFilePath());
    if (normalizedPath == normalizedSourcePath(m_videoSource.toLocalFile())) {
        return;
    }
    const bool restartVbo = m_vboLoadState == QStringLiteral("loading") && !m_pendingVboPath.isEmpty();
    const QString pendingVboPath = m_pendingVboPath;
    const quint64 generation = beginSourceGeneration();
    m_pendingVideoPath = normalizedPath;
    startVideoProbe(normalizedPath, generation, true);
    if (restartVbo) {
        startVboLoad(pendingVboPath, generation, true);
    }
    setStatus(QStringLiteral("Loading video metadata: %1").arg(info.fileName()));
}

void AppController::loadVbo(const QUrl &url)
{
    const QString path = url.toLocalFile();
    const QFileInfo info(path);
    const QString absolutePath = normalizedSourcePath(path);
    if (!info.isFile() || info.suffix().compare("vbo", Qt::CaseInsensitive) != 0) {
        setStatus("Choose an existing VBOX .vbo telemetry file.");
        return;
    }
    if (!m_telemetryPath.isEmpty()
        && ExportOutputTransaction::normalizedComparisonPath(absolutePath)
            == ExportOutputTransaction::normalizedComparisonPath(m_telemetryPath)) {
        return;
    }
    const bool restartVideo = m_videoLoadState == QStringLiteral("loading") && !m_pendingVideoPath.isEmpty();
    const QString pendingVideoPath = m_pendingVideoPath;
    const quint64 generation = beginSourceGeneration();
    m_pendingVboPath = absolutePath;
    startVboLoad(absolutePath, generation, true);
    if (restartVideo) {
        startVideoProbe(pendingVideoPath, generation, true);
    }
    setStatus(QStringLiteral("Loading telemetry: %1").arg(info.fileName()));
}

void AppController::performClearProject()
{
    const QScopedValueRollback suppressDirty(m_suppressDirtyTracking, true);
    static_cast<void>(beginSourceGeneration());
    m_videoLoadState = QStringLiteral("idle");
    m_vboLoadState = QStringLiteral("idle");
    m_pendingVideoPath.clear();
    m_pendingVboPath.clear();
    setProjectLoadState(false);
    m_videoSource = QUrl();
    m_exportSourceInfo = {};
    m_exportMetrics.clear();
    m_exportDiagnosticLog.clear();
    m_exportProgressInfo.clear();
    m_exportProgressVisible = false;
    m_telemetryPath.clear();
    m_session.reset();
    m_trackGeometry = {};
    m_previewRenderContext.setSession(nullptr);
    m_previewRenderContext.setTrackGeometry(nullptr);
    m_trackPoints.clear();
    setAnalysisChannels({});
    m_playbackTime = 0.0;
    m_sync = {};
    m_syncCandidate.clear();
    m_projectTemplate = {};
    m_widgetModel.resetDefaults();
    m_settings.remove("sources");
    saveSessionSettings();
    emit videoSourceChanged();
    emit telemetryChanged();
    emit playbackTimeChanged();
    emit syncChanged();
    emit syncCandidateChanged();
    emit sourceLoadStateChanged();
    emit liveValuesChanged();
    m_documentState.reset();
    m_pendingOpenProject = QUrl();
    m_settings.remove("project/path");
    emit documentStateChanged();
    emit destructiveActionChanged();
    setStatus("New native project created.");
}

QString AppController::valueText(const QString &channelName, const int decimals) const
{
    if (!m_session) {
        return QStringLiteral("—");
    }
    const auto value = m_session->valueAt(
        channelName,
        videoToTelemetryTime(m_playbackTime, m_sync));
    return value ? QString::number(*value, 'f', qBound(0, decimals, 6)) : QStringLiteral("—");
}

QVariant AppController::telemetryValue(const QString &channelName) const
{
    if (!m_session || channelName.isEmpty()) {
        return {};
    }
    const auto value = m_session->valueAt(
        channelName, videoToTelemetryTime(m_playbackTime, m_sync));
    return value ? QVariant(*value) : QVariant();
}

QVariantMap AppController::telemetrySeries(
    const QString &channelName,
    const double videoStart,
    const double videoEnd,
    const int maximumPoints) const
{
    if (!m_session || channelName.isEmpty() || !std::isfinite(videoStart)
        || !std::isfinite(videoEnd) || maximumPoints < 2) {
        return {};
    }
    const double telemetryStart = videoToTelemetryTime(videoStart, m_sync);
    const double telemetryEnd = videoToTelemetryTime(videoEnd, m_sync);
    const QVector<QPointF> samples = m_session->sampledRange(
        channelName, telemetryStart, telemetryEnd, qBound(2, maximumPoints, 2000));
    if (samples.isEmpty()) {
        return {};
    }
    double minimum = samples.front().y();
    double maximum = minimum;
    for (const QPointF &sample : samples) {
        minimum = std::min(minimum, sample.y());
        maximum = std::max(maximum, sample.y());
    }
    const double telemetrySpan = telemetryEnd - telemetryStart;
    QVariantList points;
    points.reserve(samples.size());
    for (const QPointF &sample : samples) {
        const double normalizedTime = telemetrySpan == 0.0
            ? 0.0
            : (sample.x() - telemetryStart) / telemetrySpan;
        points.append(QVariantMap{{"x", normalizedTime}, {"y", sample.y()}});
    }
    const QString resolved = m_session->aliases.value(channelName, channelName);
    const auto channel = m_session->channels.constFind(resolved);
    return {
        {"points", points},
        {"minimum", minimum},
        {"maximum", maximum},
        {"unit", channel == m_session->channels.cend() ? QString() : channel->unit},
    };
}

void AppController::toggleAnalysisChannel(const QString &channelName)
{
    QStringList channels = m_analysisChannels;
    if (channels.contains(channelName)) {
        channels.removeAll(channelName);
    } else if (!channelName.isEmpty() && channels.size() < 4) {
        channels.append(channelName);
    }
    setAnalysisChannels(channels);
}

void AppController::requestNewProject()
{
    AppLog::info(QStringLiteral("New project requested"));
    beginDestructiveAction(ProjectDocumentState::DestructiveAction::NewProject);
}

void AppController::requestOpenProject(const QUrl &url)
{
    AppLog::info(QStringLiteral("Open project requested: %1").arg(url.toLocalFile()));
    beginDestructiveAction(ProjectDocumentState::DestructiveAction::OpenProject, url);
}

void AppController::requestQuit()
{
    AppLog::info(QStringLiteral("Quit requested"));
    if (exporting()) {
        AppLog::warn(QStringLiteral("Quit request deferred while export is running"));
        return;
    }
    beginDestructiveAction(ProjectDocumentState::DestructiveAction::Quit);
}

void AppController::resolveDestructiveAction(const QString &decision)
{
    if (m_documentState.pendingAction() == ProjectDocumentState::DestructiveAction::None) {
        return;
    }
    if (decision == QStringLiteral("cancel")) {
        cancelPendingDestructiveAction();
        return;
    }
    AppLog::info(QStringLiteral("Dirty project decision: %1").arg(decision));
    if (decision == QStringLiteral("discard")) {
        performPendingDestructiveAction();
        return;
    }
    if (decision != QStringLiteral("save")) {
        return;
    }
    if (m_documentState.projectPath().isEmpty()) {
        emit saveAsRequested();
        return;
    }
    saveCurrentProject();
}

void AppController::cancelPendingDestructiveAction()
{
    if (m_documentState.pendingAction() == ProjectDocumentState::DestructiveAction::None) {
        return;
    }
    AppLog::info(QStringLiteral("Dirty project decision: cancel"));
    m_documentState.cancelPendingAction();
    m_pendingOpenProject = QUrl();
    emit destructiveActionChanged();
}

bool AppController::saveCurrentProject()
{
    if (m_documentState.projectPath().isEmpty()) {
        AppLog::info(QStringLiteral("Project save requested: save as"));
        emit saveAsRequested();
        return false;
    }
    return saveProject(QUrl::fromLocalFile(m_documentState.projectPath()));
}

bool AppController::performOpenProject(const QUrl &url)
{
    AppLog::info(QStringLiteral("Project load started: %1").arg(url.toLocalFile()));
    QFile file(url.toLocalFile());
    if (!file.open(QIODevice::ReadOnly)) {
        AppLog::error(QStringLiteral("Project load failed: %1: %2")
                          .arg(url.toLocalFile(), file.errorString()));
        setStatus(QStringLiteral("Project error: %1").arg(file.errorString()));
        return false;
    }
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    const QJsonObject project = document.object();
    const QJsonObject scene = project.value("scene").toObject();
    WidgetModel candidateWidgets;
    if (project.value("version").toInt() != 2 || !scene.value("widgets").isArray()
        || !candidateWidgets.fromJson(scene.value("widgets").toArray())) {
        AppLog::error(QStringLiteral("Project load failed: unsupported or invalid file: %1")
                          .arg(url.toLocalFile()));
        setStatus("Project error: unsupported or invalid .fetproject file.");
        return false;
    }
    const QJsonObject sync = project.value("sync").toObject();
    const double offset = sync.value("offset").toDouble();
    const double timeScale = sync.value("timeScale").toDouble(1.0);
    if (!std::isfinite(offset) || !std::isfinite(timeScale) || timeScale <= 0.0) {
        AppLog::error(QStringLiteral("Project load failed: invalid synchronization state: %1")
                          .arg(url.toLocalFile()));
        setStatus("Project error: synchronization state is invalid.");
        return false;
    }
    const QJsonObject analysis = project.value("analysis").toObject();
    QStringList channels;
    if (analysis.value("channels").isArray()) {
        for (const QJsonValue &value : analysis.value("channels").toArray()) {
            channels.append(value.toString());
        }
    }
    const quint64 generation = beginSourceGeneration();
    m_projectLoadCancellation = std::make_shared<std::atomic_bool>(false);
    const std::shared_ptr<std::atomic_bool> cancellation = m_projectLoadCancellation;
    const QString projectPath = normalizedSourcePath(file.fileName());
    const QString videoPath = project.value("videoPath").toString();
    const QString vboPath = project.value("vboPath").toString();
    const QString normalizedVideoPath = videoPath.isEmpty() ? QString() : normalizedSourcePath(videoPath);
    const QString normalizedVboPath = vboPath.isEmpty() ? QString() : normalizedSourcePath(vboPath);
    m_pendingVideoPath.clear();
    m_pendingVboPath.clear();
    setProjectLoadState(true, normalizedVideoPath.isEmpty()
                                  ? QStringLiteral("Loading telemetry")
                                  : QStringLiteral("Loading video metadata"));
    m_projectLoadWatcher.setFuture(QtConcurrent::run(
        [projectPath, project, widgets = scene.value("widgets").toArray(), channels,
         analysisVisible = analysis.value("visible").toBool(true),
         syncTransform = SyncTransform{offset, timeScale}, normalizedVideoPath, normalizedVboPath,
         generation, cancellation] {
            ProjectLoadResult result;
            result.projectPath = projectPath;
            result.project = project;
            result.widgets = widgets;
            result.analysisChannels = channels;
            result.analysisVisible = analysisVisible;
            result.sync = syncTransform;
            result.generation = generation;
            if (!normalizedVideoPath.isEmpty()) {
                result.video.path = normalizedVideoPath;
                result.video.generation = generation;
                try {
                    result.video.mediaInfo = MediaProbe::probe(
                        normalizedVideoPath, {}, false, -1, {},
                        [cancellation] { return cancellation->load(); });
                    result.video.success = !cancellation->load();
                } catch (const std::exception &error) {
                    result.error = QStringLiteral("video source %1: %2")
                                       .arg(normalizedVideoPath, QString::fromUtf8(error.what()));
                    return result;
                }
                if (!result.video.success) {
                    result.error = QStringLiteral("video source loading was cancelled.");
                    return result;
                }
            }
            if (!normalizedVboPath.isEmpty()) {
                result.vbo.path = normalizedVboPath;
                result.vbo.generation = generation;
                try {
                    result.vbo.session = VboParser::parseFile(normalizedVboPath);
                    if (cancellation->load()) {
                        result.error = QStringLiteral("telemetry source loading was cancelled.");
                        return result;
                    }
                    result.vbo.geometry = buildTrackGeometry(result.vbo.session);
                    result.vbo.success = !cancellation->load();
                } catch (const std::exception &error) {
                    result.error = QStringLiteral("telemetry source %1: %2")
                                       .arg(normalizedVboPath, QString::fromUtf8(error.what()));
                    return result;
                }
                if (!result.vbo.success) {
                    result.error = QStringLiteral("telemetry source loading was cancelled.");
                    return result;
                }
            }
            result.success = true;
            return result;
        }));
    return true;
}

void AppController::setProjectLoadState(bool loading, QString stage, QString error)
{
    if (m_projectLoading == loading && m_projectLoadStage == stage && m_projectLoadError == error) {
        return;
    }
    m_projectLoading = loading;
    m_projectLoadStage = std::move(stage);
    m_projectLoadError = std::move(error);
    emit projectLoadChanged();
}

void AppController::commitProjectLoad(const ProjectLoadResult &result)
{
    const QScopedValueRollback suppressDirty(m_suppressDirtyTracking, true);
    setProjectLoadState(true, QStringLiteral("Applying project"));
    if (!m_widgetModel.fromJson(result.widgets)) {
        AppLog::error(QStringLiteral("Project load failed while applying widget scene: %1")
                          .arg(result.projectPath));
        setProjectLoadState(false, {}, QStringLiteral("widget scene could not be applied."));
        setStatus("Project could not be opened: widget scene could not be applied.");
        return;
    }
    m_projectTemplate = result.project;
    m_videoSource = result.video.path.isEmpty() ? QUrl() : QUrl::fromLocalFile(result.video.path);
    m_exportSourceInfo = result.video.mediaInfo;
    m_videoLoadState = result.video.path.isEmpty() ? QStringLiteral("idle") : QStringLiteral("ready");
    m_telemetryPath = result.vbo.path;
    m_session = result.vbo.path.isEmpty()
        ? nullptr : std::make_unique<TelemetrySession>(result.vbo.session);
    m_trackGeometry = result.vbo.geometry;
    m_trackPoints = trackPointsFor(m_trackGeometry);
    m_vboLoadState = result.vbo.path.isEmpty() ? QStringLiteral("idle") : QStringLiteral("ready");
    m_previewRenderContext.setSession(m_session.get());
    m_previewRenderContext.setTrackGeometry(m_session ? &m_trackGeometry : nullptr);
    m_sync = result.sync;
    m_previewRenderContext.setSyncTransform(m_sync);
    m_playbackTime = 0.0;
    m_syncCandidate.clear();
    m_analysisChannels.clear();
    setAnalysisChannels(result.analysisChannels);
    setAnalysisVisible(result.analysisVisible);
    reconcileAnalysisChannels();
    m_settings.setValue("sources/video", result.video.path);
    m_settings.setValue("sources/vbo", result.vbo.path);
    m_settings.setValue("project/path", result.projectPath);
    saveSessionSettings();
    m_documentState.reset(result.projectPath);
    emit videoSourceChanged();
    emit telemetryChanged();
    emit exportChanged();
    emit playbackTimeChanged();
    emit syncChanged();
    emit syncCandidateChanged();
    emit liveValuesChanged();
    emit sourceLoadStateChanged();
    emit documentStateChanged();
    setProjectLoadState(false);
    AppLog::info(QStringLiteral("Project load succeeded: %1").arg(result.projectPath));
    setStatus(QStringLiteral("Project opened: %1").arg(QFileInfo(result.projectPath).fileName()));
}

bool AppController::saveProject(const QUrl &url)
{
    QString path = url.toLocalFile();
    if (!path.endsWith(".fetproject", Qt::CaseInsensitive)) {
        path.append(".fetproject");
    }
    AppLog::info(QStringLiteral("Project save requested: %1").arg(path));
    QJsonObject project = m_projectTemplate;
    project.insert("version", 2);
    project.insert("videoPath", m_videoSource.toLocalFile());
    project.insert("vboPath", m_telemetryPath);
    project.insert(
        "sync", QJsonObject{{"offset", m_sync.offset}, {"timeScale", m_sync.timeScale}});
    project.insert("scene", QJsonObject{{"widgets", m_widgetModel.toJson()}});
    project.insert(
        "analysis",
        QJsonObject{{"channels", QJsonArray::fromStringList(m_analysisChannels)},
                    {"visible", m_analysisVisible}});
    if (!project.contains("mapSettings")) {
        project.insert("mapSettings", QJsonObject{{"providerId", "none"}});
    }
    if (!project.contains("exportSettings")) {
        project.insert("exportSettings", QJsonObject{{"quality", "high"}});
    }
    const QByteArray payload = QJsonDocument(project).toJson(QJsonDocument::Indented);
    const ProjectWriter::Result writeResult = m_projectWriter.write(path, payload);
    if (!writeResult.success) {
        AppLog::error(QStringLiteral("Project save failed: %1: %2").arg(path, writeResult.error));
        setStatus(QStringLiteral("Project save error: %1").arg(writeResult.error));
        if (m_documentState.pendingAction() != ProjectDocumentState::DestructiveAction::None) {
            emit destructiveActionChanged();
        }
        return false;
    }
    m_projectTemplate = project;
    m_settings.setValue("project/path", path);
    m_documentState.markSaved(path);
    emit documentStateChanged();
    AppLog::info(QStringLiteral("Project save succeeded: %1").arg(path));
    setStatus(QStringLiteral("Project saved: %1").arg(QFileInfo(path).fileName()));
    if (m_documentState.pendingAction() != ProjectDocumentState::DestructiveAction::None) {
        performPendingDestructiveAction();
    }
    return true;
}

void AppController::autoSync()
{
    AppLog::info(QStringLiteral("Auto-sync requested"));
    if (m_syncWatcher.isRunning()) {
        return;
    }
    const QString videoPath = m_videoSource.toLocalFile();
    if (videoPath.isEmpty() || !m_session) {
        setStatus("Open both a GoPro video and VBO telemetry before auto sync.");
        return;
    }
    const TelemetrySession telemetry = *m_session;
    const quint64 generation = m_sourceGeneration;
    const QString normalizedVideoPath = normalizedSourcePath(videoPath);
    const QString normalizedVboPath = normalizedSourcePath(m_telemetryPath);
    m_syncCancellation = std::make_shared<std::atomic_bool>(false);
    const std::shared_ptr<std::atomic_bool> cancellation = m_syncCancellation;
    m_syncCandidate.clear();
    emit syncCandidateChanged();
    setStatus("Indexing GoPro telemetry and matching GPS speed…");
    m_syncWatcher.setFuture(QtConcurrent::run(
        [normalizedVideoPath, normalizedVboPath, telemetry, generation, cancellation] {
        AutoSyncResult result;
        result.generation = generation;
        result.videoPath = normalizedVideoPath;
        result.vboPath = normalizedVboPath;
        try {
            if (cancellation->load()) {
                return result;
            }
            const GoProTelemetryResult videoTelemetry = GoProTelemetrySource::load(normalizedVideoPath);
            if (cancellation->load()) {
                return result;
            }
            result.candidate = TelemetrySyncEngine::synchronize(videoTelemetry.session, telemetry);
            if (cancellation->load()) {
                return result;
            }
            result.packetCount = videoTelemetry.packetCount;
            result.gpsSampleCount = videoTelemetry.session.sampleCount;
            result.gpsStream = videoTelemetry.gpsStream;
            result.success = true;
        } catch (const std::exception &error) {
            result.error = QString::fromUtf8(error.what());
        }
        return result;
    }));
    emit syncingChanged();
}

void AppController::applySyncCandidate()
{
    if (m_syncCandidate.isEmpty()) {
        return;
    }
    const double offset = m_syncCandidate.value("offset").toDouble();
    const double scale = m_syncCandidate.value("timeScale", 1.0).toDouble();
    if (!std::isfinite(offset) || !std::isfinite(scale) || scale <= 0.0) {
        setStatus("Synchronization candidate is invalid and cannot be applied.");
        return;
    }
    setSyncOffset(offset);
    setTimeScale(scale);
    AppLog::info(QStringLiteral("Auto-sync candidate applied: offset=%1 s, scale=%2")
                     .arg(offset, 0, 'f', 3).arg(scale, 0, 'g', 12));
    m_syncCandidate.insert("automaticallyApplied", true);
    m_syncCandidate.insert("appliedManually", true);
    emit syncCandidateChanged();
    setStatus(QStringLiteral("Synchronization candidate applied: %1 s.").arg(offset, 0, 'f', 3));
}

void AppController::ignoreSyncCandidate()
{
    if (m_syncCandidate.isEmpty()) {
        return;
    }
    AppLog::info(QStringLiteral("Auto-sync candidate rejected"));
    m_syncCandidate.clear();
    emit syncCandidateChanged();
    setStatus("Synchronization candidate ignored; existing timing was retained.");
}

bool AppController::startExport(
    const QUrl &output,
    const int outputWidth, const int outputHeight, const qint64 frameRateNumerator, const qint64 frameRateDenominator,
    const qint64 videoBitrate,
    const bool audioEnabled,
    const bool customRange,
    const double rangeStart,
    const double rangeEnd,
    const bool overwriteAllowed)
{
    AppLog::info(QStringLiteral("Export requested: %1").arg(output.toLocalFile()));
    if (exporting()) {
        AppLog::warn(QStringLiteral("Export request ignored because an export is already running"));
        return false;
    }
    const QString inputPath = m_videoSource.toLocalFile();
    const QString outputPath = output.toLocalFile();
    if (!m_session || inputPath.isEmpty() || m_telemetryPath.isEmpty() || outputPath.isEmpty()) {
        m_exportError = QStringLiteral("Open a video and VBO telemetry, then choose an output file.");
        m_exportState = QStringLiteral("failed");
        AppLog::error(QStringLiteral("Export failed: %1").arg(m_exportError));
        emit exportChanged();
        return false;
    }
    if (!m_exportSourceInfo.videoSize.isValid()) {
        m_exportError = QStringLiteral("Video metadata is still loading or unavailable.");
        m_exportState = QStringLiteral("failed");
        AppLog::error(QStringLiteral("Export failed: %1").arg(m_exportError));
        emit exportChanged();
        return false;
    }
    const QSize outputSize(outputWidth, outputHeight);
    const MediaRational outputRate{frameRateNumerator, frameRateDenominator};
    if (!outputSize.isValid() || outputSize.width() % 2 || outputSize.height() % 2
        || outputSize.width() > m_exportSourceInfo.videoSize.width() || outputSize.height() > m_exportSourceInfo.videoSize.height()
        || !outputRate.isValid() || outputRate.value() > ExportEngine::effectiveFrameRate(m_exportSourceInfo).value()
        || !ExportFormat::validCustomBitrate(videoBitrate)) {
        m_exportError = QStringLiteral("Export format is invalid. Choose an even, non-upscaled size, supported frame rate, and 0.5–120 Mbps bitrate.");
        m_exportState = QStringLiteral("failed"); emit exportChanged(); return false;
    }
    const double sourceDuration = m_exportSourceInfo.duration;
    const double startTime = customRange ? rangeStart : 0.0;
    const double endTime = customRange ? rangeEnd : sourceDuration;
    if (!std::isfinite(sourceDuration) || sourceDuration <= 0.0 || !std::isfinite(startTime)
        || !std::isfinite(endTime) || startTime < 0.0 || endTime <= startTime
        || endTime > sourceDuration) {
        m_exportError = QStringLiteral(
            "Export range must satisfy 0 ≤ start < end ≤ source duration (%1 s).")
                            .arg(sourceDuration, 0, 'f', 3);
        m_exportState = QStringLiteral("failed");
        AppLog::error(QStringLiteral("Export failed: %1").arg(m_exportError));
        emit exportChanged();
        return false;
    }
    m_exportOutputTransaction = std::make_unique<ExportOutputTransaction>();
    const auto preparation = m_exportOutputTransaction->prepare(
        outputPath, inputPath, {m_telemetryPath}, overwriteAllowed);
    if (preparation.status == ExportOutputTransaction::PreparationStatus::OverwriteConfirmationRequired) {
        m_exportState = QStringLiteral("overwriteConfirmationRequired");
        m_exportError.clear();
        m_exportProgressInfo = {{"outputPath", m_exportOutputTransaction->userTargetPath()},
                                {"outputName", QFileInfo(outputPath).fileName()},
                                {"targetExistedBeforeExport", true}};
        m_exportOutputTransaction.reset();
        emit exportChanged();
        return false;
    }
    if (preparation.status == ExportOutputTransaction::PreparationStatus::Error) {
        m_exportError = preparation.error;
        m_exportState = QStringLiteral("failed");
        AppLog::error(QStringLiteral("Export failed: %1").arg(m_exportError));
        m_exportOutputTransaction.reset();
        emit exportChanged();
        return false;
    }
    const QString exportId = m_exportOutputTransaction->transactionId();
    const QString temporaryOverlayPath = QDir::temp().filePath(
        QStringLiteral("flappedear-overlay-%1.mkv").arg(exportId));
    const ExportArtifactManifestData manifest{exportId, QDateTime::currentMSecsSinceEpoch(),
        temporaryOverlayPath, m_exportOutputTransaction->stagingPath(),
        m_exportOutputTransaction->userTargetPath(), 0, QStringLiteral("preparing")};
    QString manifestError;
    if (!ExportArtifactManifest::create(manifest, &manifestError)) {
        m_exportError = QStringLiteral("Could not create export ownership manifest: %1").arg(manifestError);
        m_exportState = QStringLiteral("failed");
        AppLog::error(QStringLiteral("Export failed: %1").arg(m_exportError));
        m_exportOutputTransaction.reset();
        emit exportChanged();
        return false;
    }
    m_exportManifestPath = ExportArtifactManifest::manifestPathFor(exportId);
    m_exportConfig = std::make_unique<QTemporaryFile>(
        QDir::temp().filePath(QStringLiteral("flappedear-export-XXXXXX.json")));
    if (!m_exportConfig->open()) {
        m_exportError = QStringLiteral("Could not create temporary export configuration.");
        m_exportState = QStringLiteral("failed");
        AppLog::error(QStringLiteral("Export failed: %1").arg(m_exportError));
        static_cast<void>(ExportArtifactManifest::cleanupOwned(m_exportManifestPath));
        m_exportManifestPath.clear();
        m_exportOutputTransaction.reset();
        emit exportChanged();
        return false;
    }
    m_exportCancelPath = m_exportConfig->fileName() + QStringLiteral(".cancel");
    m_exportSupervisionReadyPath = m_exportConfig->fileName() + QStringLiteral(".supervision-ready");
    QFile::remove(m_exportCancelPath);
    QFile::remove(m_exportSupervisionReadyPath);
    const QJsonObject config = {
        {"inputPath", inputPath},
        {"outputPath", m_exportOutputTransaction->stagingPath()},
        {"vboPath", m_telemetryPath},
        {"widgets", m_widgetModel.toJson()},
        {"sync", QJsonObject{{"offset", m_sync.offset}, {"timeScale", m_sync.timeScale}}},
        {"outputWidth", outputSize.width()}, {"outputHeight", outputSize.height()},
        {"frameRateNumerator", outputRate.numerator}, {"frameRateDenominator", outputRate.denominator},
        {"videoBitrate", videoBitrate},
        {"audioEnabled", audioEnabled},
        {"startTime", startTime},
        {"endTime", endTime},
        {"cancelPath", m_exportCancelPath},
        {"supervisionReadyPath", m_exportSupervisionReadyPath},
        {"temporaryOverlayPath", temporaryOverlayPath},
        {"manifestPath", m_exportManifestPath},
    };
    if (m_exportConfig->write(QJsonDocument(config).toJson(QJsonDocument::Compact)) < 0) {
        m_exportError = QStringLiteral("Could not write temporary export configuration.");
        m_exportState = QStringLiteral("failed");
        AppLog::error(QStringLiteral("Export failed: %1").arg(m_exportError));
        static_cast<void>(ExportArtifactManifest::cleanupOwned(m_exportManifestPath));
        m_exportManifestPath.clear();
        m_exportOutputTransaction.reset();
        m_exportConfig.reset();
        emit exportChanged();
        return false;
    }
    m_exportConfig->flush();
    // The worker is a separate process; closing before it starts avoids a
    // Windows sharing violation while the controller retains ownership for
    // cleanup after completion.
    m_exportConfig->close();
    m_exportStdout.clear();
    m_exportProgress = 0;
    m_exportError.clear();
    m_exportMetrics.clear();
    m_exportDiagnosticLog.clear();
    m_exportProgressInfo = {{"outputPath", m_exportOutputTransaction->userTargetPath()},
                            {"stagingPath", m_exportOutputTransaction->stagingPath()},
                            {"outputName", QFileInfo(outputPath).fileName()},
                            {"targetExistedBeforeExport", m_exportOutputTransaction->targetExistedBeforeExport()},
                            {"syncOffset", m_sync.offset}, {"timeScale", m_sync.timeScale},
                            {"width", outputSize.width()}, {"height", outputSize.height()},
                            {"frameRate", outputRate.value()}, {"videoBitrate", videoBitrate},
                            {"audioLabel", audioEnabled ? QStringLiteral("AAC audio") : QStringLiteral("No audio")}};
    m_exportProgressVisible = true;
    m_exportState = QStringLiteral("starting");
    AppLog::info(QStringLiteral("Export Stage A preparing"));
    m_exportProcess = std::make_unique<QProcess>(this);
    m_exportSupervisor = std::make_unique<ExportProcessSupervisor>(*m_exportProcess);
    m_exportProcess->setProcessChannelMode(QProcess::SeparateChannels);
    connect(m_exportProcess.get(), &QProcess::readyReadStandardOutput, this, &AppController::handleExportOutput);
    connect(
        m_exportProcess.get(),
        qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
        this,
        &AppController::finishExport);
    m_exportSupervisor->start(
        QCoreApplication::applicationFilePath(), {"--export-worker", m_exportConfig->fileName()});
    if (!m_exportSupervisor->waitForStarted(5'000)) {
        const QString supervisionError = m_exportSupervisor->supervisionError();
        m_exportError = supervisionError.isEmpty()
            ? QStringLiteral("Could not start export worker: %1").arg(m_exportProcess->errorString())
            : QStringLiteral("Could not establish export process supervision: %1").arg(supervisionError);
        m_exportState = QStringLiteral("failed");
        AppLog::error(QStringLiteral("Export failed: %1").arg(m_exportError));
        static_cast<void>(ExportArtifactManifest::cleanupOwned(m_exportManifestPath));
        m_exportManifestPath.clear();
        m_exportSupervisor.reset();
        m_exportProcess.reset();
        m_exportConfig.reset();
        m_exportOutputTransaction.reset();
        emit exportChanged();
        return false;
    }
    if (!m_exportSupervisor->supervisionActive()) {
        m_exportError = QStringLiteral("Export process supervision was not established.");
        static_cast<void>(m_exportSupervisor->stopAndWait());
        m_exportState = QStringLiteral("failed");
        AppLog::error(QStringLiteral("Export failed: %1").arg(m_exportError));
        emit exportChanged();
        return false;
    }
    ExportArtifactManifestData activeManifest;
    if (!ExportArtifactManifest::read(m_exportManifestPath, &activeManifest, &manifestError)) {
        m_exportError = QStringLiteral("Could not read active export ownership manifest: %1").arg(manifestError);
        static_cast<void>(m_exportSupervisor->stopAndWait());
        m_exportState = QStringLiteral("failed");
        AppLog::error(QStringLiteral("Export failed: %1").arg(m_exportError));
        emit exportChanged();
        return false;
    }
    activeManifest.workerPid = m_exportProcess->processId();
    if (!ExportArtifactManifest::update(m_exportManifestPath, activeManifest, &manifestError)) {
        m_exportError = QStringLiteral("Could not update active export ownership manifest: %1").arg(manifestError);
        static_cast<void>(m_exportSupervisor->stopAndWait());
        m_exportState = QStringLiteral("failed");
        AppLog::error(QStringLiteral("Export failed: %1").arg(m_exportError));
        emit exportChanged();
        return false;
    }
    QFile supervisionReady(m_exportSupervisionReadyPath);
    if (!supervisionReady.open(QIODevice::WriteOnly | QIODevice::NewOnly)) {
        m_exportError = QStringLiteral("Could not release supervised export worker: %1")
                            .arg(supervisionReady.errorString());
        static_cast<void>(m_exportSupervisor->stopAndWait());
        m_exportState = QStringLiteral("failed");
        AppLog::error(QStringLiteral("Export failed: %1").arg(m_exportError));
        emit exportChanged();
        return false;
    }
    supervisionReady.close();
    emit exportChanged();
    return true;
}

void AppController::cancelExport()
{
    if (!exporting()) {
        return;
    }
    QFile cancellationFile(m_exportCancelPath);
    if (!cancellationFile.open(QIODevice::WriteOnly)) {
        m_exportError = QStringLiteral("Could not request export cancellation.");
        m_exportState = QStringLiteral("failed");
        AppLog::error(QStringLiteral("Export cancellation failed: %1").arg(m_exportError));
        emit exportChanged();
        return;
    }
    cancellationFile.close();
    AppLog::warn(QStringLiteral("Export cancellation requested"));
    m_exportState = QStringLiteral("cancelling");
    m_exportProgressInfo.insert("stage", QStringLiteral("cancelling"));
    emit exportChanged();
}

void AppController::cancelExportAndQuit()
{
    if (!exporting()) {
        requestQuit();
        return;
    }
    m_quitAfterExport = true;
    cancelExport();
    // Escalate through the dedicated process-tree owner: the worker and every
    // inherited FFmpeg/ffprobe descendant are stopped as one lifetime unit.
    QTimer::singleShot(7'000, this, [this] {
        if (!exporting()) {
            return;
        }
        if (m_exportSupervisor) static_cast<void>(m_exportSupervisor->stopAndWait(3'000, 3'000));
    });
}

void AppController::dismissExportProgress()
{
    if (exporting() || m_exportState == "cancelling") return;
    m_exportProgressVisible = false;
    if (m_exportState == "complete" || m_exportState == "validationWarning"
        || m_exportState == "cancelled") {
        m_exportState = QStringLiteral("idle");
    }
    emit exportChanged();
}

void AppController::copyExportDiagnostics()
{
    if (QGuiApplication::clipboard()) {
        QGuiApplication::clipboard()->setText(m_exportDiagnosticLog.text());
    }
}

namespace {

QString diagnosticTimestamp(const qint64 elapsedMilliseconds)
{
    const qint64 hours = elapsedMilliseconds / 3'600'000;
    const qint64 minutes = (elapsedMilliseconds / 60'000) % 60;
    const qint64 seconds = (elapsedMilliseconds / 1'000) % 60;
    const qint64 milliseconds = elapsedMilliseconds % 1'000;
    return QStringLiteral("%1:%2:%3.%4")
        .arg(hours, 2, 10, QLatin1Char('0'))
        .arg(minutes, 2, 10, QLatin1Char('0'))
        .arg(seconds, 2, 10, QLatin1Char('0'))
        .arg(milliseconds, 3, 10, QLatin1Char('0'));
}

QString diagnosticValue(const QVariant &value)
{
    if (value.metaType().id() == QMetaType::QStringList) {
        return value.toStringList().join(QLatin1Char(' '));
    }
    if (value.metaType().id() == QMetaType::QVariantList) {
        QStringList items;
        for (const QVariant &item : value.toList()) items.append(item.toString());
        return items.join(QLatin1Char(' '));
    }
    return value.toString();
}

QString formatDiagnosticEvent(const QJsonObject &event)
{
    QString result = QStringLiteral("[%1] %2")
                         .arg(diagnosticTimestamp(event.value("timestampMilliseconds").toInteger()),
                              event.value("message").toString());
    const QVariantMap details = event.value("details").toObject().toVariantMap();
    QStringList keys = details.keys();
    std::sort(keys.begin(), keys.end());
    for (const QString &key : std::as_const(keys)) {
        const QString value = diagnosticValue(details.value(key));
        if (!value.isEmpty()) result += QStringLiteral("\n    %1: %2").arg(key, value);
    }
    const QString error = event.value("error").toString();
    if (!error.isEmpty()) result += QStringLiteral("\n    error: %1").arg(error);
    const QString diagnostics = event.value("diagnostics").toString();
    if (!diagnostics.isEmpty()) result += QStringLiteral("\n    diagnostics:\n%1").arg(diagnostics);
    return result;
}

} // namespace

void AppController::handleExportOutput()
{
    if (!m_exportProcess) {
        return;
    }
    m_exportStdout.append(m_exportProcess->readAllStandardOutput());
    qsizetype newline = -1;
    while ((newline = m_exportStdout.indexOf('\n')) >= 0) {
        const QByteArray line = m_exportStdout.left(newline);
        m_exportStdout.remove(0, newline + 1);
        const QJsonObject event = QJsonDocument::fromJson(line).object();
        if (event.value("type").toString() == QStringLiteral("log")) {
            m_exportDiagnosticLog.append(formatDiagnosticEvent(event));
        }
        const QString state = event.value("state").toString();
        if (!state.isEmpty()) {
            if (state != m_exportState) {
                if (state == QStringLiteral("renderingOverlay")) {
                    AppLog::info(QStringLiteral("Export Stage A started"));
                } else if (state == QStringLiteral("validatingOverlay")) {
                    AppLog::info(QStringLiteral("Export Stage A ended"));
                    AppLog::info(QStringLiteral("Export overlay validation started"));
                } else if (state == QStringLiteral("encodingVideo")) {
                    AppLog::info(QStringLiteral("Export overlay validation passed"));
                    AppLog::info(QStringLiteral("Export Stage B started"));
                } else if (state == QStringLiteral("validatingOutput")) {
                    AppLog::info(QStringLiteral("Export Stage B ended"));
                    AppLog::info(QStringLiteral("Export final validation started"));
                } else if (state == QStringLiteral("complete")) {
                    AppLog::info(QStringLiteral("Export final validation passed"));
                } else if (state == QStringLiteral("validationWarning")) {
                    AppLog::warn(QStringLiteral("Export validation completed with a warning"));
                } else if (state == QStringLiteral("cancelled")) {
                    AppLog::warn(QStringLiteral("Export cancelled"));
                }
            }
            m_exportState = state;
        }
        for (const QString &key : {QStringLiteral("generatedFrames"), QStringLiteral("renderedFrames"), QStringLiteral("expectedFrames"),
                                   QStringLiteral("sourceRangeStart"), QStringLiteral("sourceRangeEnd"),
                                   QStringLiteral("exportDuration"), QStringLiteral("exportRelativeTime"),
                                   QStringLiteral("sourceVideoTime"),
                                   QStringLiteral("telemetryTime"), QStringLiteral("elapsedMilliseconds"),
                                   QStringLiteral("throughputFps"), QStringLiteral("realtimeFactor"),
                                   QStringLiteral("etaSeconds"), QStringLiteral("outputBytes"),
                                   QStringLiteral("encoderId"), QStringLiteral("encoderName"),
                                   QStringLiteral("width"), QStringLiteral("height"), QStringLiteral("frameRate"),
                                   QStringLiteral("audioEnabled"), QStringLiteral("encodedFrames"),
                                   QStringLiteral("encodedSeconds"), QStringLiteral("encodedProgress"),
                                   QStringLiteral("encoderFps"), QStringLiteral("encoderRealtimeFactor"),
                                   QStringLiteral("rendererFps"), QStringLiteral("queuedBytes"),
                                   QStringLiteral("maximumQueuedBytes"), QStringLiteral("temporaryOverlayBytes"),
                                   QStringLiteral("estimatedTemporaryOverlayBytes"),
                                   QStringLiteral("estimatedFinalOutputBytes"),
                                   QStringLiteral("safetyReserveBytes"),
                                   QStringLiteral("estimateBasis"), QStringLiteral("sampleFrames"),
                                   QStringLiteral("sampleEncodedBytes"), QStringLiteral("sampleBytesPerFrame"),
                                   QStringLiteral("sampleSafetyMargin"), QStringLiteral("sampleError"),
                                   QStringLiteral("temporaryFilesystemRoot"),
                                   QStringLiteral("temporaryFilesystemAvailableBytes"),
                                   QStringLiteral("destinationFilesystemRoot"),
                                   QStringLiteral("destinationFilesystemAvailableBytes"),
                                   QStringLiteral("outputBytes"), QStringLiteral("currentOperation"),
                                   QStringLiteral("operation"), QStringLiteral("stageElapsedMilliseconds"),
                                   QStringLiteral("totalElapsedMilliseconds"), QStringLiteral("stageDurations"),
                                   QStringLiteral("outputVideoCodec"), QStringLiteral("outputWidth"),
                                   QStringLiteral("outputHeight"), QStringLiteral("outputDuration"),
                                   QStringLiteral("outputVideoDuration"), QStringLiteral("outputVideoStart"),
                                   QStringLiteral("outputVideoPacketCount"),
                                   QStringLiteral("outputAverageFrameRate"),
                                   QStringLiteral("outputAudioCodecs"), QStringLiteral("outputAudioStart"),
                                   QStringLiteral("outputAudioDuration"),
                                   QStringLiteral("exportFrameRateNumerator"),
                                   QStringLiteral("exportFrameRateDenominator"),
                                   QStringLiteral("exportFrameRate"), QStringLiteral("diagnostics"),
                                   QStringLiteral("warning")}) {
            if (event.contains(key)) m_exportProgressInfo.insert(key, event.value(key).toVariant());
        }
        if (!state.isEmpty()) m_exportProgressInfo.insert("stage", state);
        if (event.contains("visibleProgress")) {
            m_exportProgress = qRound(event.value("visibleProgress").toDouble());
            m_exportProgressInfo.insert("progressPercent", event.value("visibleProgress").toDouble());
        } else if (state == "validatingOverlay") {
            m_exportProgress = qMax(m_exportProgress, 60);
            m_exportProgressInfo.insert("progressPercent", m_exportProgress);
        }
        if (state == "validatingOutput" || state == "validating") {
            m_exportProgress = qMax(m_exportProgress, 99);
            m_exportProgressInfo.insert("progressPercent", m_exportProgress);
        }
        if (event.contains("error")) {
            m_exportError = event.value("error").toString();
            AppLog::error(QStringLiteral("Export failed: %1").arg(m_exportError));
        }
        if (event.contains("renderMilliseconds")) {
            m_exportMetrics = {
                {"elapsedMilliseconds", event.value("elapsedMilliseconds").toInteger()},
                {"renderMilliseconds", event.value("renderMilliseconds").toInteger()},
                {"renderNanoseconds", event.value("renderNanoseconds").toInteger()},
                {"polishNanoseconds", event.value("polishNanoseconds").toInteger()},
                {"syncRenderNanoseconds", event.value("syncRenderNanoseconds").toInteger()},
                {"readbackNanoseconds", event.value("readbackNanoseconds").toInteger()},
                {"cpuCopyNanoseconds", event.value("cpuCopyNanoseconds").toInteger()},
                {"ffmpegWriteNanoseconds", event.value("ffmpegWriteNanoseconds").toInteger()},
                {"renderedFrames", event.value("renderedFrames").toInteger()},
            };
        }
        emit exportChanged();
    }
}

void AppController::finishExport(const int exitCode, const QProcess::ExitStatus exitStatus)
{
    handleExportOutput();
    const bool cancelled = QFileInfo::exists(m_exportCancelPath);
    const QString workerError = m_exportProcess
        ? QString::fromUtf8(m_exportProcess->readAllStandardError()).trimmed()
        : QString();
    if (cancelled) {
        m_exportState = QStringLiteral("cancelled");
        m_exportError.clear();
        setStatus("Export cancelled.");
    } else if (exitStatus == QProcess::NormalExit && exitCode == 0
               && m_exportState == QStringLiteral("complete")) {
        QString commitError;
        if (m_exportOutputTransaction && m_exportOutputTransaction->commit(&commitError)) {
            ExportArtifactManifestData manifest;
            if (ExportArtifactManifest::read(m_exportManifestPath, &manifest)) {
                manifest.state = QStringLiteral("completed");
                static_cast<void>(ExportArtifactManifest::update(m_exportManifestPath, manifest));
            }
            m_exportProgress = 100;
            m_exportState = QStringLiteral("complete");
            m_exportError.clear();
            AppLog::info(QStringLiteral("Export succeeded: %1")
                             .arg(m_exportOutputTransaction->userTargetPath()));
            setStatus("HEVC export finished and passed validation.");
        } else {
            m_exportState = QStringLiteral("failed");
            m_exportError = commitError.isEmpty()
                ? QStringLiteral("Validated export could not be committed to its target.") : commitError;
            AppLog::error(QStringLiteral("Export failed: %1").arg(m_exportError));
            setStatus(QStringLiteral("Export failed: %1").arg(m_exportError));
        }
    } else {
        m_exportState = QStringLiteral("failed");
        if (m_exportError.isEmpty()) {
            m_exportError = workerError.isEmpty() ? QStringLiteral("Export worker failed.") : workerError;
        }
        AppLog::error(QStringLiteral("Export failed: %1").arg(m_exportError));
        setStatus(QStringLiteral("Export failed: %1").arg(m_exportError));
    }
    m_exportProgressInfo.insert("stage", m_exportState);
    m_exportProgressInfo.insert("progressPercent", m_exportProgress);
    QFile::remove(m_exportCancelPath);
    QFile::remove(m_exportSupervisionReadyPath);
    QString cleanupError;
    if (!m_exportManifestPath.isEmpty()
        && !ExportArtifactManifest::cleanupOwned(m_exportManifestPath, &cleanupError)) {
        m_exportDiagnosticLog.append(QStringLiteral("Owned export cleanup deferred: %1").arg(cleanupError));
    }
    m_exportManifestPath.clear();
    m_exportSupervisor.reset();
    m_exportProcess.reset();
    m_exportConfig.reset();
    m_exportOutputTransaction.reset();
    emit exportChanged();
    if (m_quitAfterExport) {
        m_quitAfterExport = false;
        requestQuit();
    }
}

void AppController::saveWindowState(const int x, const int y, const int width, const int height)
{
    m_settings.setValue("window/x", x);
    m_settings.setValue("window/y", y);
    m_settings.setValue("window/width", width);
    m_settings.setValue("window/height", height);
    m_settings.sync();
}

void AppController::saveAnalysisWindowState(
    const int x,
    const int y,
    const int width,
    const int height,
    const int sidebarWidth,
    const int videoHeight)
{
    m_settings.setValue("analysis/windowX", x);
    m_settings.setValue("analysis/windowY", y);
    m_settings.setValue("analysis/windowWidth", width);
    m_settings.setValue("analysis/windowHeight", height);
    m_settings.setValue("analysis/sidebarWidth", sidebarWidth);
    m_settings.setValue("analysis/videoHeight", videoHeight);
    m_settings.sync();
}

void AppController::setPlaybackTime(const double seconds)
{
    if (qFuzzyCompare(m_playbackTime, seconds)) {
        return;
    }
    m_playbackTime = seconds;
    m_previewRenderContext.setTime(seconds);
    emit playbackTimeChanged();
    emit liveValuesChanged();
}

void AppController::setSyncOffset(const double seconds)
{
    if (!std::isfinite(seconds) || qFuzzyCompare(m_sync.offset, seconds)) {
        return;
    }
    m_sync.offset = seconds;
    m_previewRenderContext.setSyncTransform(m_sync);
    saveSessionSettings();
    emit syncChanged();
    emit liveValuesChanged();
    markPersistentChange();
}

void AppController::setTimeScale(const double scale)
{
    if (!std::isfinite(scale) || scale <= 0.0 || qFuzzyCompare(m_sync.timeScale, scale)) {
        return;
    }
    m_sync.timeScale = scale;
    m_previewRenderContext.setSyncTransform(m_sync);
    saveSessionSettings();
    emit syncChanged();
    emit liveValuesChanged();
    markPersistentChange();
}

void AppController::setAnalysisChannels(const QStringList &channels)
{
    QStringList normalized;
    for (const QString &channel : channels) {
        if (!channel.isEmpty() && !normalized.contains(channel)
            && (!m_session || m_session->channels.contains(channel))) {
            normalized.append(channel);
        }
        if (normalized.size() == 4) {
            break;
        }
    }
    if (normalized == m_analysisChannels) {
        return;
    }
    m_analysisChannels = normalized;
    m_settings.setValue("analysis/channels", m_analysisChannels);
    emit analysisChanged();
    markPersistentChange();
}

void AppController::setAnalysisVisible(const bool visible)
{
    if (visible == m_analysisVisible) {
        return;
    }
    m_analysisVisible = visible;
    m_settings.setValue("analysis/visible", visible);
    emit analysisChanged();
    markPersistentChange();
}

QVariant AppController::semanticValue(const QString &alias) const
{
    if (!m_session) {
        return {};
    }
    const auto value = m_session->valueAt(alias, videoToTelemetryTime(m_playbackTime, m_sync));
    return value ? QVariant(*value) : QVariant();
}

void AppController::setStatus(QString status)
{
    if (m_statusText == status) {
        return;
    }
    AppLog::info(QStringLiteral("Status: %1").arg(status));
    m_statusText = std::move(status);
    emit statusTextChanged();
}

void AppController::saveSessionSettings()
{
    m_settings.setValue("sync/offset", m_sync.offset);
    m_settings.setValue("sync/timeScale", m_sync.timeScale);
    m_settings.sync();
}

void AppController::saveWidgetSettings()
{
    const QJsonDocument document(
        QJsonObject{{"schemaVersion", 2}, {"widgets", m_widgetModel.toJson()}});
    m_settings.setValue("editor/widgets", document.toJson(QJsonDocument::Compact));
    m_settings.sync();
}

void AppController::markPersistentChange()
{
    if (m_suppressDirtyTracking) {
        return;
    }
    m_documentState.markChanged();
    emit documentStateChanged();
}

void AppController::beginDestructiveAction(
    const ProjectDocumentState::DestructiveAction action, const QUrl &openUrl)
{
    if (action == ProjectDocumentState::DestructiveAction::OpenProject) {
        if (!openUrl.isLocalFile() || openUrl.toLocalFile().isEmpty()) {
            setStatus("Project error: choose a local .fetproject file.");
            return;
        }
        m_pendingOpenProject = openUrl;
    }
    const auto result = m_documentState.request(action);
    emit destructiveActionChanged();
    if (result == ProjectDocumentState::RequestResult::ContinueImmediately) {
        performPendingDestructiveAction();
    } else {
        AppLog::info(QStringLiteral("Dirty project decision requested for: %1")
                         .arg(ProjectDocumentState::actionName(action)));
    }
}

void AppController::performPendingDestructiveAction()
{
    const ProjectDocumentState::DestructiveAction action = m_documentState.takePendingAction();
    const QUrl openUrl = m_pendingOpenProject;
    m_pendingOpenProject = QUrl();
    emit destructiveActionChanged();
    switch (action) {
    case ProjectDocumentState::DestructiveAction::NewProject:
        performClearProject();
        break;
    case ProjectDocumentState::DestructiveAction::OpenProject:
        performOpenProject(openUrl);
        break;
    case ProjectDocumentState::DestructiveAction::Quit:
        AppLog::info(QStringLiteral("Quit approved"));
        emit quitApproved();
        break;
    case ProjectDocumentState::DestructiveAction::None:
        break;
    }
}

void AppController::restoreSources()
{
    const QString videoPath = m_settings.value("sources/video").toString();
    const QString vboPath = m_settings.value("sources/vbo").toString();
    if (videoPath.isEmpty() && vboPath.isEmpty()) {
        return;
    }
    const quint64 generation = beginSourceGeneration();
    if (!videoPath.isEmpty()) {
        if (QFileInfo(videoPath).isFile()) {
            startVideoProbe(normalizedSourcePath(videoPath), generation, false);
        } else {
            m_videoLoadState = QStringLiteral("error");
            emit sourceLoadStateChanged();
            setStatus(QStringLiteral("Could not restore video: %1").arg(videoPath));
        }
    }
    if (!vboPath.isEmpty()) {
        if (QFileInfo(vboPath).isFile()) {
            startVboLoad(normalizedSourcePath(vboPath), generation, false);
        } else {
            m_vboLoadState = QStringLiteral("error");
            emit sourceLoadStateChanged();
            setStatus(QStringLiteral("Could not restore VBO: %1").arg(vboPath));
        }
    }
    setStatus("Restoring previous sources…");
}

void AppController::reconcileAnalysisChannels()
{
    if (!m_session) {
        return;
    }
    QStringList channels;
    for (const QString &channel : std::as_const(m_analysisChannels)) {
        if (m_session->channels.contains(channel) && !channels.contains(channel)) {
            channels.append(channel);
        }
    }
    for (const QString &alias : {QStringLiteral("speed"), QStringLiteral("rpm"),
                                 QStringLiteral("throttle"), QStringLiteral("brake")}) {
        const QString resolved = m_session->aliases.value(alias);
        if (!resolved.isEmpty() && !channels.contains(resolved)) {
            channels.append(resolved);
        }
        if (channels.size() >= 3) {
            break;
        }
    }
    for (const QString &channel : m_session->channelNames()) {
        if (channels.size() >= 3) {
            break;
        }
        if (!channels.contains(channel)) {
            channels.append(channel);
        }
    }
    setAnalysisChannels(channels);
}

QString AppController::syncCandidateLevelName(const double confidence)
{
    switch (syncConfidenceLevel(confidence)) {
    case SyncConfidenceLevel::High:
        return QStringLiteral("high");
    case SyncConfidenceLevel::Medium:
        return QStringLiteral("medium");
    case SyncConfidenceLevel::Low:
        return QStringLiteral("low");
    }
    return QStringLiteral("low");
}

} // namespace FlappedEar
