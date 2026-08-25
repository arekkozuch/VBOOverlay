#pragma once

#include "telemetry/TelemetrySession.h"
#include "telemetry/TelemetryRenderContext.h"
#include "telemetry/TrackGeometry.h"
#include "export/MediaProbe.h"
#include "export/ExportDiagnostics.h"
#include "export/ExportOutputTransaction.h"
#include "export/ExportProcessSupervisor.h"
#include "export/PersistentExportLog.h"
#include "export/BoundedProcessOutput.h"
#include "sync/TelemetrySyncEngine.h"
#include "widgets/WidgetModel.h"
#include "project/ProjectWriter.h"
#include "project/ProjectDocumentState.h"
#include "project/ProjectRecoveryStore.h"
#include "project/ProjectSourceReference.h"

#include <QFutureWatcher>
#include <QProcess>
#include <QTemporaryFile>
#include <QObject>
#include <QJsonObject>
#include <QSettings>
#include <QTimer>
#include <QUrl>
#include <QVariant>
#include <atomic>
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
    Q_PROPERTY(QUrl projectPath READ projectPath NOTIFY documentStateChanged)
    Q_PROPERTY(bool dirty READ dirty NOTIFY documentStateChanged)
    Q_PROPERTY(quint64 lastSavedRevision READ lastSavedRevision NOTIFY documentStateChanged)
    Q_PROPERTY(QString pendingDestructiveAction READ pendingDestructiveAction NOTIFY destructiveActionChanged)
    Q_PROPERTY(QString videoLoadState READ videoLoadState NOTIFY sourceLoadStateChanged)
    Q_PROPERTY(QString vboLoadState READ vboLoadState NOTIFY sourceLoadStateChanged)
    Q_PROPERTY(bool projectLoading READ projectLoading NOTIFY projectLoadChanged)
    Q_PROPERTY(QString projectLoadStage READ projectLoadStage NOTIFY projectLoadChanged)
    Q_PROPERTY(QString projectLoadError READ projectLoadError NOTIFY projectLoadChanged)
    Q_PROPERTY(bool recoveryPending READ recoveryPending NOTIFY recoveryChanged)
    Q_PROPERTY(bool recoveryDegraded READ recoveryDegraded NOTIFY recoveryChanged)
    Q_PROPERTY(QString recoveryError READ recoveryError NOTIFY recoveryChanged)
    Q_PROPERTY(QString sourceMismatchType READ sourceMismatchType NOTIFY sourceMismatchChanged)
    Q_PROPERTY(QString sourceMismatchCandidateName READ sourceMismatchCandidateName NOTIFY sourceMismatchChanged)
    Q_PROPERTY(QString selectedTemplateId READ selectedTemplateId NOTIFY templateUiStateChanged)
    Q_PROPERTY(QString activeTemplateId READ activeTemplateId NOTIFY templateUiStateChanged)

