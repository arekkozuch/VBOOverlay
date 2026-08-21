#pragma once

#include "telemetry/TelemetrySession.h"
#include "telemetry/TelemetryRenderContext.h"
#include "telemetry/TrackGeometry.h"
#include "export/MediaProbe.h"
#include "export/ExportDiagnostics.h"
#include "export/ExportOutputTransaction.h"
#include "sync/TelemetrySyncEngine.h"
#include "widgets/WidgetModel.h"

#include <QFutureWatcher>
#include <QProcess>
#include <QTemporaryFile>
#include <QObject>
#include <QJsonObject>
#include <QSettings>
#include <QUrl>
#include <QVariant>
#include <memory>

namespace FlappedEar {

class AppController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QUrl videoSource READ videoSource NOTIFY videoSourceChanged)
    Q_PROPERTY(QString videoName READ videoName NOTIFY videoSourceChanged)
    Q_PROPERTY(QString telemetryName READ telemetryName NOTIFY telemetryChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusTextChanged)
    Q_PROPERTY(QStringList channelNames READ channelNames NOTIFY telemetryChanged)
    Q_PROPERTY(qsizetype sampleCount READ sampleCount NOTIFY telemetryChanged)
    Q_PROPERTY(double telemetryDuration READ telemetryDuration NOTIFY telemetryChanged)
    Q_PROPERTY(double playbackTime READ playbackTime WRITE setPlaybackTime NOTIFY playbackTimeChanged)
    Q_PROPERTY(double syncOffset READ syncOffset WRITE setSyncOffset NOTIFY syncChanged)
    Q_PROPERTY(double timeScale READ timeScale WRITE setTimeScale NOTIFY syncChanged)
    Q_PROPERTY(bool syncing READ syncing NOTIFY syncingChanged)
    Q_PROPERTY(bool exporting READ exporting NOTIFY exportChanged)
    Q_PROPERTY(int exportProgress READ exportProgress NOTIFY exportChanged)
    Q_PROPERTY(QString exportState READ exportState NOTIFY exportChanged)
    Q_PROPERTY(QString exportError READ exportError NOTIFY exportChanged)
    Q_PROPERTY(QVariantMap exportSourceInfo READ exportSourceInfo NOTIFY exportChanged)
    Q_PROPERTY(QVariantMap exportMetrics READ exportMetrics NOTIFY exportChanged)
    Q_PROPERTY(QVariantMap exportProgressInfo READ exportProgressInfo NOTIFY exportChanged)
    Q_PROPERTY(bool exportProgressVisible READ exportProgressVisible NOTIFY exportChanged)
    Q_PROPERTY(QString exportDiagnosticLog READ exportDiagnosticLog NOTIFY exportChanged)
    Q_PROPERTY(QString fixedFontFamily READ fixedFontFamily CONSTANT)
    Q_PROPERTY(QVariantMap syncCandidate READ syncCandidate NOTIFY syncCandidateChanged)
    Q_PROPERTY(QVariant speed READ speed NOTIFY liveValuesChanged)
    Q_PROPERTY(QVariant rpm READ rpm NOTIFY liveValuesChanged)
    Q_PROPERTY(QVariant heartRate READ heartRate NOTIFY liveValuesChanged)
    Q_PROPERTY(TelemetryRenderContext *renderContext READ renderContext CONSTANT)
    Q_PROPERTY(WidgetModel *widgetModel READ widgetModel CONSTANT)
    Q_PROPERTY(QVariantList trackPoints READ trackPoints NOTIFY telemetryChanged)
    Q_PROPERTY(QVariantMap currentTrackPoint READ currentTrackPoint NOTIFY liveValuesChanged)
    Q_PROPERTY(QStringList analysisChannels READ analysisChannels WRITE setAnalysisChannels NOTIFY analysisChanged)
    Q_PROPERTY(bool analysisVisible READ analysisVisible WRITE setAnalysisVisible NOTIFY analysisChanged)
    Q_PROPERTY(int analysisWindowX READ analysisWindowX CONSTANT)
    Q_PROPERTY(int analysisWindowY READ analysisWindowY CONSTANT)
    Q_PROPERTY(int analysisWindowWidth READ analysisWindowWidth CONSTANT)
    Q_PROPERTY(int analysisWindowHeight READ analysisWindowHeight CONSTANT)
    Q_PROPERTY(int analysisSidebarWidth READ analysisSidebarWidth CONSTANT)
    Q_PROPERTY(int analysisVideoHeight READ analysisVideoHeight CONSTANT)
    Q_PROPERTY(int windowX READ windowX CONSTANT)
    Q_PROPERTY(int windowY READ windowY CONSTANT)
    Q_PROPERTY(int windowWidth READ windowWidth CONSTANT)
    Q_PROPERTY(int windowHeight READ windowHeight CONSTANT)

