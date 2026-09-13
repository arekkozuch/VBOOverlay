#pragma once

#include "telemetry/LapTiming.h"
#include "telemetry/TelemetrySession.h"
#include "telemetry/TelemetryRenderContext.h"
#include "telemetry/TrackGeometry.h"
#include "telemetry/TelemetryImportPlan.h"
#include "telemetry/OutingLaps.h"
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
#include <QSet>
#include <atomic>
#include <memory>
#include <optional>

class TelemetryTests;

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
    Q_PROPERTY(QString lapTimingStatus READ lapTimingStatus NOTIFY telemetryChanged)
    Q_PROPERTY(QVariantList lapSummaries READ lapSummaries NOTIFY telemetryChanged)
    Q_PROPERTY(QVariantList lapNavigationSegments READ lapNavigationSegments NOTIFY lapNavigationChanged)
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
    Q_PROPERTY(QString eventName READ eventName NOTIFY documentStateChanged)
    Q_PROPERTY(QVariantList eventRuns READ eventRuns NOTIFY documentStateChanged)
    Q_PROPERTY(QString activeRunId READ activeRunId NOTIFY documentStateChanged)
    Q_PROPERTY(QString batchImportState READ batchImportState NOTIFY batchImportChanged)
    Q_PROPERTY(QString batchImportError READ batchImportError NOTIFY batchImportChanged)
    Q_PROPERTY(QStringList analysisImportMessages READ analysisImportMessages NOTIFY batchImportChanged)
    Q_PROPERTY(QVariantMap selectedOutingLap READ selectedOutingLap NOTIFY outingLapDetailChanged)
    Q_PROPERTY(QString outingLapDetailState READ outingLapDetailState NOTIFY outingLapDetailChanged)
    Q_PROPERTY(QString outingLapDetailError READ outingLapDetailError NOTIFY outingLapDetailChanged)
    Q_PROPERTY(QStringList outingLapChannels READ outingLapChannels WRITE setOutingLapChannels NOTIFY outingLapDetailChanged)
    Q_PROPERTY(QStringList outingLapAvailableChannels READ outingLapAvailableChannels NOTIFY outingLapDetailChanged)
    Q_PROPERTY(QVariantList outingLapTrack READ outingLapTrack NOTIFY outingLapDetailChanged)
    Q_PROPERTY(QVariantMap outingLapTrackPoint READ outingLapTrackPoint NOTIFY outingLapCursorChanged)
    Q_PROPERTY(double outingLapCursor READ outingLapCursor WRITE setOutingLapCursor NOTIFY outingLapCursorChanged)
    Q_PROPERTY(QVariantMap outingRanking READ outingRanking NOTIFY outingLapsChanged)
    Q_PROPERTY(QVariantMap outingProgression READ outingProgression NOTIFY outingLapsChanged)
    Q_PROPERTY(QVariantList outingCompatibilityGroups READ outingCompatibilityGroups NOTIFY outingLapsChanged)
    Q_PROPERTY(QString outingComparisonGroupId READ outingComparisonGroupId NOTIFY outingLapsChanged)
    Q_PROPERTY(QVariantList outingLaps READ outingLaps NOTIFY outingLapsChanged)
    Q_PROPERTY(QStringList outingLapMessages READ outingLapMessages NOTIFY outingLapsChanged)
    Q_PROPERTY(bool outingLapsLoading READ outingLapsLoading NOTIFY outingLapsChanged)
    Q_PROPERTY(QVariantList batchImportRows READ batchImportRows NOTIFY batchImportChanged)
    Q_PROPERTY(int batchImportProcessed READ batchImportProcessed NOTIFY batchImportChanged)
    Q_PROPERTY(int batchImportTotal READ batchImportTotal NOTIFY batchImportChanged)
    Q_PROPERTY(bool dirty READ dirty NOTIFY documentStateChanged)
    Q_PROPERTY(quint64 lastSavedRevision READ lastSavedRevision NOTIFY documentStateChanged)
    Q_PROPERTY(QString pendingDestructiveAction READ pendingDestructiveAction NOTIFY destructiveActionChanged)
    Q_PROPERTY(QString videoLoadState READ videoLoadState NOTIFY sourceLoadStateChanged)
    Q_PROPERTY(QString vboLoadState READ vboLoadState NOTIFY sourceLoadStateChanged)
    Q_PROPERTY(qint64 previewEndPositionMilliseconds READ previewEndPositionMilliseconds NOTIFY previewMetadataChanged)
    Q_PROPERTY(QString previewEndTimecode READ previewEndTimecode NOTIFY previewMetadataChanged)
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
    explicit AppController(QObject *parent = nullptr, QString recoveryPath = {},
                           ProjectRecoveryStore::Operations recoveryOperations = {});
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
    [[nodiscard]] QString lapTimingStatus() const;
    [[nodiscard]] QVariantList lapSummaries() const;
    [[nodiscard]] QVariantList lapNavigationSegments() const;
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
    [[nodiscard]] QString eventName() const;
    [[nodiscard]] QVariantList eventRuns() const;
    [[nodiscard]] QString activeRunId() const;
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
    Q_INVOKABLE bool selectEventRun(const QString &runId);
    Q_INVOKABLE QVariantMap runMetadata(const QString &runId) const;
    Q_INVOKABLE bool updateRunMetadata(const QString &runId, const QString &expectedToken,
        const QString &name, const QString &notes, const QString &conditions, const QString &setupChanges);
    Q_INVOKABLE QVariantMap runTrackConfiguration(const QString &runId) const;
    Q_INVOKABLE bool confirmRunTrackConfiguration(const QString &runId, const QString &expectedDerivationKey,
        const QString &layoutId, const QString &direction);
    Q_INVOKABLE bool selectOutingComparisonGroup(const QString &groupId);
    [[nodiscard]] QVariantMap outingRanking() const;
    [[nodiscard]] QVariantMap outingProgression() const;
    [[nodiscard]] QVariantList outingCompatibilityGroups() const;
    [[nodiscard]] QString outingComparisonGroupId() const;
    Q_INVOKABLE bool setRunTrackConfiguration(
        const QString &runId, const QString &layoutId, const QString &direction);
    [[nodiscard]] QString batchImportState() const { return m_batchState; }
    [[nodiscard]] QString batchImportError() const { return m_batchError; }
    [[nodiscard]] QStringList analysisImportMessages() const { return m_analysisImportMessages; }
    Q_INVOKABLE bool selectOutingLap(int index);
    // Snapshot resolution: opening detail revalidates source content off-thread.
    Q_INVOKABLE QVariantMap resolveOutingLapReference(const QVariantMap &reference) const;
    Q_INVOKABLE bool selectOutingLapReference(const QVariantMap &reference);
    Q_INVOKABLE bool setOutingLapExcluded(const QVariantMap &reference, bool excluded, const QString &reason = {});
    Q_INVOKABLE void closeOutingLap();
    Q_INVOKABLE QVariantMap outingLapSeries(const QString &channel, int maximumPoints) const;
    Q_INVOKABLE QString outingLapValueText(const QString &channel) const;
    [[nodiscard]] QVariantMap selectedOutingLap() const { return m_selectedOutingLap; }
    [[nodiscard]] QString outingLapDetailState() const { return m_outingLapDetailState; }
    [[nodiscard]] QString outingLapDetailError() const { return m_outingLapDetailError; }
    [[nodiscard]] QStringList outingLapAvailableChannels() const;
    void setOutingLapChannels(const QStringList &channels);
    [[nodiscard]] QStringList outingLapChannels() const { return m_outingLapChannels; }
    [[nodiscard]] QVariantList outingLapTrack() const { return m_outingLapTrack; }
    [[nodiscard]] QVariantMap outingLapTrackPoint() const;
    [[nodiscard]] double outingLapCursor() const { return m_outingLapCursor; }
    void setOutingLapCursor(double seconds);
    [[nodiscard]] QVariantList outingLaps() const { return m_outingLapRows; }
    [[nodiscard]] QStringList outingLapMessages() const { return m_outingLapMessages; }
    [[nodiscard]] bool outingLapsLoading() const { return m_outingLapsLoading; }
    [[nodiscard]] QVariantList batchImportRows() const { return m_batchRows; }
    [[nodiscard]] int batchImportProcessed() const { return m_batchProcessed; }
    [[nodiscard]] int batchImportTotal() const { return m_batchTotal; }
    Q_INVOKABLE bool beginBatchImport(const QList<QUrl> &urls);
    Q_INVOKABLE bool importAnalysisRuns(const QString &name, const QList<QUrl> &urls);
    Q_INVOKABLE void cancelBatchImport();
    Q_INVOKABLE bool confirmBatchImport(const QString &name, bool append, const QVariantList &choices);
    Q_INVOKABLE void relinkVideo(const QUrl &url);
    Q_INVOKABLE void relinkVbo(const QUrl &url);
    Q_INVOKABLE void resolveSourceMismatch(bool acceptReplacement);
    Q_INVOKABLE QString valueText(const QString &channelName, int decimals = 2) const;
    Q_INVOKABLE QVariant telemetryValue(const QString &channelName) const;
    Q_INVOKABLE QVariantMap telemetrySeries(
        const QString &channelName, double videoStart, double videoEnd, int maximumPoints) const;
    Q_INVOKABLE qint64 videoMillisecondsForTelemetryTime(double telemetryTime) const;
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
        const QString &rangeIn,
        const QString &rangeOut,
        bool overwriteAllowed = false);
    Q_INVOKABLE QVariantMap exportFormatOptions() const;
    Q_INVOKABLE QString exportFullRangeTimecode(
        qint64 frameRateNumerator, qint64 frameRateDenominator, bool outPoint) const;
    Q_INVOKABLE QVariantMap lapExportRange(
        int lapNumber, qint64 frameRateNumerator, qint64 frameRateDenominator,
        int handleSeconds) const;
    Q_INVOKABLE double exportRangeDurationSeconds(
        qint64 frameRateNumerator, qint64 frameRateDenominator,
        const QString &rangeIn, const QString &rangeOut) const;
    Q_INVOKABLE qint64 recommendedExportBitrate(int width, int height, qint64 numerator, qint64 denominator, const QString &quality) const;
    Q_INVOKABLE qint64 estimateExportSize(qint64 videoBitrate, bool audioEnabled, double seconds) const;
    Q_INVOKABLE QString formatEstimatedExportSize(qint64 bytes) const;
    Q_INVOKABLE QVariantMap previewViewport(int availableWidth, int availableHeight) const;
    Q_INVOKABLE qint64 previewEndPositionMilliseconds() const;
    Q_INVOKABLE qint64 previewInitialPositionMilliseconds() const;
    Q_INVOKABLE qint64 clampPreviewPositionMilliseconds(qint64 requestedMilliseconds) const;
    Q_INVOKABLE QString previewTimecodeForPositionMilliseconds(qint64 positionMilliseconds) const;
    Q_INVOKABLE QString previewEndTimecode() const;
    Q_INVOKABLE void reportPlaybackError(const QString &message);
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
    void batchImportChanged();
    void batchImportCommitted();
    void outingLapsChanged();
    void outingLapDetailChanged();
    void outingLapCursorChanged();
    void videoSourceChanged();
    void telemetryChanged();
    void lapNavigationChanged();
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
    void previewMetadataChanged();
    void projectLoadChanged();
    void recoveryChanged();
    void sourceMismatchChanged();
    void templateUiStateChanged();
    void saveAsRequested();
    void quitApproved();