public:
    explicit AppController(QObject *parent = nullptr, QString recoveryPath = {});
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
    [[nodiscard]] QUrl projectPath() const;
    [[nodiscard]] bool dirty() const;
    [[nodiscard]] quint64 lastSavedRevision() const;
    [[nodiscard]] QString pendingDestructiveAction() const;
    [[nodiscard]] QString videoLoadState() const;
    [[nodiscard]] QString vboLoadState() const;
    [[nodiscard]] bool projectLoading() const;
    [[nodiscard]] QString projectLoadStage() const;
    [[nodiscard]] QString projectLoadError() const;
    [[nodiscard]] bool recoveryPending() const;
    [[nodiscard]] bool recoveryDegraded() const;
    [[nodiscard]] QString recoveryError() const;
    [[nodiscard]] QString sourceMismatchType() const;
    [[nodiscard]] QString sourceMismatchCandidateName() const;
    [[nodiscard]] QString selectedTemplateId() const;
    [[nodiscard]] QString activeTemplateId() const;

    Q_INVOKABLE void loadVideo(const QUrl &url);
    Q_INVOKABLE void loadVbo(const QUrl &url);
    Q_INVOKABLE void relinkVideo(const QUrl &url);
    Q_INVOKABLE void relinkVbo(const QUrl &url);
    Q_INVOKABLE void resolveSourceMismatch(bool acceptReplacement);
    Q_INVOKABLE QString valueText(const QString &channelName, int decimals = 2) const;
    Q_INVOKABLE QVariant telemetryValue(const QString &channelName) const;
    Q_INVOKABLE QVariantMap telemetrySeries(
        const QString &channelName, double videoStart, double videoEnd, int maximumPoints) const;
    Q_INVOKABLE void toggleAnalysisChannel(const QString &channelName);
    Q_INVOKABLE void requestNewProject();
    Q_INVOKABLE void requestOpenProject(const QUrl &url);
    Q_INVOKABLE void requestQuit();
    Q_INVOKABLE void resolveDestructiveAction(const QString &decision);
    Q_INVOKABLE void cancelPendingDestructiveAction();
    Q_INVOKABLE bool saveCurrentProject();
    Q_INVOKABLE bool saveProject(const QUrl &url);
    Q_INVOKABLE void resolveStartupRecovery(const QString &decision);
    Q_INVOKABLE void autoSync();
    Q_INVOKABLE void applySyncCandidate();
    Q_INVOKABLE void ignoreSyncCandidate();
    Q_INVOKABLE bool startExport(
        const QUrl &output,
        int outputWidth, int outputHeight, qint64 frameRateNumerator, qint64 frameRateDenominator,
        qint64 videoBitrate,
        bool audioEnabled,
        bool customRange,
        double rangeStart,
        double rangeEnd,
        bool overwriteAllowed = false);
    Q_INVOKABLE QVariantMap exportFormatOptions() const;
    Q_INVOKABLE qint64 recommendedExportBitrate(int width, int height, qint64 numerator, qint64 denominator, const QString &quality) const;
    Q_INVOKABLE qint64 estimateExportSize(qint64 videoBitrate, bool audioEnabled, double seconds) const;
    Q_INVOKABLE QString formatEstimatedExportSize(qint64 bytes) const;
    Q_INVOKABLE void cancelExport();
    Q_INVOKABLE void cancelExportAndQuit();
    Q_INVOKABLE void dismissExportProgress();
    Q_INVOKABLE void copyExportDiagnostics();
    Q_INVOKABLE void saveWindowState(int x, int y, int width, int height);
    Q_INVOKABLE void saveAnalysisWindowState(
        int x, int y, int width, int height, int sidebarWidth, int videoHeight);
    Q_INVOKABLE int templateIndexForId(const QString &templateId) const;
    Q_INVOKABLE void selectTemplate(const QString &templateId);
    Q_INVOKABLE void reconcileTemplateSelection();
    Q_INVOKABLE bool applyTemplate(const QString &templateId);
    Q_INVOKABLE void markTemplateActive(const QString &templateId);
    Q_INVOKABLE bool saveActiveTemplate();

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
    void documentStateChanged();
    void destructiveActionChanged();
    void sourceLoadStateChanged();
    void projectLoadChanged();
    void recoveryChanged();
    void sourceMismatchChanged();
    void templateUiStateChanged();
    void saveAsRequested();
    void quitApproved();

