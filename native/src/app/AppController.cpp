#include "app/AppController.h"

#include "gopro/GoProTelemetrySource.h"
#include "sync/TelemetrySyncEngine.h"
#include "telemetry/VboParser.h"

#include <QFileInfo>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtConcurrent>
#include <QtGlobal>
#include <cmath>

namespace FlappedEar {

AppController::AppController(QObject *parent)
    : QObject(parent)
    , m_settings()
{
    m_sync.offset = m_settings.value("sync/offset", 0.0).toDouble();
    m_sync.timeScale = m_settings.value("sync/timeScale", 1.0).toDouble();
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
        setSyncOffset(result.candidate.offset);
        setTimeScale(result.candidate.timeScale);
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
        };
        emit syncCandidateChanged();
        setStatus(QStringLiteral("Auto sync applied: %1 s · correlation %2 · confidence %3% · %4 GPS samples")
                      .arg(result.candidate.offset, 0, 'f', 3)
                      .arg(result.candidate.diagnostics.correlation, 0, 'f', 3)
                      .arg(result.candidate.confidence * 100.0, 0, 'f', 0)
                      .arg(result.gpsSampleCount));
    });
    restoreSources();
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
QVariantMap AppController::syncCandidate() const { return m_syncCandidate; }
QVariant AppController::speed() const { return semanticValue("speed"); }
QVariant AppController::rpm() const { return semanticValue("rpm"); }
QVariant AppController::heartRate() const { return semanticValue("heartRate"); }
WidgetModel *AppController::widgetModel() { return &m_widgetModel; }
QVariantList AppController::trackPoints() const { return m_trackPoints; }
QVariantMap AppController::currentTrackPoint() const
{
    if (!m_session) {
        return {};
    }
    const auto point = FlappedEar::currentTrackPoint(
        *m_session, videoToTelemetryTime(m_playbackTime, m_sync), m_trackGeometry);
    return point ? QVariantMap{{"x", point->x()}, {"y", point->y()}} : QVariantMap();
}
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
        m_trackPoints.clear();
        m_trackPoints.reserve(m_trackGeometry.points.size());
        for (const QPointF &point : m_trackGeometry.points) {
            m_trackPoints.append(point);
        }
        m_telemetryPath = QFileInfo(path).absoluteFilePath();
        m_settings.setValue("sources/vbo", m_telemetryPath);
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
    m_telemetryPath.clear();
    m_session.reset();
    m_trackGeometry = {};
    m_trackPoints.clear();
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
    m_settings.setValue("project/path", file.fileName());
    setStatus(QStringLiteral("Project opened: %1").arg(QFileInfo(file).fileName()));
}

void AppController::saveProject(const QUrl &url)
{
    QString path = url.toLocalFile();
    if (!path.endsWith(".fetproject", Qt::CaseInsensitive)) {
        path.append(".fetproject");
    }
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        setStatus(QStringLiteral("Project save error: %1").arg(file.errorString()));
        return;
    }
    QJsonObject project = m_projectTemplate;
    project.insert("version", 2);
    project.insert("videoPath", m_videoSource.toLocalFile());
    project.insert("vboPath", m_telemetryPath);
    project.insert(
        "sync", QJsonObject{{"offset", m_sync.offset}, {"timeScale", m_sync.timeScale}});
    project.insert("scene", QJsonObject{{"widgets", m_widgetModel.toJson()}});
    if (!project.contains("mapSettings")) {
        project.insert("mapSettings", QJsonObject{{"providerId", "none"}});
    }
    if (!project.contains("exportSettings")) {
        project.insert("exportSettings", QJsonObject{{"quality", "high"}});
    }
    file.write(QJsonDocument(project).toJson(QJsonDocument::Indented));
    file.close();
    m_projectTemplate = project;
    m_settings.setValue("project/path", path);
    setStatus(QStringLiteral("Project saved: %1").arg(QFileInfo(path).fileName()));
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

void AppController::saveWindowState(const int x, const int y, const int width, const int height)
{
    m_settings.setValue("window/x", x);
    m_settings.setValue("window/y", y);
    m_settings.setValue("window/width", width);
    m_settings.setValue("window/height", height);
    m_settings.sync();
}

void AppController::setPlaybackTime(const double seconds)
{
    if (qFuzzyCompare(m_playbackTime, seconds)) {
        return;
    }
    m_playbackTime = seconds;
    emit playbackTimeChanged();
    emit liveValuesChanged();
}

void AppController::setSyncOffset(const double seconds)
{
    if (!std::isfinite(seconds) || qFuzzyCompare(m_sync.offset, seconds)) {
        return;
    }
    m_sync.offset = seconds;
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
    saveSessionSettings();
    emit syncChanged();
    emit liveValuesChanged();
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
    }
    const QString vboPath = m_settings.value("sources/vbo").toString();
    if (!QFileInfo::exists(vboPath)) {
        return;
    }
    try {
        m_session = std::make_unique<TelemetrySession>(VboParser::parseFile(vboPath));
        m_trackGeometry = buildTrackGeometry(*m_session);
        m_trackPoints.clear();
        m_trackPoints.reserve(m_trackGeometry.points.size());
        for (const QPointF &point : m_trackGeometry.points) {
            m_trackPoints.append(point);
        }
        m_telemetryPath = vboPath;
        m_statusText = QStringLiteral("Previous native session restored.");
    } catch (const std::exception &error) {
        m_statusText = QStringLiteral("Could not restore VBO: %1").arg(error.what());
    }
}

} // namespace FlappedEar