public:
    explicit AppController(QObject *parent = nullptr);
    ~AppController() override;

    [[nodiscard]] QUrl videoSource() const;
    [[nodiscard]] QString videoName() const;
    [[nodiscard]] QString telemetryName() const;
    [[nodiscard]] QString statusText() const;
    [[nodiscard]] QStringList channelNames() const;
    [[nodiscard]] qsizetype sampleCount() const;
    [[nodiscard]] double telemetryDuration() const;
    [[nodiscard]] double playbackTime() const;
    [[nodiscard]] double syncOffset() const;
    [[nodiscard]] double timeScale() const;
    [[nodiscard]] bool syncing() const;
    [[nodiscard]] bool exporting() const;
    [[nodiscard]] int exportProgress() const;
    [[nodiscard]] QString exportState() const;
    [[nodiscard]] QString exportError() const;
    [[nodiscard]] QVariantMap exportSourceInfo() const;
    [[nodiscard]] QVariantMap exportMetrics() const;
    [[nodiscard]] QVariantMap exportProgressInfo() const;
    [[nodiscard]] bool exportProgressVisible() const;
    [[nodiscard]] QString exportDiagnosticLog() const;
    [[nodiscard]] QString fixedFontFamily() const;
    [[nodiscard]] QVariantMap syncCandidate() const;
    [[nodiscard]] QVariant speed() const;
    [[nodiscard]] QVariant rpm() const;
    [[nodiscard]] QVariant heartRate() const;
    [[nodiscard]] TelemetryRenderContext *renderContext();
    [[nodiscard]] WidgetModel *widgetModel();
    [[nodiscard]] QVariantList trackPoints() const;
    [[nodiscard]] QVariantMap currentTrackPoint() const;
    [[nodiscard]] QStringList analysisChannels() const;
    [[nodiscard]] bool analysisVisible() const;
    [[nodiscard]] int analysisWindowX() const;
    [[nodiscard]] int analysisWindowY() const;
    [[nodiscard]] int analysisWindowWidth() const;
    [[nodiscard]] int analysisWindowHeight() const;
    [[nodiscard]] int analysisSidebarWidth() const;
    [[nodiscard]] int analysisVideoHeight() const;
    [[nodiscard]] int windowX() const;
    [[nodiscard]] int windowY() const;
    [[nodiscard]] int windowWidth() const;
    [[nodiscard]] int windowHeight() const;

    Q_INVOKABLE void loadVideo(const QUrl &url);
    Q_INVOKABLE void loadVbo(const QUrl &url);
    Q_INVOKABLE void clearProject();
    Q_INVOKABLE QString valueText(const QString &channelName, int decimals = 2) const;
    Q_INVOKABLE QVariant telemetryValue(const QString &channelName) const;
    Q_INVOKABLE QVariantMap telemetrySeries(
        const QString &channelName, double videoStart, double videoEnd, int maximumPoints) const;
    Q_INVOKABLE void toggleAnalysisChannel(const QString &channelName);
    Q_INVOKABLE void openProject(const QUrl &url);
    Q_INVOKABLE void saveProject(const QUrl &url);
    Q_INVOKABLE void autoSync();
    Q_INVOKABLE void applySyncCandidate();
    Q_INVOKABLE void ignoreSyncCandidate();
    Q_INVOKABLE bool startExport(
        const QUrl &output,
        const QString &quality,
        bool audioEnabled,
        bool customRange,
        double rangeStart,
        double rangeEnd,
        bool overwriteAllowed = false);
    Q_INVOKABLE void cancelExport();
    Q_INVOKABLE void cancelExportAndQuit();
    Q_INVOKABLE void dismissExportProgress();
    Q_INVOKABLE void copyExportDiagnostics();
    Q_INVOKABLE void saveWindowState(int x, int y, int width, int height);
    Q_INVOKABLE void saveAnalysisWindowState(
        int x, int y, int width, int height, int sidebarWidth, int videoHeight);

public slots:
    void setPlaybackTime(double seconds);
    void setSyncOffset(double seconds);
    void setTimeScale(double scale);
    void setAnalysisChannels(const QStringList &channels);
    void setAnalysisVisible(bool visible);

signals:
    void videoSourceChanged();
    void telemetryChanged();
    void statusTextChanged();
    void playbackTimeChanged();
    void syncChanged();
    void syncingChanged();
    void exportChanged();
    void syncCandidateChanged();
    void liveValuesChanged();
    void analysisChanged();

private:
    struct AutoSyncResult {
        bool success = false;
        QString error;
        SyncCandidate candidate;
        qsizetype packetCount = 0;
        qsizetype gpsSampleCount = 0;
        QString gpsStream;
    };

    [[nodiscard]] QVariant semanticValue(const QString &alias) const;
    void setStatus(QString status);
    void saveSessionSettings();
    void saveWidgetSettings();
    void restoreSources();
    void reconcileAnalysisChannels();
    void probeExportSource();
    void handleExportOutput();
    void finishExport(int exitCode, QProcess::ExitStatus exitStatus);
    [[nodiscard]] static QString syncCandidateLevelName(double confidence);

    QSettings m_settings;
    QUrl m_videoSource;
    QString m_telemetryPath;
    QString m_statusText = QStringLiteral("Open a video and VBO to begin.");
    std::unique_ptr<TelemetrySession> m_session;
    WidgetModel m_widgetModel;
    TrackGeometry m_trackGeometry;
    TelemetryRenderContext m_previewRenderContext;
    QVariantList m_trackPoints;
    QJsonObject m_projectTemplate;
    double m_playbackTime = 0.0;
    SyncTransform m_sync;
    QFutureWatcher<AutoSyncResult> m_syncWatcher;
    std::unique_ptr<QProcess> m_exportProcess;
    std::unique_ptr<QTemporaryFile> m_exportConfig;
    std::unique_ptr<ExportOutputTransaction> m_exportOutputTransaction;
    QByteArray m_exportStdout;
    QString m_exportCancelPath;
    int m_exportProgress = 0;
    QString m_exportState = QStringLiteral("idle");
    QString m_exportError;
    MediaInfo m_exportSourceInfo;
    QVariantMap m_exportMetrics;
    QVariantMap m_exportProgressInfo;
    BoundedDiagnosticLog m_exportDiagnosticLog{1500};
    bool m_exportProgressVisible = false;
    bool m_quitAfterExport = false;
    QVariantMap m_syncCandidate;
    QStringList m_analysisChannels;
    bool m_analysisVisible = true;
};

} // namespace FlappedEar