private:
    struct AutoSyncResult {
        bool success = false;
        bool cancelled = false;
        QString error;
        SyncCandidate candidate;
        qsizetype packetCount = 0;
        qsizetype gpsSampleCount = 0;
        QString gpsStream;
        quint64 generation = 0;
        QString videoPath;
        QString vboPath;
    };

    struct VideoProbeResult {
        bool success = false;
        bool cancelled = false;
        QString path;
        MediaInfo mediaInfo;
        QString error;
        quint64 generation = 0;
        QJsonObject fingerprint;
        QJsonObject expectedFingerprint;
        bool relink = false;
    };

    struct VboLoadResult {
        bool success = false;
        bool cancelled = false;
        QString path;
        TelemetrySession session;
        TrackGeometry geometry;
        QString error;
        quint64 generation = 0;
        QJsonObject fingerprint;
        QJsonObject expectedFingerprint;
        bool relink = false;
    };

    struct ProjectLoadResult {
        bool success = false;
        bool cancelled = false;
        QString projectPath;
        QJsonObject project;
        QJsonArray widgets;
        QStringList analysisChannels;
        SyncTransform sync;
        ProjectSourceReference videoReference;
        ProjectSourceReference vboReference;
        QString resolvedVideoPath;
        QString resolvedVboPath;
        QString error;
        quint64 generation = 0;
        quint64 documentRevisionAtStart = 0;
        bool recovered = false;
        quint64 recoveredRevision = 0;
        quint64 recoveredLastSavedRevision = 0;
    };

    [[nodiscard]] QVariant semanticValue(const QString &alias) const;
    void setStatus(QString status);
    [[nodiscard]] quint64 beginSourceGeneration();
    void cancelSourceJobs();
    void startVideoProbe(const QString &path, quint64 generation, bool markDocumentDirty,
                         QJsonObject expectedFingerprint = {}, bool relink = false);
    void startVboLoad(const QString &path, quint64 generation, bool markDocumentDirty,
                      QJsonObject expectedFingerprint = {}, bool relink = false);
    void commitProjectLoad(const ProjectLoadResult &result);
    void commitVideoProbe(const VideoProbeResult &result, bool markDocumentDirty);
    void commitVboLoad(const VboLoadResult &result, bool markDocumentDirty);
    void setProjectLoadState(bool loading, QString stage = {}, QString error = {});
    [[nodiscard]] static QString normalizedSourcePath(const QString &path);
    [[nodiscard]] static QVariantList trackPointsFor(const TrackGeometry &geometry);
    void markPersistentChange();
    [[nodiscard]] QJsonObject currentProjectObject(const QString &projectPath = {}) const;
    bool beginProjectLoad(QString projectPath, const QJsonObject &project,
                          bool recovered = false, quint64 recoveredRevision = 0,
                          quint64 recoveredLastSavedRevision = 0);
    void restoreStartupState();
    void scheduleRecoveryWrite();
    void writeRecoverySnapshot();
    bool clearRecovery(const QString &reason);
    void retireLegacyDocumentSettings();
    void performClearProject();
    bool performOpenProject(const QUrl &url);
    void beginDestructiveAction(ProjectDocumentState::DestructiveAction action, const QUrl &openUrl = {});
    void performPendingDestructiveAction();
    void reconcileAnalysisChannels();
    void clearActiveTemplate();
    void handleExportOutput();
    void finishExport(int exitCode, QProcess::ExitStatus exitStatus);
    void appendExportDiagnostic(const QString &entry);
    void appendExportLifecycle(const QString &event);
    void finishPersistentExportLog(const QString &result, const QString &error = {});
    [[nodiscard]] static QString syncCandidateLevelName(double confidence);

    QSettings m_settings;
    QUrl m_videoSource;
    QString m_telemetryPath;
    ProjectSourceReference m_videoReference;
    ProjectSourceReference m_vboReference;
    QString m_statusText = QStringLiteral("Open a video and VBO to begin.");
    std::unique_ptr<TelemetrySession> m_session;
    WidgetModel m_widgetModel;
    TrackGeometry m_trackGeometry;
    TelemetryRenderContext m_previewRenderContext;
    QVariantList m_trackPoints;
    QJsonObject m_projectTemplate;
    ProjectWriter m_projectWriter;
    ProjectDocumentState m_documentState;
    ProjectRecoveryStore m_recoveryStore;
    ProjectRecoverySnapshot m_pendingRecovery;
    QTimer m_recoveryTimer;
    bool m_recoveryPending = false;
    bool m_recoveryDegraded = false;
    QString m_recoveryError;
    QUrl m_pendingOpenProject;
    bool m_suppressDirtyTracking = false;
    double m_playbackTime = 0.0;
    SyncTransform m_sync;
    QFutureWatcher<AutoSyncResult> m_syncWatcher;
    QFutureWatcher<VideoProbeResult> m_videoProbeWatcher;
    QFutureWatcher<VboLoadResult> m_vboLoadWatcher;
    QFutureWatcher<ProjectLoadResult> m_projectLoadWatcher;
    quint64 m_sourceGeneration = 0;
    std::shared_ptr<std::atomic_bool> m_videoProbeCancellation;
    std::shared_ptr<std::atomic_bool> m_vboLoadCancellation;
    std::shared_ptr<std::atomic_bool> m_projectLoadCancellation;
    std::shared_ptr<std::atomic_bool> m_syncCancellation;
    bool m_videoLoadMarksDocumentDirty = true;
    bool m_vboLoadMarksDocumentDirty = true;
    QString m_videoLoadState = QStringLiteral("idle");
    QString m_vboLoadState = QStringLiteral("idle");
    QString m_pendingVideoPath;
    QString m_pendingVboPath;
    VideoProbeResult m_pendingMismatchVideo;
    VboLoadResult m_pendingMismatchVbo;
    QString m_sourceMismatchType;
    QString m_selectedTemplateId;
    QString m_activeTemplateId;
    bool m_projectLoading = false;
    QString m_projectLoadStage;
    QString m_projectLoadError;
    std::unique_ptr<QProcess> m_exportProcess;
    std::unique_ptr<ExportProcessSupervisor> m_exportSupervisor;
    std::unique_ptr<QTemporaryFile> m_exportConfig;
    std::unique_ptr<ExportOutputTransaction> m_exportOutputTransaction;
    QByteArray m_exportStdout;
    BoundedProcessOutput m_exportStderr{BoundedProcessOutput::Mode::DiagnosticTail,
                                        ProcessOutputLimits::ffmpegDiagnosticTailBytes};
    QString m_exportCancelPath;
    QString m_exportSupervisionReadyPath;
    QString m_exportManifestPath;
    int m_exportProgress = 0;
    QString m_exportState = QStringLiteral("idle");
    QString m_exportError;
    MediaInfo m_exportSourceInfo;
    QVariantMap m_exportMetrics;
    QVariantMap m_exportProgressInfo;
    BoundedDiagnosticLog m_exportDiagnosticLog{1500};
    std::unique_ptr<PersistentExportLog> m_persistentExportLog;
    bool m_exportProgressVisible = false;
    bool m_quitAfterExport = false;
    QVariantMap m_syncCandidate;
    QStringList m_analysisChannels;
    bool m_analysisVisible = false;
};

} // namespace FlappedEar