private:
    friend class ::TelemetryTests; // Controlled asynchronous completion in regression tests.
    void invalidateSyncForTimingEdit();
    struct AutoSyncResult {
        bool success = false;
        bool cancelled = false;
        QString error;
        SyncCandidate candidate;
        qsizetype packetCount = 0;
        qsizetype gpsSampleCount = 0;
        QString gpsStream;
        quint64 generation = 0;
        quint64 syncRevision = 0;
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
        LapSession lapSession;
        QByteArray contentRevision;
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
        bool runSelection = false;
        quint64 recoveredRevision = 0;
        quint64 recoveredLastSavedRevision = 0;
        QString recoveredDocumentId;
    };

    [[nodiscard]] QVariant semanticValue(const QString &alias) const;
    void setStatus(QString status);
    struct SourceLoadRequest {
        QString path;
        bool markDocumentDirty = false;
        QJsonObject expectedFingerprint;
        bool relink = false;
    };
    [[nodiscard]] quint64 beginSourceReplacement(bool replacingVideo);
    [[nodiscard]] quint64 beginSourceGeneration();
    void cancelSourceJobs();
    void startVideoProbe(const QString &path, quint64 generation, bool markDocumentDirty,
                         QJsonObject expectedFingerprint = {}, bool relink = false);
    void startVboLoad(const QString &path, quint64 generation, bool markDocumentDirty,
                      QJsonObject expectedFingerprint = {}, bool relink = false);
    void startProjectSources(const ProjectLoadResult &result);
    [[nodiscard]] bool commitProjectLoad(const ProjectLoadResult &result);
    void commitVideoProbe(const VideoProbeResult &result, bool markDocumentDirty);
    void commitVboLoad(const VboLoadResult &result, bool markDocumentDirty);
    void setProjectLoadState(bool loading, QString stage = {}, QString error = {});
    [[nodiscard]] static QString normalizedSourcePath(const QString &path);
    [[nodiscard]] static QVariantList trackPointsFor(const TrackGeometry &geometry);
    void markPersistentChange();
    [[nodiscard]] QJsonObject currentProjectObject(const QString &projectPath = {},
                                                    std::optional<quint64> savedRevision = std::nullopt) const;
    bool beginProjectLoad(QString projectPath, const QJsonObject &project,
                          bool recovered = false, quint64 recoveredRevision = 0,
                          quint64 recoveredLastSavedRevision = 0,
                          QString recoveredDocumentId = {}, bool runSelection = false);
    void restoreStartupState();
    void scheduleRecoveryWrite();
    void writeRecoverySnapshot();
    bool clearRecovery(const QString &reason);
    bool discardRecovery(const ProjectRecoverySnapshot &snapshot, const QString &reason);
    void clearDiscardTombstoneAfterRecoveryCleanup();
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
    struct OutingLapDetailResult {
        quint64 request = 0;
        bool staleReference = false;
        std::shared_ptr<const TelemetrySession> session;
        TrackGeometry geometry;
        QVariantList track;
        QString error;
    };
    void initializeOutingLapDetail();
    void loadOutingLapDetail();
    static QVariantMap sessionSeries(const TelemetrySession &session, const QString &channel,
        double start, double end, int maximumPoints);
    QFutureWatcher<OutingLapDetailResult> m_outingLapDetailWatcher;
    QTimer m_outingLapDetailTimer;
    std::shared_ptr<std::atomic_bool> m_outingLapDetailCancellation;
    std::shared_ptr<const TelemetrySession> m_outingLapDetailSession;
    TrackGeometry m_outingLapDetailGeometry;
    QVariantMap m_selectedOutingLap;
    QJsonObject m_outingLapDetailSource;
    QByteArray m_outingLapDetailKey;
    quint64 m_outingLapDetailGeneration = 0;
    quint64 m_outingLapDetailRequest = 0;
    bool m_outingLapDetailPending = false;
    QString m_outingLapDetailState = QStringLiteral("idle");
    QString m_outingLapDetailError;
    QStringList m_outingLapChannels;
    QVariantList m_outingLapTrack;
    double m_outingLapCursor = 0;
    struct OutingSourceMessage {
        QString runId;
        QString text;
    };
    struct OutingLapResult {
        QVector<OutingLapRow> rows;
        QList<OutingSourceMessage> messages;
        QByteArray key;
        quint64 generation = 0;
        bool cancelled = false;
    };
    void initializeOutingLaps();
    void refreshOutingLaps();
    void refreshLapExclusionPolicy();
    [[nodiscard]] QJsonObject activeLapBinding() const;
    void refreshOutingCompatibility();
    QVariantMap m_outingRanking;
    QVariantMap m_outingProgression;
    QVariantList m_outingCompatibilityGroups;
    QString m_outingComparisonGroupId;
    QString m_outingCompatibilityDocumentId;
    QVector<OutingLapRow> m_outingRawLapRows;
    QList<OutingSourceMessage> m_outingSourceMessages;
    QByteArray m_loadedSourceRevision;
    [[nodiscard]] QJsonArray outingLapSources() const;
    [[nodiscard]] QByteArray outingLapKey() const;
    QFutureWatcher<OutingLapResult> m_outingLapWatcher;
    QTimer m_outingLapTimer;
    std::shared_ptr<std::atomic_bool> m_outingLapCancellation;
    QByteArray m_outingLapRequestedKey;
    quint64 m_outingLapGeneration = 0;
    QVariantList m_outingLapRows;
    QSet<QString> m_outingStaleRunIds;
    QStringList m_outingLapMessages;
    bool m_outingLapsLoading = false;
    struct BatchImportResult {
        std::shared_ptr<TelemetryImportPlan> plan;
        QHash<QString, QJsonObject> fingerprints;
        QSet<QString> existing;
        QJsonObject project;
        QString error;
        bool cancelled = false;
        bool confirmation = false;
        bool append = false;
    };
    void initializeBatchImport();
    void invalidateBatchImport();
    [[nodiscard]] bool batchContextMatches() const;
    void publishBatchRows();
    QFutureWatcher<BatchImportResult> m_batchWatcher;
    QTimer m_batchProgressTimer;
    std::shared_ptr<std::atomic_bool> m_batchCancellation;
    std::shared_ptr<std::atomic_int> m_batchProgress;
    std::shared_ptr<TelemetryImportPlan> m_batchPlan;
    QHash<QString, QJsonObject> m_batchFingerprints;
    QSet<QString> m_batchExisting;
    QVariantList m_batchRows;
    QString m_batchState = QStringLiteral("idle");
    QString m_batchError;
    QString m_batchDocumentId;
    QString m_batchProjectPath;
    quint64 m_batchRevision = 0;
    quint64 m_batchGeneration = 0;
    int m_batchProcessed = 0;
    int m_batchTotal = 0;
    bool m_batchApplying = false;
    bool m_batchPending = false;
    bool m_analysisImportAutomatic = false;
    bool m_analysisImportAppend = false;
    QString m_analysisImportName;
    QStringList m_analysisImportMessages;
    QUrl m_videoSource;
    QString m_telemetryPath;
    ProjectSourceReference m_videoReference;
    ProjectSourceReference m_vboReference;
    QString m_statusText = QStringLiteral("Open a video and VBO to begin.");
    std::unique_ptr<TelemetrySession> m_session;
    LapSession m_lapSession;
    WidgetModel m_widgetModel;
    TrackGeometry m_trackGeometry;
    TelemetryRenderContext m_previewRenderContext;
    QVariantList m_trackPoints;
    QJsonObject m_projectTemplate;
    ProjectWriter m_projectWriter;
    ProjectDocumentState m_documentState;
    ProjectRecoveryStore m_recoveryStore;
    QString m_documentId;
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
    quint64 m_syncRevision = 0;
    std::shared_ptr<std::atomic_bool> m_videoProbeCancellation;
    std::shared_ptr<std::atomic_bool> m_vboLoadCancellation;
    std::shared_ptr<std::atomic_bool> m_projectLoadCancellation;
    std::shared_ptr<std::atomic_bool> m_syncCancellation;
    bool m_videoLoadMarksDocumentDirty = true;
    bool m_vboLoadMarksDocumentDirty = true;
    QString m_videoLoadState = QStringLiteral("idle");
    QString m_vboLoadState = QStringLiteral("idle");
    SourceLoadRequest m_videoLoadRequest;
    SourceLoadRequest m_vboLoadRequest;
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
