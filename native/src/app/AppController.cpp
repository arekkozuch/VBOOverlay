#include "app/AppController.h"

#include "gopro/GoProTelemetrySource.h"
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
#include <QTimer>
#include <QtConcurrent>
#include <QtGlobal>
#include <algorithm>
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
    connect(&m_widgetModel, &WidgetModel::revisionChanged, this, &AppController::saveWidgetSettings);
    connect(&m_syncWatcher, &QFutureWatcher<AutoSyncResult>::finished, this, [this] {
        const AutoSyncResult result = m_syncWatcher.result();
        emit syncingChanged();
        if (!result.success) {
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
        setStatus(QStringLiteral("Auto sync %1: %2 s · correlation %3 · confidence %4% · %5 GPS samples")
                      .arg(automaticallyApplied ? QStringLiteral("applied")
                                                : QStringLiteral("candidate requires review"))
                      .arg(result.candidate.offset, 0, 'f', 3)
                      .arg(result.candidate.diagnostics.correlation, 0, 'f', 3)
                      .arg(result.candidate.confidence * 100.0, 0, 'f', 0)
                      .arg(result.gpsSampleCount));
    });
    restoreSources();
}

AppController::~AppController()
{
    if (exporting()) {
        QFile cancellationFile(m_exportCancelPath);
        if (cancellationFile.open(QIODevice::WriteOnly)) {
            cancellationFile.close();
        }
        m_exportProcess->terminate();
        if (!m_exportProcess->waitForFinished(5'000)) {
            m_exportProcess->kill();
            m_exportProcess->waitForFinished(5'000);
        }
    }
    m_exportOutputTransaction.reset();
    QFile::remove(m_exportCancelPath);
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

void AppController::loadVideo(const QUrl &url)
{
    const QString path = url.toLocalFile();
    const QFileInfo info(path);
    const QString extension = info.suffix().toLower();
    if (!info.isFile() || (extension != "mp4" && extension != "mov")) {
        setStatus("Choose an existing MP4 or MOV video.");
        return;
    }
    m_videoSource = QUrl::fromLocalFile(info.absoluteFilePath());
    probeExportSource();
    m_syncCandidate.clear();
    m_settings.setValue("sources/video", info.absoluteFilePath());
    emit videoSourceChanged();
    emit syncCandidateChanged();
    setStatus(QStringLiteral("Video opened: %1").arg(info.fileName()));
}

void AppController::loadVbo(const QUrl &url)
{
    const QString path = url.toLocalFile();
    try {
        auto session = std::make_unique<TelemetrySession>(VboParser::parseFile(path));
        m_session = std::move(session);
        m_syncCandidate.clear();
        m_trackGeometry = buildTrackGeometry(*m_session);
        m_previewRenderContext.setSession(m_session.get());
        m_previewRenderContext.setTrackGeometry(&m_trackGeometry);
        m_trackPoints.clear();
        m_trackPoints.reserve(m_trackGeometry.points.size());
        for (const QPointF &point : m_trackGeometry.points) {
            m_trackPoints.append(point);
        }
        m_telemetryPath = QFileInfo(path).absoluteFilePath();
        m_settings.setValue("sources/vbo", m_telemetryPath);
        reconcileAnalysisChannels();
        emit telemetryChanged();
        emit syncCandidateChanged();
        emit liveValuesChanged();
        setStatus(QStringLiteral("VBO opened: %1 samples, %2 numeric channels.")
                      .arg(m_session->sampleCount)
                      .arg(m_session->channels.size()));
    } catch (const std::exception &error) {
        setStatus(QStringLiteral("VBO error: %1").arg(error.what()));
    }
}

void AppController::clearProject()
{
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
    emit liveValuesChanged();
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

void AppController::openProject(const QUrl &url)
{
    QFile file(url.toLocalFile());
    if (!file.open(QIODevice::ReadOnly)) {
        setStatus(QStringLiteral("Project error: %1").arg(file.errorString()));
        return;
    }
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    const QJsonObject project = document.object();
    const QJsonObject scene = project.value("scene").toObject();
    if (project.value("version").toInt() != 2 || !scene.value("widgets").isArray()
        || !m_widgetModel.fromJson(scene.value("widgets").toArray())) {
        setStatus("Project error: unsupported or invalid .fetproject file.");
        return;
    }
    m_projectTemplate = project;
    const QJsonObject sync = project.value("sync").toObject();
    setSyncOffset(sync.value("offset").toDouble());
    setTimeScale(sync.value("timeScale").toDouble(1.0));
    const QString videoPath = project.value("videoPath").toString();
    const QString vboPath = project.value("vboPath").toString();
    if (!videoPath.isEmpty()) {
        loadVideo(QUrl::fromLocalFile(videoPath));
    }
    if (!vboPath.isEmpty()) {
        loadVbo(QUrl::fromLocalFile(vboPath));
    }
    const QJsonObject analysis = project.value("analysis").toObject();
    if (analysis.value("channels").isArray()) {
        QStringList channels;
        for (const QJsonValue &value : analysis.value("channels").toArray()) {
            channels.append(value.toString());
        }
        setAnalysisChannels(channels);
    }
    if (analysis.contains("visible")) {
        setAnalysisVisible(analysis.value("visible").toBool(true));
    }
    m_settings.setValue("project/path", file.fileName());
    setStatus(QStringLiteral("Project opened: %1").arg(QFileInfo(file).fileName()));
}

bool AppController::saveProject(const QUrl &url)
{
    QString path = url.toLocalFile();
    if (!path.endsWith(".fetproject", Qt::CaseInsensitive)) {
        path.append(".fetproject");
    }
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
        setStatus(QStringLiteral("Project save error: %1").arg(writeResult.error));
        return false;
    }
    m_projectTemplate = project;
    m_settings.setValue("project/path", path);
    setStatus(QStringLiteral("Project saved: %1").arg(QFileInfo(path).fileName()));
    return true;
}

void AppController::autoSync()
{
    if (m_syncWatcher.isRunning()) {
        return;
    }
    const QString videoPath = m_videoSource.toLocalFile();
    if (videoPath.isEmpty() || !m_session) {
        setStatus("Open both a GoPro video and VBO telemetry before auto sync.");
        return;
    }
    const TelemetrySession telemetry = *m_session;
    m_syncCandidate.clear();
    emit syncCandidateChanged();
    setStatus("Indexing GoPro telemetry and matching GPS speed…");
    m_syncWatcher.setFuture(QtConcurrent::run([videoPath, telemetry] {
        AutoSyncResult result;
        try {
            const GoProTelemetryResult videoTelemetry = GoProTelemetrySource::load(videoPath);
            result.candidate = TelemetrySyncEngine::synchronize(videoTelemetry.session, telemetry);
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
    m_syncCandidate.clear();
    emit syncCandidateChanged();
    setStatus("Synchronization candidate ignored; existing timing was retained.");
}

bool AppController::startExport(
    const QUrl &output,
    const QString &quality,
    const bool audioEnabled,
    const bool customRange,
    const double rangeStart,
    const double rangeEnd,
    const bool overwriteAllowed)
{
    if (exporting()) {
        return false;
    }
    const QString inputPath = m_videoSource.toLocalFile();
    const QString outputPath = output.toLocalFile();
    if (!m_session || inputPath.isEmpty() || m_telemetryPath.isEmpty() || outputPath.isEmpty()) {
        m_exportError = QStringLiteral("Open a video and VBO telemetry, then choose an output file.");
        m_exportState = QStringLiteral("failed");
        emit exportChanged();
        return false;
    }
    if (!m_exportSourceInfo.videoSize.isValid()) {
        probeExportSource();
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
        m_exportOutputTransaction.reset();
        emit exportChanged();
        return false;
    }
    m_exportConfig = std::make_unique<QTemporaryFile>(
        QDir::temp().filePath(QStringLiteral("flappedear-export-XXXXXX.json")));
    if (!m_exportConfig->open()) {
        m_exportError = QStringLiteral("Could not create temporary export configuration.");
        m_exportState = QStringLiteral("failed");
        m_exportOutputTransaction.reset();
        emit exportChanged();
        return false;
    }
    m_exportCancelPath = m_exportConfig->fileName() + QStringLiteral(".cancel");
    QFile::remove(m_exportCancelPath);
    const QJsonObject config = {
        {"inputPath", inputPath},
        {"outputPath", m_exportOutputTransaction->stagingPath()},
        {"vboPath", m_telemetryPath},
        {"widgets", m_widgetModel.toJson()},
        {"sync", QJsonObject{{"offset", m_sync.offset}, {"timeScale", m_sync.timeScale}}},
        {"quality", quality},
        {"audioEnabled", audioEnabled},
        {"startTime", startTime},
        {"endTime", endTime},
        {"cancelPath", m_exportCancelPath},
    };
    if (m_exportConfig->write(QJsonDocument(config).toJson(QJsonDocument::Compact)) < 0) {
        m_exportError = QStringLiteral("Could not write temporary export configuration.");
        m_exportState = QStringLiteral("failed");
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
                            {"audioLabel", audioEnabled ? QStringLiteral("AAC audio") : QStringLiteral("No audio")}};
    m_exportProgressVisible = true;
    m_exportState = QStringLiteral("starting");
    m_exportProcess = std::make_unique<QProcess>(this);
    m_exportProcess->setProcessChannelMode(QProcess::SeparateChannels);
    connect(m_exportProcess.get(), &QProcess::readyReadStandardOutput, this, &AppController::handleExportOutput);
    connect(
        m_exportProcess.get(),
        qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
        this,
        &AppController::finishExport);
    m_exportProcess->start(
        QCoreApplication::applicationFilePath(), {"--export-worker", m_exportConfig->fileName()});
    if (!m_exportProcess->waitForStarted(5'000)) {
        m_exportError = QStringLiteral("Could not start export worker: %1")
                            .arg(m_exportProcess->errorString());
        m_exportState = QStringLiteral("failed");
        m_exportProcess.reset();
        m_exportConfig.reset();
        m_exportOutputTransaction.reset();
        emit exportChanged();
        return false;
    }
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
        emit exportChanged();
        return;
    }
    cancellationFile.close();
    m_exportState = QStringLiteral("cancelling");
    m_exportProgressInfo.insert("stage", QStringLiteral("cancelling"));
    emit exportChanged();
}

void AppController::cancelExportAndQuit()
{
    if (!exporting()) {
        QCoreApplication::quit();
        return;
    }
    m_quitAfterExport = true;
    cancelExport();
    // A worker that cannot react to the cancellation file must not keep the
    // application alive indefinitely. Its process teardown also terminates
    // its FFmpeg child.
    QTimer::singleShot(7'000, this, [this] {
        if (!exporting()) {
            return;
        }
        m_exportProcess->terminate();
        QTimer::singleShot(3'000, this, [this] {
            if (exporting()) {
                m_exportProcess->kill();
                if (m_exportOutputTransaction) m_exportOutputTransaction->cleanup();
            }
        });
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
                                   QStringLiteral("outputBytes"), QStringLiteral("currentOperation"),
                                   QStringLiteral("operation"), QStringLiteral("stageElapsedMilliseconds"),
                                   QStringLiteral("totalElapsedMilliseconds"), QStringLiteral("stageDurations"),
                                   QStringLiteral("outputVideoCodec"), QStringLiteral("outputWidth"),
                                   QStringLiteral("outputHeight"), QStringLiteral("outputDuration"),
                                   QStringLiteral("outputAudioCodecs"), QStringLiteral("diagnostics"),
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
            m_exportProgress = 100;
            m_exportState = QStringLiteral("complete");
            m_exportError.clear();
            setStatus("HEVC export finished and passed validation.");
        } else {
            m_exportState = QStringLiteral("failed");
            m_exportError = commitError.isEmpty()
                ? QStringLiteral("Validated export could not be committed to its target.") : commitError;
            setStatus(QStringLiteral("Export failed: %1").arg(m_exportError));
        }
    } else {
        m_exportState = QStringLiteral("failed");
        if (m_exportError.isEmpty()) {
            m_exportError = workerError.isEmpty() ? QStringLiteral("Export worker failed.") : workerError;
        }
        setStatus(QStringLiteral("Export failed: %1").arg(m_exportError));
    }
    m_exportProgressInfo.insert("stage", m_exportState);
    m_exportProgressInfo.insert("progressPercent", m_exportProgress);
    QFile::remove(m_exportCancelPath);
    m_exportProcess.reset();
    m_exportConfig.reset();
    m_exportOutputTransaction.reset();
    emit exportChanged();
    if (m_quitAfterExport) {
        QCoreApplication::quit();
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
}

void AppController::setAnalysisVisible(const bool visible)
{
    if (visible == m_analysisVisible) {
        return;
    }
    m_analysisVisible = visible;
    m_settings.setValue("analysis/visible", visible);
    emit analysisChanged();
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

void AppController::restoreSources()
{
    const QString videoPath = m_settings.value("sources/video").toString();
    if (QFileInfo::exists(videoPath)) {
        m_videoSource = QUrl::fromLocalFile(videoPath);
        probeExportSource();
    }
    const QString vboPath = m_settings.value("sources/vbo").toString();
    if (!QFileInfo::exists(vboPath)) {
        return;
    }
    try {
        m_session = std::make_unique<TelemetrySession>(VboParser::parseFile(vboPath));
        m_trackGeometry = buildTrackGeometry(*m_session);
        m_previewRenderContext.setSession(m_session.get());
        m_previewRenderContext.setTrackGeometry(&m_trackGeometry);
        m_trackPoints.clear();
        m_trackPoints.reserve(m_trackGeometry.points.size());
        for (const QPointF &point : m_trackGeometry.points) {
            m_trackPoints.append(point);
        }
        m_telemetryPath = vboPath;
        reconcileAnalysisChannels();
        m_statusText = QStringLiteral("Previous native session restored.");
    } catch (const std::exception &error) {
        m_statusText = QStringLiteral("Could not restore VBO: %1").arg(error.what());
    }
}

void AppController::probeExportSource()
{
    m_exportSourceInfo = {};
    if (m_videoSource.isEmpty()) {
        return;
    }
    try {
        m_exportSourceInfo = MediaProbe::probe(m_videoSource.toLocalFile());
    } catch (const std::exception &) {
        // Video playback/import must remain available without FFmpeg tooling.
    }
    emit exportChanged();
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
