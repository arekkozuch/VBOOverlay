#include "gopro/GoProTelemetrySource.h"
#include "app/AppController.h"
#include "export/EncoderDetector.h"
#include "export/ExportEngine.h"
#include "export/ExportFormat.h"
#include "export/ExportDiagnostics.h"
#include "export/PersistentExportLog.h"
#include "export/RawFrameTransport.h"
#include "export/ExportProgress.h"
#include "export/ExportOutputTransaction.h"
#include "export/ExportTargetIdentity.h"
#include "export/ExportArtifactManifest.h"
#include "export/ExportProcessSupervisor.h"
#include "export/ExportStoragePolicy.h"
#include "export/FfmpegTools.h"
#include "export/MediaProbe.h"
#include "export/TemporaryOverlayValidation.h"
#include "sync/TelemetrySyncEngine.h"
#include "telemetry/TelemetrySession.h"
#include "telemetry/TelemetryRenderContext.h"
#include "telemetry/TrackGeometry.h"
#include "telemetry/VboParser.h"
#include "widgets/WidgetModel.h"
#include "project/ProjectWriter.h"
#include "project/ProjectDocumentState.h"
#include "project/ProjectRecoveryStore.h"

#include <QFile>
#include <QDateTime>
#include <QElapsedTimer>
#include <QJsonDocument>
#include <QProcess>
#include <QSettings>
#include <QScopeGuard>
#include <QTemporaryDir>
#include <QThread>
#include <QUuid>
#include <QtEndian>
#include <QtTest>
#include <cmath>
#include <atomic>
#include <limits>
#include <numbers>
#include <thread>

using namespace FlappedEar;

class TelemetryTests final : public QObject {
    Q_OBJECT

private slots:
    void parsesRealisticFixture();
    void toleratesMalformedRows();
    void preservesRepeatedDataSections();
    void rejectsMissingSections();
    void interpolatesByTime();
    void preservesMissingTelemetryGaps();
    void filtersOverlayPresentationValues();
    void samplesTelemetryRanges();
    void parsesTextFirstVboTimeFormats();
    void keepsVboTimestampsStrictlyMonotonic();
    void cancelsVboParsingDeterministically();
    void enforcesVboResourceLimits();
    void convertsArcMinuteCoordinates();
    void parsesOptionalRealVbo();
    void persistsWidgetScenes();
    void loadsVisualTemplates();
    void providesCustomizableArchetypes();
    void persistsAndSharesCustomTemplates();
    void persistsWidgetAnimationCues();
    void groupsAndMovesWidgets();
    void constrainsWidgetGeometry();
    void buildsTrackGeometry();
    void cachesStaticTrackGeometry();
    void keepsStaticTrackIndependentFromTime();
    void decodesGps9Gpmf();
    void rejectsMalformedGpmf();
    void cancelsSlowGoProProbePromptly();
    void boundsGoProProbeOutput();
    void rejectsOutOfFileGpmfPackets();
    void boundsGpmfDepthAndRecordCount();
    void normalizesGpmfTimestamps();
    void synchronizesGpsSpeed();
    void cancelsSynchronizationDeterministically();
    void reportsAmbiguousGpsSpeed();
    void gatesWeakSyncCandidates();
    void rendersTelemetryAtExplicitTime();
    void probesMediaInfoJson();
    void parsesMediaSummaryJson();
    void rejectsInvalidMediaProbeJson();
    void classifiesMediaProbeProcessFailures();
    void reportsMediaProbeLifecycleHeartbeat();
    void cancelsMediaProbeWithoutLeavingItRunning();
    void resolvesExportFilesystemsForFutureArtifacts();
    void evaluatesIndependentExportStorageVolumes();
    void estimatesTemporaryStorageFromRepresentativeSample();
    void streamsRawFramesToSlowConsumer();
    void failsRawFrameTransportWhenConsumerExits();
    void timesOutStalledRawFrameTransport();
    void cancelsBlockedRawFrameTransportPromptly();
    void cleansOnlyManifestOwnedArtifacts();
    void preservesLiveManifestForStartupRecovery();
    void supervisesUnixExportProcessTree();
    void detectsHevcEncoders();
    void cancelsEncoderDiscovery();
    void calculatesTimestampDrivenExportFrames();
    void resolvesExplicitExportFormats();
    void preservesExactExportRateRationals();
    void preservesCfrCadenceForCommonRates();
    void validatesQuantizedTemporaryOverlayCadence();
    void preservesAbsoluteExportTimestamps();
    void composesNonZeroExportRangeWithZeroBasedOutput();
    void normalizesNonZeroStreamPtsForVideoAndAudio();
    void convertsVfrInputToCfrWithFrameCorrectOverlay();
    void estimatesExportProgress();
    void tracksExportStageElapsedTime();
    void boundsVerboseDiagnosticStorage();
    void persistsExportDiagnosticsAndRetainsKnownLogs();
    void formatsStageAFailureDiagnostics();
    void throttlesDiagnosticHeartbeats();
    void tracksValidationSubstepStages();
    void parsesStructuredFfmpegProgress();
    void calculatesEncodedOutputProgress();
    void preservesFrameIdentityThroughCompletedOverlayComposition();
    void rejectsUnsafeExportPaths();
    void capturesExportTargetIdentity();
    void rejectsChangedExportTargets();
    void rejectsSymlinkExportTargets();
    void preservesExistingExportTargetOnFailures_data();
    void preservesExistingExportTargetOnFailures();
    void commitsNewAndReplacementExports();
    void savesProjectsAtomically();
    void writesRecoverySnapshotsAtomically();
    void detectsPartialAndCommitWriteFailures();
    void gatesDirtyDestructiveActions_data();
    void gatesDirtyDestructiveActions();
    void resolvesDirtyDecisionsSafely();
    void retainsTelemetryAfterFailedAsyncLoad();
    void replacesInFlightSourceLoad();
    void shutsDownWithInFlightSourceLoad();
    void opensProjectsTransactionally();
    void restoresSavedProjectsAndPreservesUnknownFields();
    void recoversAndDiscardsSavedChanges();
    void recoversAndDiscardsUnsavedDocuments();
    void discardsUnsavedStateForQuitNewAndOpen();
    void preservesRecoveryAcrossFailedSave();
    void rejectsProjectLoadAfterInterveningEdit();
    void syncsOptionalRealRecording();
};

namespace {

QByteArray klvRecord(
    const QByteArray &key,
    const char type,
    const quint8 size,
    const quint16 repeat,
    QByteArray data)
{
    QByteArray result = key.leftJustified(4, ' ').left(4);
    result.append(type);
    result.append(static_cast<char>(size));
    const quint16 bigRepeat = qToBigEndian(repeat);
    result.append(reinterpret_cast<const char *>(&bigRepeat), sizeof(bigRepeat));
    result.append(data);
    while (result.size() % 4 != 0) {
        result.append('\0');
    }
    return result;
}

void append32(QByteArray &data, const qint32 value)
{
    const qint32 big = qToBigEndian(value);
    data.append(reinterpret_cast<const char *>(&big), sizeof(big));
}

void append16(QByteArray &data, const quint16 value)
{
    const quint16 big = qToBigEndian(value);
    data.append(reinterpret_cast<const char *>(&big), sizeof(big));
}

TelemetrySession speedSession(const double start, const double end, const double valueOffset)
{
    TelemetrySession session;
    TelemetryChannel speed;
    speed.name = "speed";
    speed.unit = "km/h";
    for (double time = start; time <= end; time += 0.2) {
        speed.timestamps.append(time);
        const double sourceTime = time - valueOffset;
        speed.values.append(static_cast<float>(
            50.0 + 18.0 * std::sin(sourceTime * 0.21)
            + 7.0 * std::sin(sourceTime * 0.73) + sourceTime * 0.08));
    }
    session.channels.insert("speed", speed);
    session.aliases.insert("speed", "speed");
    session.duration = end - start;
    session.sampleCount = speed.values.size();
    return session;
}

bool writeBytes(const QString &path, const QByteArray &bytes)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly | QIODevice::Truncate)
        && file.write(bytes) == bytes.size();
}

QByteArray readBytes(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return {};
    return file.readAll();
}

QJsonObject testProject(const double offset, const QJsonObject &extra = {})
{
    WidgetModel widgets;
    widgets.resetDefaults();
    QJsonObject project = extra;
    project.insert(QStringLiteral("version"), 2);
    project.insert(QStringLiteral("scene"), QJsonObject{{QStringLiteral("widgets"), widgets.toJson()}});
    project.insert(QStringLiteral("sync"), QJsonObject{{QStringLiteral("offset"), offset},
                                                        {QStringLiteral("timeScale"), 1.0}});
    project.insert(QStringLiteral("analysis"), QJsonObject{{QStringLiteral("channels"), QJsonArray{}},
                                                            {QStringLiteral("visible"), true}});
    return project;
}

class InjectedProjectWriteDevice final : public ProjectWriteDevice {
public:
    InjectedProjectWriteDevice(
        const bool opens, const qint64 bytesWritten, const bool commits, QString error)
        : m_opens(opens)
        , m_bytesWritten(bytesWritten)
        , m_commits(commits)
        , m_error(std::move(error))
    {
    }

    bool open() override { return m_opens; }
    qint64 write(const QByteArray &) override { return m_bytesWritten; }
    bool commit() override { return m_commits; }
    QString errorString() const override { return m_error; }

private:
    bool m_opens;
    qint64 m_bytesWritten;
    bool m_commits;
    QString m_error;
};

} // namespace

void TelemetryTests::rejectsUnsafeExportPaths()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString input = directory.filePath("input.mp4");
    const QString vbo = directory.filePath("telemetry.vbo");
    QVERIFY(writeBytes(input, "input"));
    QVERIFY(writeBytes(vbo, "vbo"));

    ExportOutputTransaction sameInput;
    QCOMPARE(sameInput.prepare(input, input, {vbo}, false).status,
             ExportOutputTransaction::PreparationStatus::Error);

    ExportOutputTransaction sameVbo;
    QCOMPARE(sameVbo.prepare(vbo, input, {vbo}, false).status,
             ExportOutputTransaction::PreparationStatus::Error);

    ExportOutputTransaction missingDestination;
    QCOMPARE(missingDestination.prepare(directory.filePath("missing/out.mp4"), input, {vbo}, false).status,
             ExportOutputTransaction::PreparationStatus::Error);

#ifndef Q_OS_WIN
    const QString unwritablePath = directory.filePath("unwritable");
    QVERIFY(QDir().mkdir(unwritablePath));
    const QFile::Permissions originalPermissions = QFileInfo(unwritablePath).permissions();
    QVERIFY(QFile::setPermissions(
        unwritablePath, QFileDevice::ReadOwner | QFileDevice::ExeOwner));
    const auto restorePermissions = qScopeGuard([&] {
        QFile::setPermissions(unwritablePath, originalPermissions);
    });
    ExportOutputTransaction unwritableDestination;
    QCOMPARE(unwritableDestination.prepare(
                 QDir(unwritablePath).filePath("out.mp4"), input, {vbo}, false).status,
             ExportOutputTransaction::PreparationStatus::Error);
#endif
}

void TelemetryTests::capturesExportTargetIdentity()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString target = directory.filePath(QStringLiteral("target.mp4"));
    QVERIFY(writeBytes(target, "original"));

    ExportTargetIdentity original;
    QString error;
    QCOMPARE(ExportTargetIdentity::capture(target, &original, &error),
             ExportTargetIdentity::CaptureStatus::Captured);
    QVERIFY2(original.isValid(), qPrintable(error));
    ExportTargetIdentity unchanged;
    QCOMPARE(ExportTargetIdentity::capture(target, &unchanged, &error),
             ExportTargetIdentity::CaptureStatus::Captured);
    QVERIFY(original.matches(unchanged));

    QVERIFY(writeBytes(target, "changed!"));
    QFile modifiedFile(target);
    QVERIFY(modifiedFile.open(QIODevice::ReadWrite));
    QVERIFY(modifiedFile.setFileTime(
        QDateTime::fromMSecsSinceEpoch(946684800000LL), QFileDevice::FileModificationTime));
    modifiedFile.close();
    ExportTargetIdentity modified;
    QCOMPARE(ExportTargetIdentity::capture(target, &modified, &error),
             ExportTargetIdentity::CaptureStatus::Captured);
    QVERIFY(!original.matches(modified));

    const QString displaced = directory.filePath(QStringLiteral("displaced.mp4"));
    QVERIFY(QFile::rename(target, displaced));
    QVERIFY(writeBytes(target, "other!!!"));
    ExportTargetIdentity replaced;
    QCOMPARE(ExportTargetIdentity::capture(target, &replaced, &error),
             ExportTargetIdentity::CaptureStatus::Captured);
    QVERIFY(!original.matches(replaced));

    QVERIFY(QFile::remove(target));
    ExportTargetIdentity missing;
    QCOMPARE(ExportTargetIdentity::capture(target, &missing, &error),
             ExportTargetIdentity::CaptureStatus::Missing);
}

void TelemetryTests::rejectsChangedExportTargets()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString input = directory.filePath(QStringLiteral("input.mp4"));
    QVERIFY(writeBytes(input, "input"));

    const auto prepareStaged = [&](const QString &target, ExportOutputTransaction &transaction) {
        QCOMPARE(transaction.prepare(target, input, {}, true).status,
                 ExportOutputTransaction::PreparationStatus::Ready);
        QVERIFY(writeBytes(transaction.stagingPath(), "validated staging output"));
    };

    const QString modifiedTarget = directory.filePath(QStringLiteral("modified.mp4"));
    QVERIFY(writeBytes(modifiedTarget, "approved target"));
    {
        ExportOutputTransaction transaction;
        prepareStaged(modifiedTarget, transaction);
        const QByteArray externalBytes("externally modified target with a new size");
        QVERIFY(writeBytes(modifiedTarget, externalBytes));
        QString error;
        QVERIFY(!transaction.commit(&error));
        QVERIFY2(error.contains(QStringLiteral("changed")), qPrintable(error));
        QCOMPARE(readBytes(modifiedTarget), externalBytes);
        QCOMPARE(readBytes(transaction.stagingPath()), QByteArray("validated staging output"));
    }

    const QString replacedTarget = directory.filePath(QStringLiteral("replaced.mp4"));
    const QString displacedTarget = directory.filePath(QStringLiteral("approved-original.mp4"));
    QVERIFY(writeBytes(replacedTarget, "approved target A"));
    {
        ExportOutputTransaction transaction;
        prepareStaged(replacedTarget, transaction);
        QVERIFY(QFile::rename(replacedTarget, displacedTarget));
        const QByteArray replacementBytes("external replacement B");
        QVERIFY(writeBytes(replacedTarget, replacementBytes));
        QString error;
        QVERIFY(!transaction.commit(&error));
        QVERIFY2(error.contains(QStringLiteral("changed")), qPrintable(error));
        QCOMPARE(readBytes(replacedTarget), replacementBytes);
        QCOMPARE(readBytes(displacedTarget), QByteArray("approved target A"));
    }

    const QString disappearedTarget = directory.filePath(QStringLiteral("disappeared.mp4"));
    QVERIFY(writeBytes(disappearedTarget, "approved target"));
    {
        ExportOutputTransaction transaction;
        prepareStaged(disappearedTarget, transaction);
        QVERIFY(QFile::remove(disappearedTarget));
        QString error;
        QVERIFY(!transaction.commit(&error));
        QVERIFY2(error.contains(QStringLiteral("disappeared")), qPrintable(error));
        QVERIFY(!QFileInfo::exists(disappearedTarget));
        QCOMPARE(readBytes(transaction.stagingPath()), QByteArray("validated staging output"));
    }

    const QString appearedTarget = directory.filePath(QStringLiteral("appeared.mp4"));
    {
        ExportOutputTransaction transaction;
        QCOMPARE(transaction.prepare(appearedTarget, input, {}, false).status,
                 ExportOutputTransaction::PreparationStatus::Ready);
        QVERIFY(writeBytes(transaction.stagingPath(), "validated staging output"));
        const QByteArray externalBytes("external newly appeared target");
        QVERIFY(writeBytes(appearedTarget, externalBytes));
        QString error;
        QVERIFY(!transaction.commit(&error));
        QVERIFY2(error.contains(QStringLiteral("appeared")), qPrintable(error));
        QCOMPARE(readBytes(appearedTarget), externalBytes);
    }
}

void TelemetryTests::rejectsSymlinkExportTargets()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString input = directory.filePath(QStringLiteral("input.mp4"));
    const QString referent = directory.filePath(QStringLiteral("referent.mp4"));
    const QString link = directory.filePath(QStringLiteral("linked-output.mp4"));
    QVERIFY(writeBytes(input, "input"));
    QVERIFY(writeBytes(referent, "referent bytes"));
    if (!QFile::link(referent, link)) {
        QSKIP("The test environment does not permit symbolic-link creation.");
    }
    ExportTargetIdentity linkedIdentity;
    if (ExportTargetIdentity::capture(link, &linkedIdentity)
        != ExportTargetIdentity::CaptureStatus::Link) {
        QSKIP("The platform link API did not create a native symbolic link/reparse point.");
    }
    ExportOutputTransaction transaction;
    const ExportOutputTransaction::PreparationResult prepared = transaction.prepare(
        link, input, {}, true);
    QCOMPARE(prepared.status, ExportOutputTransaction::PreparationStatus::Error);
    QVERIFY2(prepared.error.contains(QStringLiteral("symbolic link"), Qt::CaseInsensitive)
                 || prepared.error.contains(QStringLiteral("reparse point"), Qt::CaseInsensitive),
             qPrintable(prepared.error));
    QCOMPARE(readBytes(referent), QByteArray("referent bytes"));
}

void TelemetryTests::preservesExistingExportTargetOnFailures_data()
{
    QTest::addColumn<QString>("scenario");
    QTest::addColumn<bool>("overwriteAllowed");
    for (const QString &scenario : {
             QStringLiteral("cancel during Stage A"),
             QStringLiteral("Stage A FFmpeg failure"),
             QStringLiteral("temporary overlay validation failure"),
             QStringLiteral("Stage B failure"),
             QStringLiteral("final validation failure"),
             QStringLiteral("worker termination"),
             QStringLiteral("application shutdown cleanup")}) {
        QTest::newRow(qPrintable(scenario)) << scenario << true;
    }
    QTest::newRow("overwrite denied") << QStringLiteral("overwrite denied") << false;
}

void TelemetryTests::preservesExistingExportTargetOnFailures()
{
    QFETCH(QString, scenario);
    QFETCH(bool, overwriteAllowed);
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QByteArray original("known existing target bytes\0unchanged", 37);
    const QString input = directory.filePath("input.mp4");
    const QString target = directory.filePath("target.mp4");
    QVERIFY(writeBytes(input, "input"));
    QVERIFY(writeBytes(target, original));

    {
        ExportOutputTransaction transaction;
        const auto prepared = transaction.prepare(target, input, {}, overwriteAllowed);
        if (!overwriteAllowed) {
            QCOMPARE(prepared.status,
                     ExportOutputTransaction::PreparationStatus::OverwriteConfirmationRequired);
            QVERIFY(transaction.stagingPath().isEmpty());
        } else {
            QCOMPARE(prepared.status, ExportOutputTransaction::PreparationStatus::Ready);
            QVERIFY(transaction.ownsPath(transaction.stagingPath()));
            QVERIFY(writeBytes(transaction.stagingPath(), "partial staged output"));
            if (scenario != QStringLiteral("application shutdown cleanup")) {
                transaction.cleanup();
            }
        }
    }
    QFile targetFile(target);
    QVERIFY(targetFile.open(QIODevice::ReadOnly));
    QCOMPARE(targetFile.readAll(), original);
    const QStringList artifacts = QDir(directory.path()).entryList(
        {QStringLiteral("*.part.*"), QStringLiteral("*.backup"), QStringLiteral("*.cancel")},
        QDir::Files | QDir::Hidden);
    QVERIFY2(artifacts.isEmpty(), qPrintable(artifacts.join(',')));
}

void TelemetryTests::commitsNewAndReplacementExports()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString input = directory.filePath("input.mp4");
    QVERIFY(writeBytes(input, "input"));

    const QString newTarget = directory.filePath("new.mp4");
    ExportOutputTransaction newFile;
    QCOMPARE(newFile.prepare(newTarget, input, {}, false).status,
             ExportOutputTransaction::PreparationStatus::Ready);
    QCOMPARE(QFileInfo(newFile.stagingPath()).absolutePath(), QFileInfo(newTarget).absolutePath());
    QVERIFY(writeBytes(newFile.stagingPath(), "new valid output"));
    QString error;
    QVERIFY2(newFile.commit(&error), qPrintable(error));
    QFile newTargetFile(newTarget);
    QVERIFY(newTargetFile.open(QIODevice::ReadOnly));
    QCOMPARE(newTargetFile.readAll(), QByteArray("new valid output"));

    const QString existingTarget = directory.filePath("existing.mp4");
    QVERIFY(writeBytes(existingTarget, "old known target"));
    ExportOutputTransaction replacement;
    QCOMPARE(replacement.prepare(existingTarget, input, {}, true).status,
             ExportOutputTransaction::PreparationStatus::Ready);
    QVERIFY(replacement.targetExistedBeforeExport());
    QVERIFY(writeBytes(replacement.stagingPath(), "replacement output"));
    QVERIFY2(replacement.commit(&error), qPrintable(error));
    QFile replacementFile(existingTarget);
    QVERIFY(replacementFile.open(QIODevice::ReadOnly));
    QCOMPARE(replacementFile.readAll(), QByteArray("replacement output"));
    const QStringList artifacts = QDir(directory.path()).entryList(
        {QStringLiteral("*.part.*"), QStringLiteral("*.backup"), QStringLiteral("*.cancel")},
        QDir::Files | QDir::Hidden);
    QVERIFY2(artifacts.isEmpty(), qPrintable(artifacts.join(',')));
}

void TelemetryTests::savesProjectsAtomically()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath("project.fetproject");
    const QByteArray original = R"({"version":2,"scene":{"widgets":[]}})";
    const QByteArray replacement = R"({"version":2,"scene":{"widgets":[{"type":"speed"}]}})";
    QVERIFY(writeBytes(path, original));

    const ProjectWriter writer;
    const ProjectWriter::Result success = writer.write(path, replacement);
    QVERIFY2(success.success, qPrintable(success.error));
    QFile saved(path);
    QVERIFY(saved.open(QIODevice::ReadOnly));
    QCOMPARE(saved.readAll(), replacement);
    saved.close();

#ifndef Q_OS_WIN
    const QFile::Permissions originalPermissions = QFileInfo(directory.path()).permissions();
    QVERIFY(QFile::setPermissions(
        directory.path(), QFileDevice::ReadOwner | QFileDevice::ExeOwner));
    const auto restorePermissions = qScopeGuard([&] {
        QFile::setPermissions(directory.path(), originalPermissions);
    });
    const ProjectWriter::Result failure = writer.write(path, QByteArray("corrupting replacement"));
    QVERIFY(!failure.success);
    QFile unchanged(path);
    QVERIFY(unchanged.open(QIODevice::ReadOnly));
    QCOMPARE(unchanged.readAll(), replacement);
#endif
}

void TelemetryTests::writesRecoverySnapshotsAtomically()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("recovery.json"));
    const ProjectRecoveryStore store(path);
    const ProjectRecoverySnapshot first{
        QStringLiteral("saved.fetproject"), 2, 1,
        QStringLiteral("2026-08-22T12:00:00.000Z"), testProject(1.0)};
    QString error;
    QVERIFY2(store.write(first, &error), qPrintable(error));
    ProjectRecoverySnapshot loaded;
    QVERIFY2(store.load(&loaded, &error), qPrintable(error));
    QCOMPARE(loaded.originalProjectPath, first.originalProjectPath);
    QCOMPARE(loaded.revision, first.revision);
    QCOMPARE(loaded.lastSavedRevision, first.lastSavedRevision);
    QCOMPARE(loaded.project, first.project);

#ifndef Q_OS_WIN
    const QFile::Permissions originalPermissions = QFileInfo(directory.path()).permissions();
    QVERIFY(QFile::setPermissions(
        directory.path(), QFileDevice::ReadOwner | QFileDevice::ExeOwner));
    const auto restorePermissions = qScopeGuard([&] {
        QFile::setPermissions(directory.path(), originalPermissions);
    });
    ProjectRecoverySnapshot replacement = first;
    replacement.revision = 3;
    replacement.project = testProject(9.0);
    QVERIFY(!store.write(replacement, &error));
    QVERIFY2(store.load(&loaded, &error), qPrintable(error));
    QCOMPARE(loaded.revision, first.revision);
    QCOMPARE(loaded.project, first.project);
#endif
}

void TelemetryTests::detectsPartialAndCommitWriteFailures()
{
    const QByteArray payload("complete serialized project");
    const ProjectWriter partialWriter([&](const QString &) {
        return std::make_unique<InjectedProjectWriteDevice>(
            true, payload.size() - 1, true, QStringLiteral("injected partial write"));
    });
    const ProjectWriter::Result partial = partialWriter.write("project.fetproject", payload);
    QVERIFY(!partial.success);
    QVERIFY(partial.error.contains(QStringLiteral("incomplete")));

    const ProjectWriter commitWriter([&](const QString &) {
        return std::make_unique<InjectedProjectWriteDevice>(
            true, payload.size(), false, QStringLiteral("injected commit failure"));
    });
    const ProjectWriter::Result commit = commitWriter.write("project.fetproject", payload);
    QVERIFY(!commit.success);
    QVERIFY(commit.error.contains(QStringLiteral("commit")));
}

void TelemetryTests::gatesDirtyDestructiveActions_data()
{
    QTest::addColumn<int>("actionValue");
    QTest::newRow("New dirty")
        << static_cast<int>(ProjectDocumentState::DestructiveAction::NewProject);
    QTest::newRow("Open dirty")
        << static_cast<int>(ProjectDocumentState::DestructiveAction::OpenProject);
    QTest::newRow("Quit dirty")
        << static_cast<int>(ProjectDocumentState::DestructiveAction::Quit);
}

void TelemetryTests::gatesDirtyDestructiveActions()
{
    QFETCH(int, actionValue);
    const auto action = static_cast<ProjectDocumentState::DestructiveAction>(actionValue);
    ProjectDocumentState document;
    document.reset("current.fetproject");
    QCOMPARE(document.request(action), ProjectDocumentState::RequestResult::ContinueImmediately);
    QCOMPARE(document.takePendingAction(), action);

    document.markChanged();
    QVERIFY(document.dirty());
    QCOMPARE(document.request(action), ProjectDocumentState::RequestResult::DecisionRequired);
    QCOMPARE(document.pendingAction(), action);
}

void TelemetryTests::resolvesDirtyDecisionsSafely()
{
    ProjectDocumentState document;
    document.reset("current.fetproject");
    document.markChanged();
    const quint64 changedRevision = document.revision();
    QCOMPARE(document.request(ProjectDocumentState::DestructiveAction::OpenProject),
             ProjectDocumentState::RequestResult::DecisionRequired);

    // A failed save leaves both dirty state and the pending destructive action intact.
    QVERIFY(document.dirty());
    QCOMPARE(document.lastSavedRevision(), quint64(0));
    QCOMPARE(document.pendingAction(), ProjectDocumentState::DestructiveAction::OpenProject);

    // A successful save clears dirty, after which the requested action can continue.
    document.markSaved("current.fetproject");
    QVERIFY(!document.dirty());
    QCOMPARE(document.lastSavedRevision(), changedRevision);
    QCOMPARE(document.takePendingAction(), ProjectDocumentState::DestructiveAction::OpenProject);

    // Don't Save continues; Cancel retains the current dirty document.
    document.markChanged();
    QCOMPARE(document.request(ProjectDocumentState::DestructiveAction::NewProject),
             ProjectDocumentState::RequestResult::DecisionRequired);
    QCOMPARE(document.takePendingAction(), ProjectDocumentState::DestructiveAction::NewProject);
    QVERIFY(document.dirty());
    QCOMPARE(document.request(ProjectDocumentState::DestructiveAction::Quit),
             ProjectDocumentState::RequestResult::DecisionRequired);
    document.cancelPendingAction();
    QCOMPARE(document.pendingAction(), ProjectDocumentState::DestructiveAction::None);
    QVERIFY(document.dirty());
    QCOMPARE(document.projectPath(), QString("current.fetproject"));
}

void TelemetryTests::parsesRealisticFixture()
{
    const TelemetrySession session = VboParser::parseFile(TEST_FIXTURE_PATH);
    QCOMPARE(session.sampleCount, 3);
    QCOMPARE(session.duration, 1.0);
    QCOMPARE(session.metadata.value("vehicle"), QString("Test Car"));
    QVERIFY(session.channels.contains("mystery"));
    QCOMPARE(session.aliases.value("speed"), QString("velocity"));
    QCOMPARE(session.aliases.value("rpm"), QString("rpm"));
    QCOMPARE(session.aliases.value("throttle"), QString("throttle"));
    QCOMPARE(session.aliases.value("brake"), QString("brake"));
    QCOMPARE(session.aliases.value("heartRate"), QString("heart_rate"));
    QCOMPARE(session.channels.value("velocity").values, QVector<float>({0.0F, 50.0F, 100.0F}));
}

void TelemetryTests::toleratesMalformedRows()
{
    const auto session = VboParser::parse(
        u"[column names]\ntime speed unknown\n[data]\n0   10  1\n1 bad\n2 30 3 extra");
    QCOMPARE(session.sampleCount, 3);
    QVERIFY(std::isnan(session.channels.value("speed").values[1]));
    QVERIFY(!session.valueAt("speed", 0.5));
    QVERIFY(!session.valueAt("speed", 1.0));
    QVERIFY(!session.valueAt("speed", 1.5));
    QCOMPARE(session.warnings.size(), 2);
}

void TelemetryTests::preservesRepeatedDataSections()
{
    const TelemetrySession session = VboParser::parse(
        u"[column names]\ntime speed\n[data]\n0 10\n[data]\n1 20");
    QCOMPARE(session.sampleCount, 2);
    QCOMPARE(session.channels.value("speed").values, QVector<float>({10.0F, 20.0F}));
}

void TelemetryTests::rejectsMissingSections()
{
    QVERIFY_THROWS_EXCEPTION(VboParseError, (void) VboParser::parse(u"[header]\nfoo=bar"));
}

void TelemetryTests::interpolatesByTime()
{
    const auto session = VboParser::parse(u"[column names]\ntime speed\n[data]\n0 0\n1 10\n2 30");
    QCOMPARE(session.valueAt("speed", 0).value(), 0.0);
    QCOMPARE(session.valueAt("speed", 1).value(), 10.0);
    QCOMPARE(session.valueAt("speed", 2).value(), 30.0);
    QCOMPARE(session.valueAt("speed", 1.5).value(), 20.0);
    QCOMPARE(session.valueAt("speed", 1.6, InterpolationMode::Previous).value(), 10.0);
    QCOMPARE(session.valueAt("speed", 1.6, InterpolationMode::Nearest).value(), 30.0);
    QVERIFY(!session.valueAt("speed", -1));
    QVERIFY(!session.valueAt("speed", 9));
    QVERIFY(!session.valueAt("rpm", 1));
    QCOMPARE(videoToTelemetryTime(10, {2.5, 1.01}), 12.6);
}

void TelemetryTests::preservesMissingTelemetryGaps()
{
    TelemetrySession session;
    TelemetryChannel speed;
    speed.name = QStringLiteral("speed");
    speed.timestamps = {0.0, 0.1, 0.2};
    speed.values = {10.0F, std::numeric_limits<float>::quiet_NaN(), 30.0F};
    session.channels.insert(speed.name, speed);
    session.aliases.insert(QStringLiteral("speed"), speed.name);

    QVERIFY(!session.valueAt("speed", 0.05));
    QVERIFY(!session.valueAt("speed", 0.1));
    QVERIFY(!session.valueAt("speed", 0.15));
    QVERIFY(!session.valueAt("speed", 0.15, InterpolationMode::Previous));
    QVERIFY(!session.valueAt("speed", 0.11, InterpolationMode::Nearest));
    const QVector<QVector<QPointF>> gapSegments = session.sampledSegments("speed", 0.0, 0.2, 5);
    QCOMPARE(gapSegments.size(), 2);
    QCOMPARE(gapSegments.front(), QVector<QPointF>({QPointF(0.0, 10.0)}));
    QCOMPARE(gapSegments.back(), QVector<QPointF>({QPointF(0.2, 30.0)}));
    QVERIFY(!session.valueAt("speed", std::numeric_limits<double>::quiet_NaN()));
    QVERIFY(!session.valueAt("speed", std::numeric_limits<double>::infinity()));
    QVERIFY(!session.valueAt("speed", -std::numeric_limits<double>::infinity()));

    TelemetryChannel edgeValues;
    edgeValues.name = QStringLiteral("edge");
    edgeValues.timestamps = {0.0, 1.0, 2.0, 3.0};
    edgeValues.values = {std::numeric_limits<float>::quiet_NaN(), 10.0F, 20.0F,
                         std::numeric_limits<float>::quiet_NaN()};
    session.channels.insert(edgeValues.name, edgeValues);
    QVERIFY(!session.valueAt("edge", 0.0));
    QVERIFY(!session.valueAt("edge", 0.5));
    QCOMPARE(session.valueAt("edge", 1.0).value(), 10.0);
    QCOMPARE(session.valueAt("edge", 1.5).value(), 15.0);
    QVERIFY(!session.valueAt("edge", 2.5));
    QVERIFY(!session.valueAt("edge", 3.0));
    QVERIFY(!session.valueAt("edge", -0.001));
    QVERIFY(!session.valueAt("edge", 3.001));

    TelemetryChannel malformed;
    malformed.name = QStringLiteral("malformed");
    malformed.timestamps = {0.0, 1.0};
    malformed.values = {10.0F};
    session.channels.insert(malformed.name, malformed);
    QVERIFY(!session.valueAt("malformed", 0.0));

    TelemetryRenderContext context;
    context.setSession(&session);
    context.setTime(0.1);
    QCOMPARE(context.telemetryValue("speed").toDouble(), 10.0);
    QCOMPARE(context.valueText("speed"), QStringLiteral("10.00"));

    TelemetrySession positionSession;
    TelemetryChannel latitude;
    latitude.name = QStringLiteral("latitude");
    latitude.timestamps = {0.0, 1.0, 2.0};
    latitude.values = {52.0F, std::numeric_limits<float>::quiet_NaN(), 52.001F};
    TelemetryChannel longitude;
    longitude.name = QStringLiteral("longitude");
    longitude.timestamps = latitude.timestamps;
    longitude.values = {21.0F, std::numeric_limits<float>::quiet_NaN(), 21.001F};
    positionSession.channels.insert(latitude.name, latitude);
    positionSession.channels.insert(longitude.name, longitude);
    positionSession.aliases.insert(QStringLiteral("latitude"), latitude.name);
    positionSession.aliases.insert(QStringLiteral("longitude"), longitude.name);
    const TrackGeometry geometry = buildTrackGeometry(positionSession);
    QVERIFY(geometry.valid);
    QVERIFY(!currentTrackPoint(positionSession, -0.1, geometry));
    QVERIFY(!currentTrackPoint(positionSession, 1.0, geometry));
    QVERIFY(!currentTrackPoint(positionSession, 2.1, geometry));
}

void TelemetryTests::filtersOverlayPresentationValues()
{
    TelemetrySession session;
    const auto addChannel = [&session](const QString &name, QVector<float> values) {
        TelemetryChannel channel;
        channel.name = name;
        channel.timestamps = {0.0, 0.1, 0.2, 0.3};
        channel.values = std::move(values);
        session.channels.insert(name, channel);
        session.aliases.insert(name, name);
    };
    addChannel(QStringLiteral("speed"), {0.0F, 10.0F, std::numeric_limits<float>::quiet_NaN(), 30.0F});
    addChannel(QStringLiteral("rpm"), {0.0F, 1000.0F, std::numeric_limits<float>::quiet_NaN(), 3000.0F});
    addChannel(QStringLiteral("throttle"), {0.0F, 50.0F, std::numeric_limits<float>::quiet_NaN(), 100.0F});
    addChannel(QStringLiteral("brake"), {0.0F, 25.0F, std::numeric_limits<float>::quiet_NaN(), 0.0F});
    addChannel(QStringLiteral("lateralAcceleration"), {0.0F, 0.5F, std::numeric_limits<float>::quiet_NaN(), 1.0F});
    addChannel(QStringLiteral("longitudinalAcceleration"), {0.0F, -0.5F, std::numeric_limits<float>::quiet_NaN(), -1.0F});

    TelemetryRenderContext context;
    context.setSession(&session);
    context.setTime(0.0);
    QCOMPARE(context.telemetryValue("speed").toDouble(), 0.0);
    QCOMPARE(context.telemetryValue("rpm").toDouble(), 0.0);
    QCOMPARE(context.telemetryValue("throttle").toDouble(), 0.0);
    QCOMPARE(context.telemetryValue("brake").toDouble(), 0.0);
    QCOMPARE(context.telemetryValue("lateralAcceleration").toDouble(), 0.0);
    QCOMPARE(context.telemetryValue("longitudinalAcceleration").toDouble(), 0.0);
    QVERIFY(!context.telemetryValue("missing").isValid());

    context.setTime(0.2);
    QVERIFY(context.telemetryValue("speed").isValid());
    QVERIFY(context.telemetryValue("rpm").isValid());
    QVERIFY(context.telemetryValue("throttle").isValid());
    QVERIFY(context.telemetryValue("brake").isValid());
    QVERIFY(context.telemetryValue("lateralAcceleration").isValid());
    QVERIFY(context.telemetryValue("longitudinalAcceleration").isValid());

    context.setTime(1.1);
    QVERIFY(!context.telemetryValue("speed").isValid());
    QCOMPARE(context.valueText("speed"), QStringLiteral("—"));
}

void TelemetryTests::samplesTelemetryRanges()
{
    const auto session = VboParser::parse(
        u"[column names]\ntime speed\n[data]\n0 0\n1 10\n2 20\n3 30\n4 40");
    const QVector<QVector<QPointF>> segments = session.sampledSegments("speed", 1.0, 3.0, 5);
    QCOMPARE(segments.size(), 1);
    QCOMPARE(segments.front(), QVector<QPointF>({QPointF(1.0, 10.0), QPointF(2.0, 20.0), QPointF(3.0, 30.0)}));
    QVERIFY(session.sampledSegments("missing", 0.0, 1.0, 10).isEmpty());
    QVERIFY(session.sampledSegments("speed", 0.0, 1.0, 1).isEmpty());

    TelemetrySession extrema;
    TelemetryChannel signal;
    signal.name = QStringLiteral("rpm");
    for (int index = 0; index < 1000; ++index) {
        signal.timestamps.append(index / 100.0);
        signal.values.append(index == 513 ? 9000.0F : (index % 2 == 0 ? 1000.0F : 1001.0F));
    }
    extrema.channels.insert(signal.name, signal);
    const QVector<QVector<QPointF>> reduced = extrema.sampledSegments("rpm", 0.0, 10.0, 20);
    QCOMPARE(reduced.size(), 1);
    qsizetype pointCount = 0;
    bool retainedPeak = false;
    for (const QPointF &point : reduced.front()) {
        ++pointCount;
        retainedPeak = retainedPeak || point.y() == 9000.0;
    }
    QVERIFY(pointCount <= 40);
    QVERIFY(retainedPeak);

    TelemetrySession flat;
    TelemetryChannel flatSignal;
    flatSignal.name = QStringLiteral("flat");
    for (int index = 0; index < 100; ++index) {
        flatSignal.timestamps.append(index / 10.0);
        flatSignal.values.append(42.0F);
    }
    flat.channels.insert(flatSignal.name, flatSignal);
    const QVector<QVector<QPointF>> flatReduced = flat.sampledSegments("flat", 0.0, 10.0, 10);
    QCOMPARE(flatReduced.size(), 1);
    QVERIFY(flatReduced.front().size() <= 20);
    QVERIFY(std::all_of(flatReduced.front().cbegin(), flatReduced.front().cend(), [](const QPointF &point) {
        return point.y() == 42.0;
    }));

    TelemetrySession timestampGap;
    TelemetryChannel gapped;
    gapped.name = QStringLiteral("brake");
    gapped.timestamps = {0.0, 0.1, 0.2, 2.0, 2.1, 2.2};
    gapped.values = {0.0F, 10.0F, 20.0F, 80.0F, 90.0F, 100.0F};
    timestampGap.channels.insert(gapped.name, gapped);
    timestampGap.aliases.insert(gapped.name, gapped.name);
    const QVector<QVector<QPointF>> separated = timestampGap.sampledSegments("brake", 0.0, 2.2, 20);
    QCOMPARE(separated.size(), 2);
    QCOMPARE(separated.front().back(), QPointF(0.2, 20.0));
    QCOMPARE(separated.back().front(), QPointF(2.0, 80.0));

    TelemetryRenderContext presentation;
    presentation.setSession(&timestampGap);
    presentation.setTime(0.5);
    QVERIFY(presentation.telemetryValue("brake").isValid());
    presentation.setTime(1.0);
    QVERIFY(!presentation.telemetryValue("brake").isValid());

    const QVector<QVector<QPointF>> shortRange = extrema.sampledSegments("rpm", 5.13, 5.13, 2);
    QCOMPARE(shortRange.size(), 1);
    QCOMPARE(shortRange.front(), QVector<QPointF>({QPointF(5.13, 9000.0)}));
}

void TelemetryTests::parsesTextFirstVboTimeFormats()
{
    const auto timestampsFor = [](QStringView rows) {
        return VboParser::parse(QStringLiteral("[column names]\ntime speed\n[data]\n")
                                    + rows.toString())
            .channels.value(QStringLiteral("speed")).timestamps;
    };
    const auto verifyTimes = [](const QVector<double> &actual, const QVector<double> &expected) {
        QCOMPARE(actual.size(), expected.size());
        for (qsizetype index = 0; index < expected.size(); ++index) {
            QVERIFY2(qAbs(actual[index] - expected[index]) < 0.000001,
                     qPrintable(QStringLiteral("timestamp %1: %2 != %3")
                                    .arg(index).arg(actual[index], 0, 'f', 6)
                                    .arg(expected[index], 0, 'f', 6)));
        }
    };

    verifyTimes(timestampsFor(u"00:00:00.000 1\n00:00:00.100 2\n00:00:00.200 3"),
                {0.0, 0.1, 0.2});
    verifyTimes(timestampsFor(u"003059.500 1\n003100.500 2\n003101.500 3"),
                {0.0, 1.0, 2.0});
    verifyTimes(timestampsFor(u"091428.380 1\n091428.480 2\n091428.580 3"),
                {0.0, 0.1, 0.2});
    verifyTimes(timestampsFor(u"0 1\n0.1 2\n0.2 3\n10.5 4"), {0.0, 0.1, 0.2, 10.5});
}

void TelemetryTests::keepsVboTimestampsStrictlyMonotonic()
{
    const TelemetrySession midnight = VboParser::parse(
        u"[column names]\ntime speed rpm\n[data]\n"
        "235959.800 1 10\n235959.900 2 20\n000000.000 3 30\n000000.100 4 40");
    const TelemetryChannel midnightSpeed = midnight.channels.value(QStringLiteral("speed"));
    QCOMPARE(midnightSpeed.timestamps.size(), 4);
    QVERIFY(qAbs(midnightSpeed.timestamps[0]) < 0.000001);
    QVERIFY(qAbs(midnightSpeed.timestamps[1] - 0.1) < 0.000001);
    QVERIFY(qAbs(midnightSpeed.timestamps[2] - 0.2) < 0.000001);
    QVERIFY(qAbs(midnightSpeed.timestamps[3] - 0.3) < 0.000001);
    QCOMPARE(midnight.channels.value(QStringLiteral("rpm")).timestamps.size(), 4);
    QVERIFY(std::any_of(midnight.warnings.cbegin(), midnight.warnings.cend(), [](const QString &warning) {
        return warning.contains(QStringLiteral("midnight rollover"));
    }));

    const TelemetrySession guarded = VboParser::parse(
        u"[column names]\ntime speed rpm\n[data]\n"
        "120000.000 1 10\n120000.100 2 20\n120000.100 3 30\n115959.900 4 40\n"
        "126199 5 50\n246000 6 60\n12:61:00 7 70\n12:00:60 8 80\n120000.200 9 90");
    const TelemetryChannel speed = guarded.channels.value(QStringLiteral("speed"));
    QCOMPARE(speed.timestamps.size(), 3);
    QCOMPARE(speed.values, QVector<float>({1.0F, 2.0F, 9.0F}));
    QCOMPARE(guarded.channels.value(QStringLiteral("rpm")).values.size(), speed.timestamps.size());
    for (qsizetype index = 1; index < speed.timestamps.size(); ++index) {
        QVERIFY(speed.timestamps[index] > speed.timestamps[index - 1]);
    }
    QVERIFY(guarded.warnings.size() >= 6);
}

void TelemetryTests::cancelsVboParsingDeterministically()
{
    QString text = QStringLiteral("[column names]\ntime speed\n[data]\n");
    text.reserve(200'000);
    for (int row = 0; row < 10'000; ++row) text += QStringLiteral("%1 %2\n").arg(row).arg(row % 200);
    int checks = 0;
    QVERIFY_THROWS_EXCEPTION(
        OperationCancelled,
        (void) VboParser::parse(text, [&checks] { return ++checks == 4; }));
    QVERIFY(checks >= 4);
}

void TelemetryTests::enforcesVboResourceLimits()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QFile oversized(directory.filePath(QStringLiteral("oversized.vbo")));
    QVERIFY(oversized.open(QIODevice::WriteOnly));
    QVERIFY(oversized.resize(VboParser::kMaximumFileBytes + 1));
    oversized.close();
    QVERIFY_THROWS_EXCEPTION(ResourceLimitError, (void) VboParser::parseFile(oversized.fileName()));

    QStringList names;
    QStringList values;
    for (qsizetype column = 0; column <= VboParser::kMaximumColumns; ++column) {
        names.append(QStringLiteral("c%1").arg(column));
        values.append(QStringLiteral("1"));
    }
    QVERIFY_THROWS_EXCEPTION(
        ResourceLimitError,
        (void) VboParser::parse(QStringLiteral("[column names]\n%1\n[data]\n%2")
                                    .arg(names.join(' '), values.join(' '))));

    QString rows = QStringLiteral("[column names]\ntime speed\n[data]\n");
    rows.reserve(static_cast<qsizetype>(VboParser::kMaximumDataRows) * 4);
    for (qsizetype row = 0; row <= VboParser::kMaximumDataRows; ++row) rows += QStringLiteral("0 1\n");
    QVERIFY_THROWS_EXCEPTION(ResourceLimitError, (void) VboParser::parse(rows));

    const QString longField(VboParser::kMaximumFieldCharacters + 1, QLatin1Char('1'));
    QVERIFY_THROWS_EXCEPTION(
        ResourceLimitError,
        (void) VboParser::parse(
            QStringLiteral("[column names]\ntime speed\n[data]\n0 %1").arg(longField)));
}

void TelemetryTests::convertsArcMinuteCoordinates()
{
    const auto session = VboParser::parse(
        u"[column names]\ntime latitude longitude\n[data]\n0 3120 -1260\n1 3126 -1266");
    QCOMPARE(session.channels.value("latitude").values[0], 52.0F);
    QCOMPARE(session.channels.value("longitude").values[0], -21.0F);
}

void TelemetryTests::parsesOptionalRealVbo()
{
    const QString path = qEnvironmentVariable("FLAPPEDEAR_REAL_VBO");
    if (path.isEmpty()) {
        QSKIP("FLAPPEDEAR_REAL_VBO is not set");
    }
    const auto session = VboParser::parseFile(path);
    QCOMPARE(session.sampleCount, 32'718);
    QCOMPARE(session.channels.size(), 49);
    QVERIFY(qAbs(session.duration - 3'271.7) < 0.001);
    QVERIFY(session.warnings.isEmpty());
    QCOMPARE(session.aliases.value("speed"), QStringLiteral("velocity"));
    QVERIFY(session.aliases.contains("rpm"));
    QVERIFY(qAbs(session.valueAt("speed", 0.0).value() - 0.11) < 0.001);
    qsizetype nonFiniteSamples = 0;
    for (const TelemetryChannel &channel : session.channels) {
        QCOMPARE(channel.timestamps.size(), session.sampleCount);
        QCOMPARE(channel.values.size(), session.sampleCount);
        for (const float value : channel.values) {
            if (!std::isfinite(value)) {
                ++nonFiniteSamples;
            }
        }
    }
    QCOMPARE(nonFiniteSamples, 0);
}

void TelemetryTests::persistsWidgetScenes()
{
    WidgetModel source;
    source.resetDefaults();
    const int custom = source.addWidget("customValue");
    source.setSetting(custom, "source", "oiltemp");
    source.setSetting(custom, "label", "Oil temperature");
    source.setWidgetProperty(custom, "rotation", 12.0);

    WidgetModel restored;
    QVERIFY(restored.fromJson(source.toJson()));
    QCOMPARE(restored.count(), source.count());
    const QVariantMap widget = restored.widget(custom);
    QCOMPARE(widget.value("type").toString(), QString("customValue"));
    QCOMPARE(widget.value("rotation").toDouble(), 12.0);
    QCOMPARE(widget.value("settings").toMap().value("source").toString(), QString("oiltemp"));
}

void TelemetryTests::loadsVisualTemplates()
{
    WidgetModel model;
    QVERIFY(model.templates().size() >= 3);
    QVERIFY(model.applyTemplate("minimal"));
    QCOMPARE(model.count(), 2);
    QCOMPARE(model.widget(0).value("type").toString(), QString("speed"));
    QCOMPARE(
        model.widget(0).value("settings").toMap().value("showBackground").toBool(),
        false);
    QVERIFY(model.applyTemplate("performance"));
    QCOMPARE(model.count(), 3);
    QCOMPARE(model.widget(0).value("type").toString(), QString("arcGauge"));
    QCOMPARE(model.widget(1).value("type").toString(), QString("dialGauge"));
    QCOMPARE(model.widget(2).value("type").toString(), QString("telemetryOverlay"));
    QVERIFY(model.applyTemplate("2000s-grand-prix"));
    QCOMPARE(model.count(), 6);
    QCOMPARE(model.widget(0).value("type").toString(), QString("retroTachometer"));
    QCOMPARE(model.widget(1).value("type").toString(), QString("retroGear"));
    QCOMPARE(model.widget(2).value("type").toString(), QString("retroPedal"));
    QCOMPARE(model.widget(3).value("settings").toMap().value("source").toString(), QString("brake_pos-obd"));
    QVERIFY(!model.applyTemplate("missing-template"));
}

void TelemetryTests::providesCustomizableArchetypes()
{
    WidgetModel model;
    const QHash<QString, QStringList> specialized = {
        {"speed", {"showGauge", "unit", "maxValue"}},
        {"rpm", {"showBar", "warningValue", "maxValue"}},
        {"heartRate", {"showIcon", "unit", "accentColor"}},
        {"pedals", {"acceleratorSource", "brakeSource", "acceleratorColor", "brakeColor"}},
        {"gForce", {"lateralSource", "longitudinalSource", "gRange", "gridColor"}},
        {"track", {"lineColor", "lineWidth", "markerColor", "mirrorX", "mirrorY"}},
        {"customValue", {"label", "decimals", "multiplier"}},
        {"arcGauge", {"source", "startAngle", "endAngle", "arcWidth", "trackColor"}},
        {"dialGauge", {"source", "startAngle", "endAngle", "majorTicks", "needleColor"}},
        {"telemetryOverlay", {"source1", "source2", "source3", "source4", "columns"}},
        {"retroGrandPrix", {"rpmSource", "speedSource", "gearSource", "throttleSource", "brakeSource", "driverName"}},
        {"retroTachometer", {"source", "minValue", "maxValue", "needleColor"}},
        {"retroGear", {"source", "label", "fallbackText", "panelColor"}},
        {"retroPedal", {"source", "minValue", "maxValue", "fillColor", "emptyColor"}},
        {"retroSpeedArc", {"source", "minValue", "maxValue", "segments", "lowColor"}},
        {"retroNameplate", {"topSource", "bottomSource", "topText", "bottomText"}},
        {"brandLogo", {"logoOpacity", "logoScale"}},
    };
    for (auto iterator = specialized.cbegin(); iterator != specialized.cend(); ++iterator) {
        const int index = model.addWidget(iterator.key());
        QVERIFY(index >= 0);
        const QVariantMap settings = model.widget(index).value("settings").toMap();
        for (const QString &common : {
                 "backgroundColor", "backgroundOpacity", "borderColor", "cornerRadius",
                 "textColor", "secondaryTextColor", "accentColor", "fontFamily",
                 "fontWeight", "valueFontScale", "labelFontScale", "padding"}) {
            QVERIFY2(settings.contains(common), qPrintable(iterator.key() + ": " + common));
        }
        for (const QString &key : iterator.value()) {
            QVERIFY2(settings.contains(key), qPrintable(iterator.key() + ": " + key));
        }
    }
}

void TelemetryTests::persistsAndSharesCustomTemplates()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const bool hadOverride = qEnvironmentVariableIsSet("FLAPPEDEAR_TEMPLATE_STORE");
    const QByteArray previousOverride = qgetenv("FLAPPEDEAR_TEMPLATE_STORE");
    qputenv(
        "FLAPPEDEAR_TEMPLATE_STORE",
        directory.filePath("layout-templates.json").toUtf8());
    const auto restoreEnvironment = qScopeGuard([hadOverride, previousOverride] {
        if (hadOverride) {
            qputenv("FLAPPEDEAR_TEMPLATE_STORE", previousOverride);
        } else {
            qunsetenv("FLAPPEDEAR_TEMPLATE_STORE");
        }
    });

    WidgetModel source;
    QVERIFY(source.applyTemplate("2000s-grand-prix"));
    const QString templateId =
        source.saveCurrentAsTemplate("My broadcast", "Reusable race insert");
    QVERIFY(!templateId.isEmpty());

    WidgetModel restored;
    QVERIFY(restored.applyTemplate(templateId));
    QCOMPARE(restored.count(), source.count());
    const QUrl exported = QUrl::fromLocalFile(directory.filePath("shared.fettemplate"));
    QVERIFY(restored.exportTemplate(templateId, exported));
    QVERIFY(QFileInfo::exists(exported.toLocalFile()));
    QVERIFY(restored.deleteTemplate(templateId));
    QVERIFY(!restored.applyTemplate(templateId));

    const QString importedId = restored.importTemplate(exported);
    QVERIFY(!importedId.isEmpty());
    QVERIFY(restored.applyTemplate(importedId));
    QCOMPARE(restored.widget(0).value("type").toString(), QString("retroTachometer"));
}

void TelemetryTests::persistsWidgetAnimationCues()
{
    WidgetModel source;
    const int widget = source.addWidget("telemetryOverlay");
    QCOMPARE(source.addCue(widget, 12.5, 4.0, "slideUp"), 0);
    source.setCueProperty(widget, 0, "fadeIn", 0.6);
    source.setCueProperty(widget, 0, "fadeOut", 0.8);

    WidgetModel restored;
    QVERIFY(restored.fromJson(source.toJson()));
    const QVariantList cues = restored.widget(0).value("cues").toList();
    QCOMPARE(cues.size(), 1);
    const QVariantMap cue = cues.front().toMap();
    QCOMPARE(cue.value("start").toDouble(), 12.5);
    QCOMPARE(cue.value("duration").toDouble(), 4.0);
    QCOMPARE(cue.value("fadeIn").toDouble(), 0.6);
    QCOMPARE(cue.value("fadeOut").toDouble(), 0.8);
    QCOMPARE(cue.value("effect").toString(), QString("slideUp"));
    restored.removeCue(0, 0);
    QVERIFY(restored.widget(0).value("cues").toList().isEmpty());
}

void TelemetryTests::groupsAndMovesWidgets()
{
    WidgetModel model;
    const int first = model.addWidget("retroGear");
    const int second = model.addWidget("retroPedal");
    const double firstX = model.widget(first).value("x").toDouble();
    const double secondX = model.widget(second).value("x").toDouble();
    const QString groupId = model.groupWidgets({first, second});
    QVERIFY(!groupId.isEmpty());
    QCOMPARE(model.groupMembers(first), QVariantList({first, second}));

    model.moveWidget(first, firstX + 0.1, model.widget(first).value("y").toDouble());
    QCOMPARE(model.widget(first).value("x").toDouble(), firstX + 0.1);
    QCOMPARE(model.widget(second).value("x").toDouble(), secondX + 0.1);

    WidgetModel restored;
    QVERIFY(restored.fromJson(model.toJson()));
    QCOMPARE(restored.groupMembers(second), QVariantList({first, second}));
    restored.ungroupWidget(first);
    QCOMPARE(restored.groupMembers(first), QVariantList{QVariant(first)});
    restored.removeWidgets({first, second});
    QCOMPARE(restored.count(), 0);
}

void TelemetryTests::constrainsWidgetGeometry()
{
    WidgetModel model;
    const int index = model.addWidget("speed");
    model.resizeWidget(index, 0.4, 0.3);
    model.moveWidget(index, 0.9, -1.0);
    const QVariantMap widget = model.widget(index);
    QCOMPARE(widget.value("x").toDouble(), 0.6);
    QCOMPARE(widget.value("y").toDouble(), 0.0);
    model.setWidgetProperty(index, "opacity", 4.0);
    QCOMPARE(model.widget(index).value("opacity").toDouble(), 1.0);
}

void TelemetryTests::buildsTrackGeometry()
{
    const TelemetrySession session = VboParser::parseFile(TEST_FIXTURE_PATH);
    const TrackGeometry geometry = buildTrackGeometry(session);
    QVERIFY(geometry.valid);
    QCOMPARE(geometry.points.size(), 3);
    QVERIFY(geometry.points.front().x() > 0.0);
    QCOMPARE(geometry.points.front().y(), 1.0);
    QVERIFY(geometry.points.back().x() < 1.0);
    QCOMPARE(geometry.points.back().y(), 0.0);
    const auto current = currentTrackPoint(session, 0.5, geometry);
    QVERIFY(current.has_value());
    QVERIFY2(
        qAbs(current->x() - 0.5) < 0.01,
        qPrintable(QStringLiteral("x=%1").arg(current->x(), 0, 'g', 12)));
    QVERIFY2(
        qAbs(current->y() - 0.5) < 0.01,
        qPrintable(QStringLiteral("y=%1").arg(current->y(), 0, 'g', 12)));
}

void TelemetryTests::cachesStaticTrackGeometry()
{
    constexpr qsizetype pointCount = 30'000;
    TrackGeometry geometry;
    geometry.valid = true;
    geometry.originLatitude = 52.0;
    geometry.originLongitude = 21.0;
    geometry.localCenter = {0.0, 0.0};
    geometry.normalizationScale = 1.0;
    geometry.points.reserve(pointCount);

    TelemetrySession session;
    TelemetryChannel latitude;
    latitude.name = QStringLiteral("latitude");
    latitude.timestamps.reserve(pointCount);
    latitude.values.reserve(pointCount);
    TelemetryChannel longitude;
    longitude.name = QStringLiteral("longitude");
    longitude.timestamps.reserve(pointCount);
    longitude.values.reserve(pointCount);
    for (qsizetype index = 0; index < pointCount; ++index) {
        const double progress = static_cast<double>(index) / static_cast<double>(pointCount - 1);
        geometry.points.append(
            {progress, 0.5 + 0.4 * std::sin(progress * 8.0 * std::numbers::pi)});
        const double timestamp = static_cast<double>(index) / 10.0;
        latitude.timestamps.append(timestamp);
        longitude.timestamps.append(timestamp);
        latitude.values.append(static_cast<float>(52.0 + progress * 0.001));
        longitude.values.append(static_cast<float>(21.0 + progress * 0.001));
    }
    session.channels.insert(latitude.name, latitude);
    session.channels.insert(longitude.name, longitude);
    session.aliases.insert(QStringLiteral("latitude"), latitude.name);
    session.aliases.insert(QStringLiteral("longitude"), longitude.name);

    TelemetryRenderContext context;
    context.setSession(&session);
    QSignalSpy geometryChanges(&context, &TelemetryRenderContext::trackGeometryChanged);
    QElapsedTimer constructionTimer;
    constructionTimer.start();
    context.setTrackGeometry(&geometry);
    const qint64 constructionNanoseconds = constructionTimer.nsecsElapsed();
    QCOMPARE(context.trackPoints().size(), pointCount);
    QCOMPARE(context.trackRevision(), quint64(1));
    QCOMPARE(context.trackConversionCount(), quint64(1));
    QCOMPARE(geometryChanges.count(), 1);

    const QVariantMap startPoint = context.currentTrackPoint();
    QElapsedTimer updatesTimer;
    updatesTimer.start();
    for (int update = 1; update <= 10'000; ++update) {
        context.setTime(static_cast<double>(update % pointCount) / 10.0);
        QVERIFY(context.currentTrackPoint().contains(QStringLiteral("x")));
        QCOMPARE(context.trackPoints().size(), pointCount);
    }
    const qint64 updateNanoseconds = updatesTimer.nsecsElapsed();
    const QVariantMap laterPoint = context.currentTrackPoint();
    QVERIFY(startPoint != laterPoint);
    QCOMPARE(context.trackConversionCount(), quint64(1));
    QCOMPARE(context.trackRevision(), quint64(1));
    QCOMPARE(geometryChanges.count(), 1);
    const quint64 additionalConversions = context.trackConversionCount() - 1;

    geometry.points = {{0.1, 0.2}, {0.8, 0.9}};
    context.setTrackGeometry(&geometry); // The owner intentionally reuses the same storage address.
    QCOMPARE(context.trackPoints().size(), 2);
    QCOMPARE(context.trackPoints().front().toPointF(), QPointF(0.1, 0.2));
    QCOMPARE(context.trackConversionCount(), quint64(2));
    QCOMPARE(context.trackRevision(), quint64(2));
    QCOMPARE(geometryChanges.count(), 2);

    context.setTrackGeometry(nullptr);
    QVERIFY(context.trackPoints().isEmpty());
    QVERIFY(context.currentTrackPoint().isEmpty());
    QCOMPARE(context.trackConversionCount(), quint64(2));
    QCOMPARE(context.trackRevision(), quint64(3));
    QCOMPARE(geometryChanges.count(), 3);

    qInfo().nospace() << "track-cache-benchmark points=" << pointCount
                      << " initial-cache-ms=" << constructionNanoseconds / 1'000'000.0
                      << " time-marker-updates=10000 updates-ms="
                      << updateNanoseconds / 1'000'000.0
                      << " additional-conversions=" << additionalConversions;
}

void TelemetryTests::keepsStaticTrackIndependentFromTime()
{
    QFile source(QStringLiteral(TRACK_WIDGET_QML_PATH));
    QVERIFY2(source.open(QIODevice::ReadOnly), qPrintable(source.errorString()));
    const QByteArray qml = source.readAll();
    QVERIFY(qml.contains("frame.renderContext.trackRevision"));
    QVERIFY(qml.contains("PathPolyline"));
    QVERIFY(!qml.contains("function onTimeChanged()"));
    QVERIFY(!qml.contains("requestPaint"));
    QVERIFY(!qml.contains("onRevisionChanged"));
}

void TelemetryTests::gatesWeakSyncCandidates()
{
    SyncCandidate strong;
    strong.confidence = 0.75;
    QVERIFY(shouldAutoApplySyncCandidate(strong));
    QVERIFY(syncConfidenceLevel(strong.confidence) == SyncConfidenceLevel::High);

    SyncCandidate weak;
    weak.confidence = 0.43;
    QVERIFY(!shouldAutoApplySyncCandidate(weak));
    QVERIFY(syncConfidenceLevel(weak.confidence) == SyncConfidenceLevel::Low);

    TelemetrySession rectangle;
    TelemetryChannel latitude;
    latitude.name = "latitude";
    latitude.timestamps = {0.0, 1.0, 2.0, 3.0};
    latitude.values = {0.0F, 0.0F, 0.001F, 0.001F};
    TelemetryChannel longitude;
    longitude.name = "longitude";
    longitude.timestamps = latitude.timestamps;
    longitude.values = {0.0F, 0.004F, 0.004F, 0.0F};
    rectangle.channels.insert(latitude.name, latitude);
    rectangle.channels.insert(longitude.name, longitude);
    rectangle.aliases.insert("latitude", latitude.name);
    rectangle.aliases.insert("longitude", longitude.name);
    const TrackGeometry geometry = buildTrackGeometry(rectangle);
    QVERIFY(geometry.valid);
    const double width = geometry.points[1].x() - geometry.points[0].x();
    const double height = geometry.points[0].y() - geometry.points[2].y();
    QVERIFY2(qAbs(width / height - 4.0) < 0.05,
             qPrintable(QStringLiteral("aspect=%1").arg(width / height, 0, 'f', 3)));
    const auto current = currentTrackPoint(rectangle, 1.0, geometry);
    QVERIFY(current.has_value());
    QVERIFY(qAbs(current->x() - geometry.points[1].x()) < 0.001);
    QVERIFY(qAbs(current->y() - geometry.points[1].y()) < 0.001);
}

void TelemetryTests::rendersTelemetryAtExplicitTime()
{
    const TelemetrySession session = VboParser::parse(
        u"[column names]\ntime speed\n[data]\n0 0\n10 100");
    TelemetryRenderContext context;
    context.setSession(&session);
    context.setSyncTransform({2.0, 1.5});
    context.setTime(4.0);
    QCOMPARE(context.telemetryTime(), 8.0);
    QCOMPARE(context.telemetryValue("speed").toDouble(), 80.0);
    context.setTime(2.0);
    QCOMPARE(context.telemetryValue("speed").toDouble(), 50.0);
}

void TelemetryTests::probesMediaInfoJson()
{
    const QByteArray json = R"({"format":{"duration":"3.000000","start_time":"0.500000"},"streams":[{"codec_type":"video","codec_name":"h264","width":320,"height":180,"r_frame_rate":"30000/1001","avg_frame_rate":"30000/1001","time_base":"1/90000","pix_fmt":"yuv420p","start_time":"0.500000","duration":"3.003000","nb_read_frames":"90","nb_read_packets":"90"},{"codec_type":"audio","codec_name":"aac","start_time":"0.500000","duration":"3.021333","time_base":"1/48000","sample_rate":"48000"}]})";
    const MediaInfo info = MediaProbe::parseJson(json, "/fixture.mp4");
    QCOMPARE(info.path, QString("/fixture.mp4"));
    QCOMPARE(info.videoSize, QSize(320, 180));
    QCOMPARE(info.videoCodec, QString("h264"));
    QCOMPARE(info.videoFrameCount, qsizetype(90));
    QCOMPARE(info.videoPacketCount, qsizetype(90));
    QCOMPARE(info.audioCodecs, QStringList({"aac"}));
    QCOMPARE(info.videoStartTime, 0.5);
    QCOMPARE(info.videoDuration, 3.003);
    QCOMPARE(info.audioStartTime, 0.5);
    QCOMPARE(info.audioDuration, 3.021333);
    QCOMPARE(info.audioSampleRate, 48000);
    QVERIFY(info.audioTimeBase.isEquivalentTo({1, 48000}));
    QVERIFY(qAbs(info.frameRate.value() - 29.97002997) < 0.00001);
    QVERIFY(!info.likelyVariableFrameRate);
}

void TelemetryTests::parsesMediaSummaryJson()
{
    const QByteArray json = R"({"format":{"duration":"120.003000"},"streams":[{"index":0,"codec_type":"video","codec_name":"hevc","width":3840,"height":2160,"r_frame_rate":"60000/1001","avg_frame_rate":"60000/1001"},{"index":1,"codec_type":"audio","codec_name":"aac"}]})";
    const MediaInfo info = MediaProbe::parseJson(json, "/summary.mp4");
    QCOMPARE(info.videoCodec, QStringLiteral("hevc"));
    QCOMPARE(info.videoSize, QSize(3840, 2160));
    QVERIFY(qAbs(info.duration - 120.003) < 0.0001);
    QVERIFY(qAbs(info.averageFrameRate.value() - 59.94005994) < 0.00001);
    QCOMPARE(info.audioCodecs, QStringList({QStringLiteral("aac")}));

    const QByteArray silentJson = R"({"format":{"duration":"12.000000"},"streams":[{"index":0,"codec_type":"video","codec_name":"hevc","width":1920,"height":1080,"r_frame_rate":"30/1","avg_frame_rate":"30/1"}]})";
    const MediaInfo silentInfo = MediaProbe::parseJson(silentJson, "/silent.mp4");
    QVERIFY(silentInfo.audioCodecs.isEmpty());
}

void TelemetryTests::rejectsInvalidMediaProbeJson()
{
    try {
        static_cast<void>(MediaProbe::parseJson("not-json", "/invalid.mp4"));
        QFAIL("Invalid ffprobe JSON should throw.");
    } catch (const std::runtime_error &error) {
        QCOMPARE(QString::fromUtf8(error.what()), QStringLiteral("ffprobe returned invalid JSON."));
    }
}

void TelemetryTests::classifiesMediaProbeProcessFailures()
{
    const auto errorFrom = [](const std::function<void()> &operation) {
        try {
            operation();
        } catch (const std::runtime_error &error) {
            return QString::fromUtf8(error.what());
        }
        return QString();
    };

    const QString startError = errorFrom([] {
        static_cast<void>(MediaProbe::probe(
            "/fixture.mp4", "/definitely/missing/flappedear-ffprobe", false, 100));
    });
    QVERIFY2(startError.startsWith("Could not start ffprobe while probing: /fixture.mp4"),
             qPrintable(startError));

#ifdef Q_OS_UNIX
    const QString exitError = errorFrom([] {
        static_cast<void>(MediaProbe::probe("/fixture.mp4", "/usr/bin/false", false, 1'000));
    });
    QVERIFY2(exitError.contains("ffprobe exited with code 1 while probing: /fixture.mp4"),
             qPrintable(exitError));
    QVERIFY2(exitError.contains("stderr: <no stderr output>"), qPrintable(exitError));

    const QString jsonError = errorFrom([] {
        static_cast<void>(MediaProbe::probe("/fixture.mp4", "/usr/bin/true", false, 1'000));
    });
    QCOMPARE(jsonError, QStringLiteral("ffprobe returned invalid JSON."));

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString slowProbePath = directory.filePath("slow-ffprobe");
    QFile slowProbe(slowProbePath);
    QVERIFY(slowProbe.open(QIODevice::WriteOnly));
    QVERIFY(slowProbe.write("#!/bin/sh\nwhile :; do :; done\n") > 0);
    slowProbe.close();
    QVERIFY(slowProbe.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner
                                     | QFileDevice::ExeOwner));
    const QString timeoutError = errorFrom([&slowProbePath] {
        static_cast<void>(MediaProbe::probe("/fixture.mp4", slowProbePath, false, 10));
    });
    QVERIFY2(timeoutError.startsWith("ffprobe timed out after 0.010 seconds while probing: /fixture.mp4"),
             qPrintable(timeoutError));
    QVERIFY2(timeoutError.contains("stderr: <no stderr output>"), qPrintable(timeoutError));

    const QString crashingProbePath = directory.filePath("crashing-ffprobe");
    QFile crashingProbe(crashingProbePath);
    QVERIFY(crashingProbe.open(QIODevice::WriteOnly));
    QVERIFY(crashingProbe.write("#!/bin/sh\nkill -SEGV $$\n") > 0);
    crashingProbe.close();
    QVERIFY(crashingProbe.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner
                                         | QFileDevice::ExeOwner));
    const QString crashError = errorFrom([&crashingProbePath] {
        static_cast<void>(MediaProbe::probe("/fixture.mp4", crashingProbePath, false, 1'000));
    });
    QVERIFY2(crashError.startsWith("ffprobe crashed while probing: /fixture.mp4"),
             qPrintable(crashError));
#endif
}

void TelemetryTests::reportsMediaProbeLifecycleHeartbeat()
{
#ifdef Q_OS_UNIX
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString probePath = directory.filePath("heartbeat-ffprobe");
    QFile probe(probePath);
    QVERIFY(probe.open(QIODevice::WriteOnly));
    QVERIFY(probe.write(
        "#!/bin/sh\n"
        "sleep 0.7\n"
        "printf '%s\\n' '{\"format\":{\"duration\":\"1.0\"},\"streams\":[{\"codec_type\":\"video\",\"codec_name\":\"hevc\",\"width\":16,\"height\":16,\"r_frame_rate\":\"30/1\",\"avg_frame_rate\":\"30/1\"}]}'\n") > 0);
    probe.close();
    QVERIFY(probe.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner
                                 | QFileDevice::ExeOwner));
    QList<MediaProbeEvent::Phase> phases;
    const MediaInfo info = MediaProbe::probe(
        "/fixture.mp4", probePath, false, 2'000,
        [&phases](const MediaProbeEvent &event) { phases.append(event.phase); });
    QCOMPARE(info.videoCodec, QStringLiteral("hevc"));
    QCOMPARE(phases.first(), MediaProbeEvent::Phase::Started);
    QVERIFY(phases.contains(MediaProbeEvent::Phase::Heartbeat));
    QCOMPARE(phases.last(), MediaProbeEvent::Phase::Finished);
#else
    QSKIP("Lifecycle helper script requires a POSIX shell.");
#endif
}

void TelemetryTests::cancelsMediaProbeWithoutLeavingItRunning()
{
#ifdef Q_OS_UNIX
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString probePath = directory.filePath("cancel-probe.sh");
    QFile probe(probePath);
    QVERIFY(probe.open(QIODevice::WriteOnly | QIODevice::Truncate));
    QCOMPARE(probe.write("#!/bin/sh\nsleep 10\n"), qint64(19));
    probe.close();
    QVERIFY(probe.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner
                                 | QFileDevice::ExeOwner));
    QString error;
    try {
        static_cast<void>(MediaProbe::probe(
            "/fixture.mp4", probePath, false, 20'000, {}, [] { return true; }));
    } catch (const std::exception &exception) {
        error = QString::fromUtf8(exception.what());
    }
    QVERIFY2(error.startsWith("ffprobe cancelled while probing: /fixture.mp4"), qPrintable(error));
#else
    QSKIP("Lifecycle helper script requires a POSIX shell.");
#endif
}

void TelemetryTests::evaluatesIndependentExportStorageVolumes()
{
    const ExportStorageEstimate estimate{100, 50, 25};
    const auto provider = [](const QString &path) {
        return path.startsWith(QStringLiteral("/temp"))
            ? ExportFilesystemInfo{QStringLiteral("/temporary"), path, path, 130, 1'000}
            : ExportFilesystemInfo{QStringLiteral("/destination"), path, path, 80, 1'000};
    };
    const ExportStoragePreflight enough = ExportStoragePolicy::evaluate("/temp/overlay", "/output/final", estimate, provider);
    QVERIFY(enough.sufficient);
    const auto insufficientTemp = [](const QString &path) {
        return path.startsWith(QStringLiteral("/temp"))
            ? ExportFilesystemInfo{QStringLiteral("/temporary"), path, path, 124, 1'000}
            : ExportFilesystemInfo{QStringLiteral("/destination"), path, path, 80, 1'000};
    };
    QVERIFY(!ExportStoragePolicy::evaluate("/temp/overlay", "/output/final", estimate, insufficientTemp).sufficient);
    const auto insufficientOutput = [](const QString &path) {
        return path.startsWith(QStringLiteral("/temp"))
            ? ExportFilesystemInfo{QStringLiteral("/temporary"), path, path, 130, 1'000}
            : ExportFilesystemInfo{QStringLiteral("/destination"), path, path, 74, 1'000};
    };
    QVERIFY(!ExportStoragePolicy::evaluate("/temp/overlay", "/output/final", estimate, insufficientOutput).sufficient);
    const auto sharedFilesystem = [](const QString &path) {
        return ExportFilesystemInfo{QStringLiteral("/shared"), path, path, 174, 1'000};
    };
    QVERIFY(!ExportStoragePolicy::evaluate("/temp/overlay", "/output/final", estimate, sharedFilesystem).sufficient);
}

void TelemetryTests::resolvesExportFilesystemsForFutureArtifacts()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString existingFile = directory.filePath(QStringLiteral("existing.mkv"));
    QVERIFY(writeBytes(existingFile, "fixture"));

    const auto verifyUsable = [](const ExportFilesystemInfo &filesystem, const QString &requested) {
        QVERIFY2(filesystem.isUsable(), qPrintable(requested));
        QCOMPARE(filesystem.inspectedPath, requested);
        QVERIFY(QFileInfo::exists(filesystem.probePath));
        QVERIFY(filesystem.availableBytes > 0);
        QVERIFY(filesystem.totalBytes > 0);
    };

    const ExportFilesystemInfo fileFilesystem = ExportStoragePolicy::filesystemForPath(existingFile);
    verifyUsable(fileFilesystem, existingFile);
    QCOMPARE(fileFilesystem.probePath, QFileInfo(existingFile).absoluteFilePath());

    const ExportFilesystemInfo directoryFilesystem = ExportStoragePolicy::filesystemForPath(directory.path());
    verifyUsable(directoryFilesystem, directory.path());
    QCOMPARE(directoryFilesystem.probePath, QFileInfo(directory.path()).absoluteFilePath());

    const QString futureFile = directory.filePath(QStringLiteral("future.mkv"));
    const ExportFilesystemInfo futureFilesystem = ExportStoragePolicy::filesystemForPath(futureFile);
    verifyUsable(futureFilesystem, futureFile);
    QCOMPARE(futureFilesystem.probePath, QFileInfo(directory.path()).absoluteFilePath());

    const QString nestedFuture = directory.filePath(QStringLiteral("a/b/c/output.mkv"));
    const ExportFilesystemInfo nestedFilesystem = ExportStoragePolicy::filesystemForPath(nestedFuture);
    verifyUsable(nestedFilesystem, nestedFuture);
    QCOMPARE(nestedFilesystem.probePath, QFileInfo(directory.path()).absoluteFilePath());

    const ExportFilesystemInfo unresolved = ExportStoragePolicy::filesystemForPath(QString());
    QVERIFY(!unresolved.isUsable());
    QCOMPARE(unresolved.inspectedPath, QString());
    QCOMPARE(ExportStoragePolicy::bytesText(unresolved.availableBytes), QStringLiteral("unavailable"));
}

void TelemetryTests::estimatesTemporaryStorageFromRepresentativeSample()
{
    constexpr qsizetype expectedFrames = 7'193;
    // 513,343,525 bytes was the observed complete 4K FFV1 overlay size.
    constexpr qint64 representativeBytesPerFrame = 71'368;
    const ExportStorageEstimate measured = ExportStoragePolicy::estimateFromSample(
        representativeBytesPerFrame * 24, 24, expectedFrames, 120.0, 12'000'000);
    QCOMPARE(measured.basis, ExportStorageEstimate::Basis::MeasuredSample);
    QCOMPARE(measured.sampleFrames, qsizetype(24));
    QCOMPARE(measured.bytesPerFrame, representativeBytesPerFrame);
    QCOMPARE(measured.safetyMargin, 1.5);
    QVERIFY(measured.temporaryOverlayBytes > 700LL * 1024 * 1024);
    QVERIFY2(measured.temporaryOverlayBytes < 2LL * 1024 * 1024 * 1024,
             "Measured representative sample must not regress to a tens-of-GiB estimate.");

    const ExportStorageEstimate fallback = ExportStoragePolicy::estimate(
        expectedFrames, {3840, 2160}, 120.0, 12'000'000);
    QCOMPARE(fallback.basis, ExportStorageEstimate::Basis::ConservativeFallback);
    QCOMPARE(fallback.bytesPerFrame, 512LL * 1024LL);
    QCOMPARE(fallback.safetyMargin, 1.75);
    QVERIFY(fallback.temporaryOverlayBytes > measured.temporaryOverlayBytes);
    QVERIFY(fallback.temporaryOverlayBytes < 10LL * 1024 * 1024 * 1024);

    const ExportStorageEstimate overflow = ExportStoragePolicy::estimateFromSample(
        std::numeric_limits<qint64>::max(), 1, std::numeric_limits<qsizetype>::max(),
        1.0, 12'000'000);
    QCOMPARE(overflow.temporaryOverlayBytes, std::numeric_limits<qint64>::max());
}

void TelemetryTests::streamsRawFramesToSlowConsumer()
{
    QProcess consumer;
    consumer.start(QStringLiteral(RAW_TRANSPORT_CONSUMER_PATH), {QStringLiteral("slow")});
    QVERIFY2(consumer.waitForStarted(5'000), qPrintable(consumer.errorString()));
    const QByteArray fullHdFrame(1920 * 1080 * 4, 'x');
    const QByteArray ultraHdFrame(3840 * 2160 * 4, 'y');
    RawFrameTransport transport(consumer);
    const RawFrameTransportResult fullHdResult = transport.writeFrame(fullHdFrame);
    QVERIFY2(fullHdResult.succeeded(), qPrintable(fullHdResult.error));
    const RawFrameTransportResult ultraHdResult = transport.writeFrame(ultraHdFrame);
    QVERIFY2(ultraHdResult.succeeded(), qPrintable(ultraHdResult.error));
    consumer.closeWriteChannel();
    QVERIFY2(consumer.waitForFinished(15'000), qPrintable(consumer.errorString()));
    QCOMPARE(consumer.exitStatus(), QProcess::NormalExit);
    QCOMPARE(consumer.exitCode(), 0);
    QCOMPARE(consumer.readAllStandardOutput().trimmed().toLongLong(),
             qint64(fullHdFrame.size() + ultraHdFrame.size()));
    const RawFrameTransportConfig config;
    QVERIFY(ultraHdResult.maximumQueuedBytes <= config.highWaterBytes + config.writeChunkBytes);
}

void TelemetryTests::failsRawFrameTransportWhenConsumerExits()
{
    QProcess consumer;
    consumer.start(QStringLiteral(RAW_TRANSPORT_CONSUMER_PATH), {QStringLiteral("early-exit")});
    QVERIFY2(consumer.waitForStarted(5'000), qPrintable(consumer.errorString()));
    const auto stopConsumer = qScopeGuard([&] {
        if (consumer.state() != QProcess::NotRunning) {
            consumer.kill();
            static_cast<void>(consumer.waitForFinished(5'000));
        }
    });
    const QByteArray frame(32 * 1024 * 1024, 'x');
    RawFrameTransport transport(consumer);
    const RawFrameTransportResult result = transport.writeFrame(frame);
    QVERIFY(!result.succeeded());
    QCOMPARE(result.status, RawFrameTransportResult::Status::Failure);
    QVERIFY2(result.error.contains(QStringLiteral("exited"), Qt::CaseInsensitive)
                 || result.error.contains(QStringLiteral("write failed"), Qt::CaseInsensitive)
                 || result.error.contains(QStringLiteral("device error"), Qt::CaseInsensitive),
             qPrintable(result.error));
}

void TelemetryTests::timesOutStalledRawFrameTransport()
{
    QProcess consumer;
    consumer.start(QStringLiteral(RAW_TRANSPORT_CONSUMER_PATH), {QStringLiteral("stall")});
    QVERIFY2(consumer.waitForStarted(5'000), qPrintable(consumer.errorString()));
    RawFrameTransportConfig config;
    config.stallTimeoutMilliseconds = 700;
    const QByteArray frame(32 * 1024 * 1024, 'x');
    RawFrameTransport transport(consumer, config);
    QElapsedTimer elapsed;
    elapsed.start();
    const RawFrameTransportResult result = transport.writeFrame(frame);
    QVERIFY(!result.succeeded());
    QVERIFY2(result.error.contains(QStringLiteral("stalled")), qPrintable(result.error));
    QVERIFY(elapsed.elapsed() >= config.stallTimeoutMilliseconds);
    QVERIFY(elapsed.elapsed() < 5'000);
    consumer.kill();
    QVERIFY(consumer.waitForFinished(5'000));
}

void TelemetryTests::cancelsBlockedRawFrameTransportPromptly()
{
    QProcess consumer;
    consumer.start(QStringLiteral(RAW_TRANSPORT_CONSUMER_PATH), {QStringLiteral("stall")});
    QVERIFY2(consumer.waitForStarted(5'000), qPrintable(consumer.errorString()));
    std::atomic_bool cancelled = false;
    RawFrameTransport transport(consumer, {}, [&cancelled] { return cancelled.load(); });
    std::thread canceller([&cancelled] {
        QThread::msleep(300);
        cancelled.store(true);
    });
    const auto joinCanceller = qScopeGuard([&] { canceller.join(); });
    const QByteArray frame(32 * 1024 * 1024, 'x');
    QElapsedTimer elapsed;
    elapsed.start();
    const RawFrameTransportResult result = transport.writeFrame(frame);
    QCOMPARE(result.status, RawFrameTransportResult::Status::Cancelled);
    QVERIFY(elapsed.elapsed() < 2'000);
    consumer.kill();
    QVERIFY(consumer.waitForFinished(5'000));
}

void TelemetryTests::cleansOnlyManifestOwnedArtifacts()
{
    QTemporaryDir destination;
    QVERIFY(destination.isValid());
    const QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QString overlay = QDir::temp().filePath(QStringLiteral("flappedear-overlay-%1.mkv").arg(id));
    const QString staging = destination.filePath(QStringLiteral(".result.flappedear-%1.part.mp4").arg(id));
    const QString target = destination.filePath("result.mp4");
    const QString unrelated = destination.filePath("unrelated.mkv");
    QVERIFY(writeBytes(overlay, "overlay"));
    QVERIFY(writeBytes(staging, "staging"));
    QVERIFY(writeBytes(unrelated, "keep"));
    const ExportArtifactManifestData manifest{id, QDateTime::currentMSecsSinceEpoch(), overlay, staging, target, 0, "stageA"};
    QString error;
    QVERIFY2(ExportArtifactManifest::create(manifest, &error), qPrintable(error));
    QVERIFY2(ExportArtifactManifest::cleanupOwned(ExportArtifactManifest::manifestPathFor(id), &error), qPrintable(error));
    QVERIFY(!QFileInfo::exists(overlay));
    QVERIFY(!QFileInfo::exists(staging));
    QVERIFY(QFileInfo::exists(unrelated));

    const QString malformed = QDir::temp().filePath(QStringLiteral("flappedear-export-%1.manifest.json")
        .arg(QUuid::createUuid().toString(QUuid::WithoutBraces)));
    QVERIFY(writeBytes(malformed, "not json"));
    QVERIFY(!ExportArtifactManifest::cleanupOwned(malformed, &error));
    QVERIFY(QFile::remove(malformed));
}

void TelemetryTests::preservesLiveManifestForStartupRecovery()
{
    QTemporaryDir destination;
    QVERIFY(destination.isValid());
    const QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QString overlay = QDir::temp().filePath(QStringLiteral("flappedear-overlay-%1.mkv").arg(id));
    const QString staging = destination.filePath(QStringLiteral(".result.flappedear-%1.part.mp4").arg(id));
    QVERIFY(writeBytes(overlay, "overlay"));
    QVERIFY(writeBytes(staging, "staging"));
    const ExportArtifactManifestData manifest{id, QDateTime::currentMSecsSinceEpoch(), overlay, staging,
        destination.filePath("result.mp4"), QCoreApplication::applicationPid(), "stageB"};
    QString error;
    QVERIFY2(ExportArtifactManifest::create(manifest, &error), qPrintable(error));
    const QString manifestPath = ExportArtifactManifest::manifestPathFor(id);
    const QStringList recovered = ExportArtifactManifest::recoverStale();
    QVERIFY(!recovered.contains(manifestPath));
    QVERIFY(QFileInfo::exists(overlay));
    QVERIFY2(ExportArtifactManifest::cleanupOwned(manifestPath, &error), qPrintable(error));
}

void TelemetryTests::supervisesUnixExportProcessTree()
{
#ifdef Q_OS_UNIX
    QProcess process;
    process.setProcessChannelMode(QProcess::SeparateChannels);
    ExportProcessSupervisor supervisor(process);
    supervisor.start(QStringLiteral("/bin/sh"), {QStringLiteral("-c"), QStringLiteral("sleep 30 & echo $!; wait")});
    QVERIFY2(supervisor.waitForStarted(), qPrintable(process.errorString()));
    QVERIFY(supervisor.supervisionActive());
    QVERIFY(process.waitForReadyRead(2'000));
    bool ok = false;
    const qint64 grandchildPid = QString::fromUtf8(process.readAllStandardOutput()).trimmed().toLongLong(&ok);
    QVERIFY(ok && grandchildPid > 0);
    QVERIFY(supervisor.stopAndWait(500, 2'000));
    QTRY_VERIFY(!ExportArtifactManifest::processIsActive(grandchildPid));
#else
    QSKIP("Unix process-group behavior is runtime-tested on this platform only.");
#endif
}

void TelemetryTests::retainsTelemetryAfterFailedAsyncLoad()
{
    QSettings settings;
    settings.clear();
    settings.sync();
    AppController controller;
    controller.loadVbo(QUrl::fromLocalFile(QStringLiteral(TEST_FIXTURE_PATH)));
    QTRY_COMPARE(controller.vboLoadState(), QStringLiteral("ready"));
    const QString loadedName = controller.telemetryName();
    const QStringList loadedChannels = controller.channelNames();
    QVERIFY(!loadedName.isEmpty());

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString malformedPath = directory.filePath("malformed.vbo");
    QVERIFY(writeBytes(malformedPath, "not a VBOX file"));
    controller.loadVbo(QUrl::fromLocalFile(malformedPath));
    QTRY_COMPARE(controller.vboLoadState(), QStringLiteral("error"));
    QCOMPARE(controller.telemetryName(), loadedName);
    QCOMPARE(controller.channelNames(), loadedChannels);
}

void TelemetryTests::replacesInFlightSourceLoad()
{
    QSettings settings;
    settings.clear();
    settings.sync();
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QString large = QStringLiteral("[column names]\ntime speed\n[data]\n");
    large.reserve(4'000'000);
    for (int row = 0; row < 250'000; ++row) large += QStringLiteral("%1 %2\n").arg(row).arg(row % 200);
    const QString sourceA = directory.filePath(QStringLiteral("source-a.vbo"));
    QVERIFY(writeBytes(sourceA, large.toUtf8()));

    AppController controller;
    controller.loadVbo(QUrl::fromLocalFile(sourceA));
    controller.loadVbo(QUrl::fromLocalFile(QStringLiteral(TEST_FIXTURE_PATH)));
    QTRY_COMPARE_WITH_TIMEOUT(controller.vboLoadState(), QStringLiteral("ready"), 10'000);
    QCOMPARE(controller.telemetryName(), QStringLiteral("basic.vbo"));
    QCOMPARE(controller.sampleCount(), 3);
}

void TelemetryTests::shutsDownWithInFlightSourceLoad()
{
    QSettings settings;
    settings.clear();
    settings.sync();
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QString large = QStringLiteral("[column names]\ntime speed\n[data]\n");
    large.reserve(4'000'000);
    for (int row = 0; row < 250'000; ++row) large += QStringLiteral("%1 %2\n").arg(row).arg(row % 200);
    const QString source = directory.filePath(QStringLiteral("shutdown.vbo"));
    QVERIFY(writeBytes(source, large.toUtf8()));
    QElapsedTimer elapsed;
    elapsed.start();
    {
        AppController controller;
        controller.loadVbo(QUrl::fromLocalFile(source));
    }
    QVERIFY2(elapsed.elapsed() < 3'000, qPrintable(QString::number(elapsed.elapsed())));
}

void TelemetryTests::opensProjectsTransactionally()
{
    QSettings settings;
    settings.clear();
    settings.sync();
    AppController controller;
    controller.loadVbo(QUrl::fromLocalFile(QStringLiteral(TEST_FIXTURE_PATH)));
    QTRY_COMPARE(controller.vboLoadState(), QStringLiteral("ready"));
    const QString oldTelemetryName = controller.telemetryName();
    const QStringList oldChannels = controller.channelNames();
    const QStringList oldAnalysisChannels = controller.analysisChannels();
    const QJsonArray oldWidgets = controller.widgetModel()->toJson();
    const double oldOffset = controller.syncOffset();
    const double oldScale = controller.timeScale();
    QVERIFY(controller.dirty());

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QJsonObject scene{{"widgets", controller.widgetModel()->toJson()}};
    const QJsonObject failedProject{{"version", 2},
                                    {"scene", scene},
                                    {"vboPath", directory.filePath("missing.vbo")},
                                    {"sync", QJsonObject{{"offset", 4.0}, {"timeScale", 1.0}}}};
    const QString failedPath = directory.filePath("missing-source.fetproject");
    QVERIFY(writeBytes(failedPath, QJsonDocument(failedProject).toJson()));
    controller.requestOpenProject(QUrl::fromLocalFile(failedPath));
    controller.resolveDestructiveAction("discard");
    QTRY_VERIFY(!controller.projectLoading());
    QVERIFY(!controller.projectLoadError().isEmpty());
    QCOMPARE(controller.telemetryName(), oldTelemetryName);
    QCOMPARE(controller.channelNames(), oldChannels);
    QCOMPARE(controller.analysisChannels(), oldAnalysisChannels);
    QCOMPARE(controller.widgetModel()->toJson(), oldWidgets);
    QCOMPARE(controller.syncOffset(), oldOffset);
    QCOMPARE(controller.timeScale(), oldScale);
    QVERIFY(controller.projectPath().isEmpty());
    QVERIFY(controller.dirty());

    const QJsonObject successProject{{"version", 2},
                                     {"scene", scene},
                                     {"vboPath", QStringLiteral(TEST_FIXTURE_PATH)},
                                     {"sync", QJsonObject{{"offset", 2.5}, {"timeScale", 1.0}}},
                                     {"analysis", QJsonObject{{"channels", QJsonArray{}}, {"visible", true}}}};
    const QString successPath = directory.filePath("valid.fetproject");
    QVERIFY(writeBytes(successPath, QJsonDocument(successProject).toJson()));
    controller.requestOpenProject(QUrl::fromLocalFile(successPath));
    controller.resolveDestructiveAction("discard");
    QTRY_VERIFY(!controller.projectLoading());
    QCOMPARE(controller.projectPath().toLocalFile(), QFileInfo(successPath).canonicalFilePath());
    QCOMPARE(controller.telemetryName(), QStringLiteral("basic.vbo"));
    QCOMPARE(controller.syncOffset(), 2.5);
    QVERIFY(!controller.dirty());
}

void TelemetryTests::restoresSavedProjectsAndPreservesUnknownFields()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettings settings;
    settings.clear();
    const QString projectPath = directory.filePath(QStringLiteral("authoritative.fetproject"));
    const QString recoveryPath = directory.filePath(QStringLiteral("recovery.json"));
    const QJsonObject future{{QStringLiteral("something"), 123}};
    QJsonObject project = testProject(1.25, {{QStringLiteral("futureField"), future}});
    QJsonObject scene = project.value(QStringLiteral("scene")).toObject();
    scene.insert(QStringLiteral("futureSceneField"), QStringLiteral("preserve me"));
    project.insert(QStringLiteral("scene"), scene);
    QVERIFY(writeBytes(projectPath, QJsonDocument(project).toJson()));
    settings.setValue(QStringLiteral("project/path"), projectPath);
    settings.setValue(QStringLiteral("sync/offset"), 99.0);
    settings.setValue(QStringLiteral("editor/widgets"), QByteArray("legacy"));
    settings.setValue(QStringLiteral("analysis/windowWidth"), 777);
    settings.sync();

    {
        AppController controller(nullptr, recoveryPath);
        QTRY_VERIFY(!controller.projectLoading());
        QCOMPARE(controller.syncOffset(), 1.25);
        QVERIFY(!controller.dirty());
        QCOMPARE(controller.analysisWindowWidth(), 777);
        controller.setSyncOffset(2.5);
        QVERIFY(controller.saveCurrentProject());
        QVERIFY(!controller.dirty());
    }

    QFile saved(projectPath);
    QVERIFY(saved.open(QIODevice::ReadOnly));
    const QJsonObject reloaded = QJsonDocument::fromJson(saved.readAll()).object();
    QCOMPARE(reloaded.value(QStringLiteral("futureField")).toObject(), future);
    QCOMPARE(reloaded.value(QStringLiteral("scene")).toObject()
                 .value(QStringLiteral("futureSceneField")).toString(),
             QStringLiteral("preserve me"));
    QCOMPARE(reloaded.value(QStringLiteral("sync")).toObject()
                 .value(QStringLiteral("offset")).toDouble(), 2.5);
    QVERIFY(!settings.contains(QStringLiteral("sync/offset")));
    QVERIFY(!settings.contains(QStringLiteral("editor/widgets")));
}

void TelemetryTests::recoversAndDiscardsSavedChanges()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettings settings;
    settings.clear();
    const QString projectPath = directory.filePath(QStringLiteral("saved.fetproject"));
    const QString recoveryPath = directory.filePath(QStringLiteral("recovery.json"));
    QVERIFY(writeBytes(projectPath, QJsonDocument(testProject(1.0)).toJson()));
    settings.setValue(QStringLiteral("project/path"), projectPath);
    settings.sync();

    {
        AppController controller(nullptr, recoveryPath);
        QTRY_VERIFY(!controller.projectLoading());
        controller.setSyncOffset(7.0);
        QTRY_VERIFY(QFileInfo(recoveryPath).isFile());
    }
    {
        AppController controller(nullptr, recoveryPath);
        QVERIFY(controller.recoveryPending());
        controller.resolveStartupRecovery(QStringLiteral("recover"));
        QTRY_VERIFY(!controller.projectLoading());
        QCOMPARE(controller.syncOffset(), 7.0);
        QCOMPARE(controller.projectPath().toLocalFile(), QFileInfo(projectPath).canonicalFilePath());
        QVERIFY(controller.dirty());
    }
    {
        AppController controller(nullptr, recoveryPath);
        QVERIFY(controller.recoveryPending());
        controller.resolveStartupRecovery(QStringLiteral("discard"));
        QTRY_VERIFY(!controller.projectLoading());
        QCOMPARE(controller.syncOffset(), 1.0);
        QVERIFY(!controller.dirty());
        QVERIFY(!QFileInfo(recoveryPath).exists());
    }
}

void TelemetryTests::recoversAndDiscardsUnsavedDocuments()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettings settings;
    settings.clear();
    settings.sync();
    const QString recoveryPath = directory.filePath(QStringLiteral("recovery.json"));
    int recoveredWidgetCount = 0;
    {
        AppController controller(nullptr, recoveryPath);
        controller.widgetModel()->addWidget(QStringLiteral("customValue"));
        controller.setSyncOffset(4.0);
        recoveredWidgetCount = controller.widgetModel()->count();
        QTRY_VERIFY(QFileInfo(recoveryPath).isFile());
    }
    {
        AppController controller(nullptr, recoveryPath);
        QVERIFY(controller.recoveryPending());
        controller.resolveStartupRecovery(QStringLiteral("recover"));
        QTRY_VERIFY(!controller.projectLoading());
        QCOMPARE(controller.widgetModel()->count(), recoveredWidgetCount);
        QCOMPARE(controller.syncOffset(), 4.0);
        QVERIFY(controller.projectPath().isEmpty());
        QVERIFY(controller.dirty());
    }
    {
        AppController controller(nullptr, recoveryPath);
        QVERIFY(controller.recoveryPending());
        controller.resolveStartupRecovery(QStringLiteral("discard"));
        QVERIFY(!controller.projectLoading());
        QCOMPARE(controller.syncOffset(), 0.0);
        QVERIFY(controller.projectPath().isEmpty());
        QVERIFY(!controller.dirty());
        QVERIFY(!QFileInfo(recoveryPath).exists());
    }
}

void TelemetryTests::discardsUnsavedStateForQuitNewAndOpen()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettings settings;
    settings.clear();
    const QString projectA = directory.filePath(QStringLiteral("a.fetproject"));
    const QString projectB = directory.filePath(QStringLiteral("b.fetproject"));
    const QString recoveryPath = directory.filePath(QStringLiteral("recovery.json"));
    QVERIFY(writeBytes(projectA, QJsonDocument(testProject(1.0)).toJson()));
    QVERIFY(writeBytes(projectB, QJsonDocument(testProject(3.0)).toJson()));
    settings.setValue(QStringLiteral("project/path"), projectA);
    settings.sync();

    {
        AppController controller(nullptr, recoveryPath);
        QTRY_VERIFY(!controller.projectLoading());
        controller.widgetModel()->addWidget(QStringLiteral("customValue"));
        controller.setSyncOffset(8.0);
        controller.setAnalysisVisible(false);
        controller.loadVbo(QUrl::fromLocalFile(QStringLiteral(TEST_FIXTURE_PATH)));
        QTRY_COMPARE(controller.vboLoadState(), QStringLiteral("ready"));
        QTRY_VERIFY(QFileInfo(recoveryPath).isFile());
        QSignalSpy quitSpy(&controller, &AppController::quitApproved);
        controller.requestQuit();
        controller.resolveDestructiveAction(QStringLiteral("discard"));
        QCOMPARE(quitSpy.count(), 1);
        QVERIFY(!QFileInfo(recoveryPath).exists());
    }
    {
        AppController controller(nullptr, recoveryPath);
        QTRY_VERIFY(!controller.projectLoading());
        QCOMPARE(controller.syncOffset(), 1.0);
        QVERIFY(controller.telemetryName().isEmpty());
        QVERIFY(controller.analysisVisible());
        QVERIFY(!controller.dirty());

        controller.setSyncOffset(8.0);
        QTRY_VERIFY(QFileInfo(recoveryPath).isFile());
        controller.requestNewProject();
        controller.resolveDestructiveAction(QStringLiteral("discard"));
        QCOMPARE(controller.syncOffset(), 0.0);
        QVERIFY(controller.projectPath().isEmpty());
        QVERIFY(!controller.dirty());
        QVERIFY(!QFileInfo(recoveryPath).exists());

        controller.setSyncOffset(9.0);
        QTRY_VERIFY(QFileInfo(recoveryPath).isFile());
        controller.requestOpenProject(QUrl::fromLocalFile(projectB));
        controller.resolveDestructiveAction(QStringLiteral("discard"));
        QTRY_VERIFY(!controller.projectLoading());
        QCOMPARE(controller.syncOffset(), 3.0);
        QCOMPARE(controller.projectPath().toLocalFile(), QFileInfo(projectB).canonicalFilePath());
        QVERIFY(!controller.dirty());
        QVERIFY(!QFileInfo(recoveryPath).exists());
    }
}

void TelemetryTests::preservesRecoveryAcrossFailedSave()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettings settings;
    settings.clear();
    settings.sync();
    const QString projectPath = directory.filePath(QStringLiteral("saved.fetproject"));
    const QString recoveryPath = directory.filePath(QStringLiteral("recovery.json"));
    const QByteArray original = QJsonDocument(testProject(1.0)).toJson();
    QVERIFY(writeBytes(projectPath, original));

    AppController controller(nullptr, recoveryPath);
    controller.requestOpenProject(QUrl::fromLocalFile(projectPath));
    QTRY_VERIFY(!controller.projectLoading());
    controller.setSyncOffset(8.0);
    QTRY_VERIFY(QFileInfo(recoveryPath).isFile());
    QVERIFY(!controller.saveProject(
        QUrl::fromLocalFile(directory.filePath(QStringLiteral("missing/project.fetproject")))));
    QVERIFY(controller.dirty());
    QVERIFY(QFileInfo(recoveryPath).isFile());
    QFile unchanged(projectPath);
    QVERIFY(unchanged.open(QIODevice::ReadOnly));
    QCOMPARE(unchanged.readAll(), original);
    QVERIFY(controller.saveCurrentProject());
    QVERIFY(!controller.dirty());
    QVERIFY(!QFileInfo(recoveryPath).exists());
}

void TelemetryTests::rejectsProjectLoadAfterInterveningEdit()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettings settings;
    settings.clear();
    settings.sync();
    const QString projectPath = directory.filePath(QStringLiteral("delayed.fetproject"));
    const QString recoveryPath = directory.filePath(QStringLiteral("recovery.json"));
    QVERIFY(writeBytes(projectPath, QJsonDocument(testProject(2.0)).toJson()));

    AppController controller(nullptr, recoveryPath);
    controller.requestOpenProject(QUrl::fromLocalFile(projectPath));
    QVERIFY(controller.projectLoading());
    controller.setSyncOffset(9.0);
    QTRY_VERIFY(!controller.projectLoading());
    QCOMPARE(controller.syncOffset(), 9.0);
    QVERIFY(controller.projectPath().isEmpty());
    QVERIFY(controller.dirty());
    QVERIFY(controller.projectLoadError().contains(QStringLiteral("document changed")));
}

void TelemetryTests::tracksExportStageElapsedTime()
{
    ExportStageTimer timer;
    timer.start(100, QStringLiteral("preparing"));
    QCOMPARE(timer.totalElapsedMilliseconds(250), 150);
    QCOMPARE(timer.stageElapsedMilliseconds(250), 150);
    timer.transition(300, QStringLiteral("renderingOverlay"));
    QCOMPARE(timer.totalElapsedMilliseconds(350), 250);
    QCOMPARE(timer.stageElapsedMilliseconds(350), 50);
    timer.transition(375, QStringLiteral("renderingOverlay"));
    QCOMPARE(timer.stageElapsedMilliseconds(400), 100);
    QCOMPARE(timer.completedStageDurations().value(QStringLiteral("preparing")), 200);
}

void TelemetryTests::boundsVerboseDiagnosticStorage()
{
    BoundedDiagnosticLog log(4);
    log.append(QStringLiteral("one"));
    log.append(QStringLiteral("two"));
    log.append(QStringLiteral("three"));
    log.append(QStringLiteral("four"));
    log.append(QStringLiteral("five\nwith details"));
    QCOMPARE(log.size(), 4);
    QCOMPARE(log.maximumEntries(), 4);
    QVERIFY(log.text().startsWith(QStringLiteral("[older diagnostic entries omitted]\n")));
    QVERIFY(!log.text().contains(QStringLiteral("one")));
    QVERIFY(log.text().endsWith(QStringLiteral("five\nwith details")));
}

void TelemetryTests::persistsExportDiagnosticsAndRetainsKnownLogs()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QDateTime started(QDate(2026, 8, 22), QTime(21, 25, 30), QTimeZone::UTC);
    QCOMPARE(PersistentExportLog::fileName(started, QStringLiteral("a83f91c2d4e5f678")),
             QStringLiteral("export-20260822-212530-a83f91c2d4e5f678.log"));

    QString error;
    auto log = PersistentExportLog::create(
        directory.path(), QStringLiteral("a83f91c2d4e5f678"),
        QStringLiteral("FlappedEar Telemetry Export Log\nExport ID: a83f91c2d4e5f678"), &error, started);
    QVERIFY2(log, qPrintable(error));
    const QString activePath = log->path();
    QVERIFY(log->append(QStringLiteral("[lifecycle] Preparing")));
    QVERIFY(log->append(QStringLiteral("[00:00:01.000] Stage A started")));
    QVERIFY(log->append(QStringLiteral("Result: SUCCESS")));
    QFile saved(activePath);
    QVERIFY(saved.open(QIODevice::ReadOnly));
    const QString contents = QString::fromUtf8(saved.readAll());
    QVERIFY(contents.contains(QStringLiteral("Export ID: a83f91c2d4e5f678")));
    QVERIFY(contents.contains(QStringLiteral("Stage A started")));
    QVERIFY(contents.contains(QStringLiteral("Result: SUCCESS")));

    QFile unrelated(directory.filePath(QStringLiteral("keep-me.log")));
    QVERIFY(unrelated.open(QIODevice::WriteOnly));
    unrelated.close();
    for (int index = 0; index < 12; ++index) {
        const QDateTime time = started.addSecs(index + 1);
        const QString id = QStringLiteral("%1abcdef012345678").arg(index, 8, 16, QLatin1Char('0'));
        auto oldLog = PersistentExportLog::create(directory.path(), id, QStringLiteral("Result: CANCELLED"),
                                                   &error, time);
        QVERIFY2(oldLog, qPrintable(error));
    }
    PersistentExportLog::retainNewest(directory.path(), activePath, 10);
    QVERIFY(QFileInfo::exists(activePath));
    QVERIFY(QFileInfo::exists(unrelated.fileName()));
    const QStringList remaining = QDir(directory.path()).entryList({QStringLiteral("export-*.log")}, QDir::Files);
    QCOMPARE(remaining.size(), 11); // ten most recent plus the active log
}

void TelemetryTests::formatsStageAFailureDiagnostics()
{
    const StageAFailureDiagnostics diagnostics{
        QStringLiteral("FFmpeg stopped before the next overlay frame was rendered"),
        819,
        8992,
        1,
        QStringLiteral("NormalExit"),
        QStringLiteral("WriteError"),
        QStringLiteral("Broken pipe"),
        817,
        13'630'000,
        59.94,
        0.21,
        31'457'280,
        94'371'840,
        QStringLiteral("/tmp/flappedear-overlay-test.mkv"),
        59'391'756,
        QStringLiteral("No space left on device"),
        QStringLiteral("/"),
        123'456,
        987'654,
        QStringLiteral("/Volumes/Exports"),
        456'789,
        987'654,
        QStringLiteral("/tmp/flappedear-export.cancel"),
        false};
    const QString formatted = formatStageAFailureDiagnostics(diagnostics);
    QVERIFY(formatted.contains(QStringLiteral("Stage A exited unexpectedly")));
    QVERIFY(formatted.contains(QStringLiteral("Submitted frames: 819 / 8992")));
    QVERIFY(formatted.contains(QStringLiteral("FFmpeg exit: code=1 status=NormalExit")));
    QVERIFY(formatted.contains(QStringLiteral("QProcess error: WriteError (Broken pipe)")));
    QVERIFY(formatted.contains(QStringLiteral("Temporary overlay before cleanup: /tmp/flappedear-overlay-test.mkv (59391756 bytes)")));
    QVERIFY(formatted.contains(QStringLiteral("Cancellation marker: /tmp/flappedear-export.cancel exists=no")));
    QVERIFY(formatted.contains(QStringLiteral("FFmpeg stderr tail:\nNo space left on device")));

    const QVariantMap details = stageAFailureDiagnosticDetails(diagnostics);
    QCOMPARE(details.value(QStringLiteral("submittedFrames")).toLongLong(), 819);
    QCOMPARE(details.value(QStringLiteral("temporaryOverlayBytes")).toLongLong(), 59'391'756);
    QCOMPARE(details.value(QStringLiteral("cancellationFileExists")).toBool(), false);
    QCOMPARE(details.value(QStringLiteral("stderrTail")).toString(), QStringLiteral("No space left on device"));

    StageAFailureDiagnostics crash = diagnostics;
    crash.exitCode = 9;
    crash.exitStatus = QStringLiteral("CrashExit (Unix signal unavailable from QProcess)");
    crash.processError = QStringLiteral("Crashed");
    crash.processErrorString = QStringLiteral("Process crashed");
    crash.cancellationFileExists = true;
    crash.stderrTail.clear();
    const QString crashFormatted = formatStageAFailureDiagnostics(crash);
    QVERIFY(crashFormatted.contains(QStringLiteral("status=CrashExit (Unix signal unavailable from QProcess)")));
    QVERIFY(crashFormatted.contains(QStringLiteral("QProcess error: Crashed (Process crashed)")));
    QVERIFY(crashFormatted.contains(QStringLiteral("exists=yes")));
}

void TelemetryTests::throttlesDiagnosticHeartbeats()
{
    DiagnosticHeartbeat heartbeat(500);
    QVERIFY(!heartbeat.shouldEmit(0));
    QVERIFY(!heartbeat.shouldEmit(499));
    QVERIFY(heartbeat.shouldEmit(500));
    QVERIFY(!heartbeat.shouldEmit(999));
    QVERIFY(heartbeat.shouldEmit(1'000));
}

void TelemetryTests::tracksValidationSubstepStages()
{
    ExportStageTimer timer;
    timer.start(0, QStringLiteral("preparing"));
    timer.transition(10, QStringLiteral("renderingOverlay"));
    timer.transition(20, QStringLiteral("validatingOverlay"));
    QCOMPARE(timer.stage(), QStringLiteral("validatingOverlay"));
    QCOMPARE(timer.stageElapsedMilliseconds(25), 5);
    timer.transition(30, QStringLiteral("encodingVideo"));
    timer.transition(40, QStringLiteral("validatingOutput"));
    timer.transition(50, QStringLiteral("cleaningUp"));
    timer.transition(60, QStringLiteral("complete"));
    const auto durations = timer.completedStageDurations();
    for (const QString &stage : {QStringLiteral("preparing"), QStringLiteral("renderingOverlay"),
                                 QStringLiteral("validatingOverlay"), QStringLiteral("encodingVideo"),
                                 QStringLiteral("validatingOutput"), QStringLiteral("cleaningUp")}) {
        QCOMPARE(durations.value(stage), 10);
    }
}

void TelemetryTests::detectsHevcEncoders()
{
    const QString output = " V....D hevc_videotoolbox Apple VideoToolbox\n V....D libx265 x265\n";
    const QList<EncoderCapability> encoders = EncoderDetector::parseEncoders(output);
    QCOMPARE(encoders.size(), 2);
    QCOMPARE(encoders[0].id, QString("hevc_videotoolbox"));
    QVERIFY(encoders[0].hardware);
    QCOMPARE(EncoderDetector::preferredHevcEncoder(encoders), QString("hevc_videotoolbox"));
}

void TelemetryTests::cancelsEncoderDiscovery()
{
#ifdef Q_OS_UNIX
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString helper = directory.filePath("slow-ffmpeg.sh");
    QFile script(helper);
    QVERIFY(script.open(QIODevice::WriteOnly | QIODevice::Truncate));
    QCOMPARE(script.write("#!/bin/sh\nsleep 10\n"), qint64(19));
    script.close();
    QVERIFY(script.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner));
    QString error;
    try {
        static_cast<void>(EncoderDetector::discover(helper, [] { return true; }));
    } catch (const std::exception &exception) {
        error = QString::fromUtf8(exception.what());
    }
    QCOMPARE(error, QStringLiteral("Encoder discovery cancelled."));
#else
    QSKIP("Cancellable helper script requires a POSIX shell.");
#endif
}

void TelemetryTests::calculatesTimestampDrivenExportFrames()
{
    QCOMPARE(ExportEngine::frameCount(120.0, 140.0, {30'000, 1001}), qsizetype(600));
    QCOMPARE(ExportEngine::frameCount(0.0, 1.0, {60'000, 1001}), qsizetype(60));
    QCOMPARE(ExportEngine::frameCount(1.0, 1.0, {30, 1}), qsizetype(0));
    const MediaRational ntscRate{60'000, 1001};
    QCOMPARE(ExportEngine::frameCount(30.0, 150.0, ntscRate), qsizetype(7'193));
    QCOMPARE(ExportEngine::sourceVideoTime(30.0, 0, ntscRate), 30.0);
    QVERIFY(qAbs(ExportEngine::sourceVideoTime(30.0, 1, ntscRate) - (30.0 + 1001.0 / 60'000.0)) < 0.000001);
    QVERIFY(ExportEngine::sourceVideoTime(30.0, 7'192, ntscRate) < 150.0);
    QVERIFY(ExportEngine::sourceVideoTime(30.0, 7'192, ntscRate) > 149.9);
    QCOMPARE(ExportEngine::exportRelativeTime(0, ntscRate), 0.0);
}

void TelemetryTests::resolvesExplicitExportFormats()
{
    const QList<QSize> sizes = ExportFormat::resolutionOptions({3840, 2160});
    QCOMPARE(sizes.first(), QSize(3840, 2160));
    QVERIFY(sizes.contains(QSize(1920, 1080)));
    for (const QSize &size : sizes) {
        QVERIFY(size.width() <= 3840 && size.height() <= 2160);
        QCOMPARE(size.width() % 2, 0); QCOMPARE(size.height() % 2, 0);
    }
    const MediaRational ntsc{60'000, 1'001};
    const QList<MediaRational> rates = ExportFormat::frameRateOptions(ntsc);
    QCOMPARE(rates.size(), 2); QCOMPARE(rates.at(1).numerator, qint64(30'000));
    QCOMPARE(rates.at(1).denominator, qint64(1'001));
    const qint64 recommended = ExportFormat::recommendedVideoBitrate({1920, 1080}, {30, 1});
    QVERIFY(recommended >= 10'000'000 && recommended <= 14'000'000);
    QVERIFY(ExportFormat::recommendedVideoBitrate({1920, 1080}, {60'000, 1'001}) >= 15'000'000);
    QVERIFY(ExportFormat::recommendedVideoBitrate({1920, 1080}, {60'000, 1'001}) <= 20'000'000);
    QVERIFY(ExportFormat::recommendedVideoBitrate({2560, 1440}, {60, 1}) >= 26'000'000);
    QVERIFY(ExportFormat::recommendedVideoBitrate({2560, 1440}, {60, 1}) <= 34'000'000);
    QVERIFY(ExportFormat::recommendedVideoBitrate({3840, 2160}, {30, 1}) >= 30'000'000);
    QVERIFY(ExportFormat::recommendedVideoBitrate({3840, 2160}, {30, 1}) <= 40'000'000);
    const qint64 fourK60 = ExportFormat::recommendedVideoBitrate({3840, 2160}, {60'000, 1'001});
    QVERIFY(fourK60 >= 45'000'000 && fourK60 <= 60'000'000);
    QVERIFY(fourK60 < 120'000'000);
    QCOMPARE(ExportFormat::bitrateForQuality("smaller", {1920, 1080}, {30, 1}), qRound64(recommended * .7));
    const qint64 high = ExportFormat::bitrateForQuality("high", {3840, 2160}, {60'000, 1'001});
    QVERIFY(ExportFormat::bitrateForQuality("smaller", {3840, 2160}, {60'000, 1'001}) < fourK60);
    QVERIFY(fourK60 < high); QVERIFY(ExportFormat::validCustomBitrate(high));
    QVERIFY(!ExportFormat::validCustomBitrate(0)); QVERIFY(!ExportFormat::validCustomBitrate(121'000'000));
    QVERIFY(ExportFormat::validCustomBitrate(10'000'000));
    QVERIFY(ExportFormat::estimatedBytes(10'000'000, true, 60) > 75'000'000);
    QCOMPARE(ExportFormat::formatEstimatedSize(qint64(850) * 1024 * 1024), QStringLiteral("~850 MiB"));
    QCOMPARE(ExportFormat::formatEstimatedSize(qint64(46) * 1024 * 1024 * 1024 / 10), QStringLiteral("~4.60 GiB"));
    QCOMPARE(ExportEngine::frameCount(0, 10, rates.at(1)), qsizetype(300));
}

void TelemetryTests::preservesExactExportRateRationals()
{
    QVERIFY((MediaRational{24'000, 1'001}.isEquivalentTo({24'000, 1'001})));
    QVERIFY((MediaRational{30'000, 1'001}.isEquivalentTo({60'000, 2'002})));
    QVERIFY((MediaRational{60'000, 1'001}.isEquivalentTo({60'000, 1'001})));
    QVERIFY((!MediaRational{30'000, 1'001}.isEquivalentTo({30, 1})));
    QVERIFY(qAbs(ExportEngine::outputDuration(600, {30'000, 1'001}) - 20.02) < 0.0000001);
    QVERIFY(qAbs(ExportEngine::outputDuration(60, {60'000, 1'001}) - 1.001) < 0.0000001);
}

void TelemetryTests::preservesCfrCadenceForCommonRates()
{
    const QString ffmpeg = FfmpegTools::ffmpegPath();
    if (ffmpeg.isEmpty()) QSKIP("FFmpeg is unavailable for the CFR cadence integration test.");
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto runFfmpeg = [&ffmpeg](const QStringList &arguments) {
        QProcess process;
        process.start(ffmpeg, arguments);
        QVERIFY2(process.waitForStarted(), qPrintable(process.errorString()));
        QVERIFY2(process.waitForFinished(30'000), qPrintable(process.errorString()));
        QCOMPARE(process.exitStatus(), QProcess::NormalExit);
        QVERIFY2(process.exitCode() == 0, process.readAllStandardError().constData());
    };
    for (const MediaRational &rate : {MediaRational{30, 1}, MediaRational{30'000, 1'001},
                                      MediaRational{60'000, 1'001}}) {
        const QString rateText = QStringLiteral("%1/%2").arg(rate.numerator).arg(rate.denominator);
        const qsizetype expectedFrames = ExportEngine::frameCount(0.0, 1.0, rate);
        const QString source = directory.filePath(QStringLiteral("source-%1.mkv").arg(rate.numerator));
        const QString output = directory.filePath(QStringLiteral("output-%1.mp4").arg(rate.numerator));
        runFfmpeg({"-hide_banner", "-loglevel", "error", "-y", "-f", "lavfi", "-i",
                   QStringLiteral("color=c=black:s=64x16:r=%1:d=1").arg(rateText), "-frames:v",
                   QString::number(expectedFrames), "-c:v", "ffv1", "-pix_fmt", "bgra", source});
        runFfmpeg({"-hide_banner", "-loglevel", "error", "-y", "-i", source, "-vf",
                   QStringLiteral("fps=fps=%1:start_time=0:round=near:eof_action=round,trim=end_frame=%2,setpts=PTS-STARTPTS")
                       .arg(rateText).arg(expectedFrames),
                   "-fps_mode:v", "cfr", "-c:v", "mpeg4", "-q:v", "2", output});
        const MediaInfo info = MediaProbe::probe(output, {}, true, -1, {}, {}, true);
        QCOMPARE(info.videoFrameCount, expectedFrames);
        QCOMPARE(info.videoPacketCount, expectedFrames);
        QVERIFY(info.frameRate.isEquivalentTo(rate));
        QVERIFY(info.averageFrameRate.isEquivalentTo(rate));
        QVERIFY(qAbs(info.videoStartTime) <= info.timeBase.value());
        QVERIFY(qAbs(info.videoDuration - ExportEngine::outputDuration(expectedFrames, rate))
                <= 1.0 / rate.value());
    }
}

void TelemetryTests::validatesQuantizedTemporaryOverlayCadence()
{
    const MediaRational scheduledRate{60'000, 1'001};
    constexpr qsizetype expectedFrames = 7'193;
    const double scheduledDuration = ExportEngine::outputDuration(expectedFrames, scheduledRate);
    MediaInfo quantized;
    quantized.videoCodec = QStringLiteral("ffv1");
    quantized.videoSize = {3840, 2160};
    quantized.frameRate = {19'001, 317};
    quantized.averageFrameRate = {19'001, 317};
    quantized.timeBase = {1, 1'000};
    quantized.duration = 120.004;
    quantized.videoStartTime = 0.0;
    const TemporaryOverlayValidationResult quantizedResult = TemporaryOverlayValidation::validate(
        quantized, {3840, 2160}, scheduledRate, expectedFrames, scheduledDuration);
    QVERIFY(!quantized.frameRate.isEquivalentTo(scheduledRate));
    QVERIFY(!quantized.averageFrameRate.isEquivalentTo(scheduledRate));
    QVERIFY(!quantizedResult.nominalRateExact);
    QVERIFY(quantizedResult.nominalRateOk);
    QVERIFY(quantizedResult.averageRateOk);
    QVERIFY(quantizedResult.durationOk);
    QVERIFY(quantizedResult.passed());

    MediaInfo wrongCadence = quantized;
    wrongCadence.frameRate = {30, 1};
    wrongCadence.averageFrameRate = {30, 1};
    const TemporaryOverlayValidationResult wrongCadenceResult = TemporaryOverlayValidation::validate(
        wrongCadence, {3840, 2160}, scheduledRate, expectedFrames, scheduledDuration);
    QVERIFY(!wrongCadenceResult.nominalRateOk);
    QVERIFY(!wrongCadenceResult.averageRateOk);
    QVERIFY(!wrongCadenceResult.passed());

    const QString ffmpeg = FfmpegTools::ffmpegPath();
    if (ffmpeg.isEmpty()) QSKIP("FFmpeg is unavailable for the staged-overlay validation integration test.");
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString overlay = directory.filePath(QStringLiteral("quantized-overlay.mkv"));
    QProcess process;
    process.start(ffmpeg, {"-hide_banner", "-loglevel", "error", "-y", "-f", "lavfi", "-i",
                           "color=c=black:s=16x16:r=60000/1001", "-frames:v",
                           QString::number(expectedFrames), "-an", "-c:v", "ffv1", "-pix_fmt", "bgra",
                           "-f", "matroska", overlay});
    QVERIFY2(process.waitForStarted(), qPrintable(process.errorString()));
    QVERIFY2(process.waitForFinished(30'000), qPrintable(process.errorString()));
    QVERIFY2(process.exitCode() == 0, process.readAllStandardError().constData());

    const MediaInfo metadata = MediaProbe::probeSummary(overlay);
    const TemporaryOverlayValidationResult metadataResult = TemporaryOverlayValidation::validate(
        metadata, {16, 16}, scheduledRate, expectedFrames, scheduledDuration);
    QVERIFY(metadataResult.passed());
    const MediaInfo packetCount = MediaProbe::probe(overlay, {}, false, -1, {}, {}, true);
    QCOMPARE(packetCount.videoPacketCount, expectedFrames);
    const MediaInfo deepCount = MediaProbe::probe(overlay, {}, true);
    QCOMPARE(deepCount.videoFrameCount, expectedFrames);
}

void TelemetryTests::preservesAbsoluteExportTimestamps()
{
    const MediaRational rate{30'000, 1001};
    QCOMPARE(ExportEngine::framePresentationTime(120.0, 0, rate), 120.0);
    QVERIFY(qAbs(ExportEngine::framePresentationTime(120.0, 300, rate) - 130.01) < 0.000001);
    const SyncTransform sync{90.203, 1.0};
    TelemetrySession session;
    session.channels.insert(
        "speed", TelemetryChannel{"speed", {}, {210.203, 215.203}, {73.4F, 101.2F}});
    session.channels.insert(
        "rpm", TelemetryChannel{"rpm", {}, {210.203, 215.203}, {3842.0F, 5270.0F}});
    TelemetryRenderContext context;
    context.setSession(&session);
    context.setSyncTransform(sync);
    context.setTime(ExportEngine::framePresentationTime(120.0, 0, rate));
    QVERIFY(qAbs(context.telemetryTime() - 210.203) < 0.000001);
    QVERIFY(qAbs(context.telemetryValue("speed").toDouble() - 73.4) < 0.001);
    context.setTime(125.0);
    QVERIFY(qAbs(context.telemetryTime() - 215.203) < 0.000001);
    QVERIFY(qAbs(context.telemetryValue("rpm").toDouble() - 5270.0) < 0.001);
}

void TelemetryTests::composesNonZeroExportRangeWithZeroBasedOutput()
{
    const QString ffmpeg = FfmpegTools::ffmpegPath();
    if (ffmpeg.isEmpty()) QSKIP("FFmpeg is unavailable for the non-zero-range integration test.");
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    constexpr int width = 64;
    constexpr int height = 16;
    constexpr int sourceFrames = 300;
    constexpr int rangeStartFrame = 90;
    constexpr int exportFrames = 150;
    const QString primaryRaw = directory.filePath("source.rgba");
    const QString overlayRaw = directory.filePath("telemetry.rgba");
    const QString primary = directory.filePath("source.mkv");
    const QString overlay = directory.filePath("overlay.mkv");
    const QString composed = directory.filePath("composed.mkv");
    const QString decoded = directory.filePath("decoded.rgba");
    const auto writeIdentityFrames = [](const QString &path, const int count, const int pixelOffset) {
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly)) return false;
        for (int frame = 0; frame < count; ++frame) {
            QByteArray pixels(width * height * 4, '\0');
            for (int bit = 0; bit < 8; ++bit) {
                const int offset = (pixelOffset + bit) * 4;
                const char value = (frame & (1 << bit)) ? static_cast<char>(255) : 0;
                pixels[offset] = value;
                pixels[offset + 1] = value;
                pixels[offset + 2] = value;
                pixels[offset + 3] = static_cast<char>(255);
            }
            if (file.write(pixels) != pixels.size()) return false;
        }
        return true;
    };
    QVERIFY(writeIdentityFrames(primaryRaw, sourceFrames, 0));
    QVERIFY(writeIdentityFrames(overlayRaw, exportFrames, 8));
    const auto runFfmpeg = [&ffmpeg](const QStringList &arguments) {
        QProcess process;
        process.start(ffmpeg, arguments);
        QVERIFY2(process.waitForStarted(), qPrintable(process.errorString()));
        QVERIFY2(process.waitForFinished(30'000), qPrintable(process.errorString()));
        QCOMPARE(process.exitStatus(), QProcess::NormalExit);
        QVERIFY2(process.exitCode() == 0, process.readAllStandardError().constData());
    };
    const QString size = QStringLiteral("%1x%2").arg(width).arg(height);
    runFfmpeg({"-hide_banner", "-loglevel", "error", "-y", "-f", "rawvideo", "-pixel_format", "rgba",
               "-video_size", size, "-framerate", "30", "-i", primaryRaw, "-f", "lavfi", "-i",
               "sine=frequency=440:sample_rate=48000:duration=10", "-frames:v", QString::number(sourceFrames),
               "-c:v", "ffv1", "-pix_fmt", "bgra", "-c:a", "pcm_s16le", primary});
    runFfmpeg({"-hide_banner", "-loglevel", "error", "-y", "-f", "rawvideo", "-pixel_format", "rgba",
               "-video_size", size, "-framerate", "30", "-i", overlayRaw, "-an", "-c:v", "ffv1",
               "-pix_fmt", "bgra", overlay});
    runFfmpeg({"-hide_banner", "-loglevel", "error", "-y", "-i", primary, "-i", overlay,
               "-filter_complex", "[0:v]trim=start=3:end=8,setpts=PTS-STARTPTS,fps=fps=30/1:start_time=0:round=near:eof_action=round,trim=end_frame=150,setpts=PTS-STARTPTS[source];[1:v]setpts=PTS-STARTPTS[telemetry];[source][telemetry]overlay=0:0:shortest=1:repeatlast=0:eof_action=endall:format=rgb[video];[0:a]atrim=start=3:end=8,asetpts=PTS-STARTPTS[audio]",
               "-map", "[video]", "-fps_mode:v", "cfr", "-map", "[audio]", "-c:v", "ffv1", "-pix_fmt", "bgra", "-c:a",
               "pcm_s16le", composed});
    const MediaInfo outputInfo = MediaProbe::probe(composed, {}, true, -1, {}, {}, true);
    QCOMPARE(outputInfo.videoFrameCount, qsizetype(exportFrames));
    QCOMPARE(outputInfo.videoPacketCount, qsizetype(exportFrames));
    QVERIFY(outputInfo.frameRate.isEquivalentTo({30, 1}));
    QVERIFY(outputInfo.averageFrameRate.isEquivalentTo({30, 1}));
    QVERIFY(qAbs(outputInfo.videoStartTime) <= outputInfo.timeBase.value());
    QVERIFY(qAbs(outputInfo.videoDuration - 5.0) <= 1.0 / 30.0);
    QVERIFY(qAbs(outputInfo.audioStartTime - outputInfo.videoStartTime) <= 1.0 / 48000.0);
    QVERIFY(qAbs(outputInfo.audioDuration - 5.0) <= 1.0 / 48000.0);
    QVERIFY(qAbs(outputInfo.duration - 5.0) < 0.05);
    QVERIFY(!outputInfo.audioCodecs.isEmpty());
    const MediaInfo summaryInfo = MediaProbe::probeSummary(composed);
    QCOMPARE(summaryInfo.videoCodec, QStringLiteral("ffv1"));
    QCOMPARE(summaryInfo.videoSize, QSize(width, height));
    QVERIFY(qAbs(summaryInfo.duration - 5.0) < 0.05);
    QVERIFY(!summaryInfo.audioCodecs.isEmpty());
    runFfmpeg({"-hide_banner", "-loglevel", "error", "-y", "-i", composed, "-f", "rawvideo",
               "-pixel_format", "rgba", decoded});
    QFile decodedFile(decoded);
    QVERIFY(decodedFile.open(QIODevice::ReadOnly));
    const QByteArray output = decodedFile.readAll();
    const qsizetype bytesPerFrame = width * height * 4;
    QCOMPARE(output.size(), exportFrames * bytesPerFrame);
    const auto decodeIdentity = [](const char *pixels, const int pixelOffset) {
        int identity = 0;
        for (int bit = 0; bit < 8; ++bit) {
            if (static_cast<uchar>(pixels[(pixelOffset + bit) * 4]) > 127) identity |= 1 << bit;
        }
        return identity;
    };
    for (int frame = 0; frame < exportFrames; ++frame) {
        const char *pixels = output.constData() + frame * bytesPerFrame;
        QCOMPARE(decodeIdentity(pixels, 0), rangeStartFrame + frame);
        QCOMPARE(decodeIdentity(pixels, 8), frame);
    }
}

void TelemetryTests::normalizesNonZeroStreamPtsForVideoAndAudio()
{
    const QString ffmpeg = FfmpegTools::ffmpegPath();
    if (ffmpeg.isEmpty()) QSKIP("FFmpeg is unavailable for the stream-PTS integration test.");
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    constexpr int width = 64;
    constexpr int height = 16;
    const QString source = directory.filePath("offset-source.mkv");
    const QString overlay = directory.filePath("overlay.mkv");
    const QString output = directory.filePath("output.mkv");
    const auto runFfmpeg = [&ffmpeg](const QStringList &arguments) {
        QProcess process;
        process.start(ffmpeg, arguments);
        QVERIFY2(process.waitForStarted(), qPrintable(process.errorString()));
        QVERIFY2(process.waitForFinished(30'000), qPrintable(process.errorString()));
        QCOMPARE(process.exitStatus(), QProcess::NormalExit);
        QVERIFY2(process.exitCode() == 0, process.readAllStandardError().constData());
    };
    const QString size = QStringLiteral("%1x%2").arg(width).arg(height);
    runFfmpeg({"-hide_banner", "-loglevel", "error", "-y", "-f", "lavfi", "-i",
               QStringLiteral("color=c=black:s=%1:r=30:d=10").arg(size), "-f", "lavfi", "-i",
               "sine=frequency=440:sample_rate=48000:duration=10", "-output_ts_offset", "2",
               "-map", "0:v", "-map", "1:a", "-c:v", "ffv1", "-pix_fmt", "bgra", "-c:a",
               "pcm_s16le", source});
    const MediaInfo sourceInfo = MediaProbe::probe(source);
    QVERIFY(qAbs(sourceInfo.videoStartTime - 2.0) <= sourceInfo.timeBase.value());
    QVERIFY(qAbs(sourceInfo.audioStartTime - 2.0) <= sourceInfo.audioTimeBase.value());
    runFfmpeg({"-hide_banner", "-loglevel", "error", "-y", "-f", "lavfi", "-i",
               QStringLiteral("color=c=black:s=%1:r=30:d=5").arg(size), "-frames:v", "150",
               "-an", "-c:v", "ffv1", "-pix_fmt", "bgra", overlay});
    runFfmpeg({"-hide_banner", "-loglevel", "error", "-y", "-i", source, "-i", overlay,
               "-filter_complex", "[0:v]trim=start=3:end=8,setpts=PTS-STARTPTS,fps=fps=30/1:start_time=0:round=near:eof_action=round,trim=end_frame=150,setpts=PTS-STARTPTS[source];[1:v]setpts=PTS-STARTPTS[telemetry];[source][telemetry]overlay=0:0:shortest=1:repeatlast=0:eof_action=endall[video];[0:a]atrim=start=3:end=8,asetpts=PTS-STARTPTS[audio]",
               "-map", "[video]", "-fps_mode:v", "cfr", "-map", "[audio]", "-c:v", "ffv1",
               "-pix_fmt", "bgra", "-c:a", "pcm_s16le", output});
    const MediaInfo outputInfo = MediaProbe::probe(output, {}, true, -1, {}, {}, true);
    QCOMPARE(outputInfo.videoFrameCount, qsizetype(150));
    QCOMPARE(outputInfo.videoPacketCount, qsizetype(150));
    QVERIFY(outputInfo.frameRate.isEquivalentTo({30, 1}));
    QVERIFY(outputInfo.averageFrameRate.isEquivalentTo({30, 1}));
    QVERIFY(qAbs(outputInfo.videoStartTime) <= outputInfo.timeBase.value());
    QVERIFY(qAbs(outputInfo.audioStartTime) <= outputInfo.audioTimeBase.value());
    QVERIFY(qAbs(outputInfo.audioStartTime - outputInfo.videoStartTime) <= 1.0 / 48000.0);
    QVERIFY(qAbs(outputInfo.videoDuration - 5.0) <= 1.0 / 30.0);
    QVERIFY(qAbs(outputInfo.audioDuration - 5.0) <= 1.0 / 48000.0);
}

void TelemetryTests::convertsVfrInputToCfrWithFrameCorrectOverlay()
{
    const QString ffmpeg = FfmpegTools::ffmpegPath();
    if (ffmpeg.isEmpty()) QSKIP("FFmpeg is unavailable for the VFR-to-CFR integration test.");
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    constexpr int width = 64;
    constexpr int height = 16;
    const QString source = directory.filePath("vfr-source.mp4");
    const QString rawOverlay = directory.filePath("overlay.rgba");
    const QString overlay = directory.filePath("overlay.mkv");
    const QString output = directory.filePath("output.mkv");
    const QString decoded = directory.filePath("decoded.rgba");
    const auto runFfmpeg = [&ffmpeg](const QStringList &arguments) {
        QProcess process;
        process.start(ffmpeg, arguments);
        QVERIFY2(process.waitForStarted(), qPrintable(process.errorString()));
        QVERIFY2(process.waitForFinished(30'000), qPrintable(process.errorString()));
        QCOMPARE(process.exitStatus(), QProcess::NormalExit);
        QVERIFY2(process.exitCode() == 0, process.readAllStandardError().constData());
    };
    const QString size = QStringLiteral("%1x%2").arg(width).arg(height);
    runFfmpeg({"-hide_banner", "-loglevel", "error", "-y", "-f", "lavfi", "-i",
               QStringLiteral("testsrc2=size=%1:rate=30:duration=2").arg(size), "-vf",
               "select='eq(mod(n\\,5)\\,0)+eq(mod(n\\,5)\\,1)'", "-fps_mode:v", "vfr",
               "-c:v", "mpeg4", "-q:v", "2", source});
    const MediaInfo sourceInfo = MediaProbe::probe(source, {}, false, -1, {}, {}, true);
    QVERIFY(sourceInfo.likelyVariableFrameRate);
    const MediaRational exportRate = sourceInfo.averageFrameRate;
    QVERIFY(exportRate.isValid());
    const qsizetype expectedFrames = ExportEngine::frameCount(0.0, sourceInfo.duration, exportRate);
    QCOMPARE(expectedFrames, qsizetype(24));
    QFile rawFile(rawOverlay);
    QVERIFY(rawFile.open(QIODevice::WriteOnly));
    for (qsizetype frame = 0; frame < expectedFrames; ++frame) {
        QByteArray pixels(width * height * 4, '\0');
        for (int bit = 0; bit < 8; ++bit) {
            const char value = (frame & (qsizetype(1) << bit)) ? static_cast<char>(255) : 0;
            pixels[bit * 4] = value;
            pixels[bit * 4 + 1] = value;
            pixels[bit * 4 + 2] = value;
            pixels[bit * 4 + 3] = static_cast<char>(255);
        }
        QCOMPARE(rawFile.write(pixels), qint64(pixels.size()));
    }
    rawFile.close();
    const QString rate = QStringLiteral("%1/%2").arg(exportRate.numerator).arg(exportRate.denominator);
    runFfmpeg({"-hide_banner", "-loglevel", "error", "-y", "-f", "rawvideo", "-pixel_format",
               "rgba", "-video_size", size, "-framerate", rate, "-i", rawOverlay, "-frames:v",
               QString::number(expectedFrames), "-an", "-c:v", "ffv1", "-pix_fmt", "bgra", overlay});
    runFfmpeg({"-hide_banner", "-loglevel", "error", "-y", "-i", source, "-i", overlay,
               "-filter_complex", QStringLiteral("[0:v]trim=start=0:end=%1,setpts=PTS-STARTPTS,fps=fps=%2:start_time=0:round=near:eof_action=round,trim=end_frame=%3,setpts=PTS-STARTPTS[source];[1:v]setpts=PTS-STARTPTS[telemetry];[source][telemetry]overlay=0:0:shortest=1:repeatlast=0:eof_action=endall:format=rgb[video]")
                                      .arg(sourceInfo.duration, 0, 'f', 9).arg(rate).arg(expectedFrames),
               "-map", "[video]", "-fps_mode:v", "cfr", "-c:v", "ffv1", "-pix_fmt", "bgra", output});
    const MediaInfo outputInfo = MediaProbe::probe(output, {}, true, -1, {}, {}, true);
    QCOMPARE(outputInfo.videoFrameCount, expectedFrames);
    QCOMPARE(outputInfo.videoPacketCount, expectedFrames);
    QVERIFY(outputInfo.frameRate.isEquivalentTo(exportRate));
    QVERIFY(outputInfo.averageFrameRate.isEquivalentTo(exportRate));
    QVERIFY(qAbs(outputInfo.videoStartTime) <= outputInfo.timeBase.value());
    QVERIFY(qAbs(outputInfo.videoDuration - ExportEngine::outputDuration(expectedFrames, exportRate))
            <= 1.0 / exportRate.value());
    runFfmpeg({"-hide_banner", "-loglevel", "error", "-y", "-i", output, "-f", "rawvideo",
               "-pixel_format", "rgba", decoded});
    QFile decodedFile(decoded);
    QVERIFY(decodedFile.open(QIODevice::ReadOnly));
    const QByteArray frames = decodedFile.readAll();
    const qsizetype bytesPerFrame = width * height * 4;
    QCOMPARE(frames.size(), expectedFrames * bytesPerFrame);
    for (qsizetype frame = 0; frame < expectedFrames; ++frame) {
        const char *pixels = frames.constData() + frame * bytesPerFrame;
        int identity = 0;
        for (int bit = 0; bit < 8; ++bit) {
            if (static_cast<uchar>(pixels[bit * 4]) > 127) identity |= 1 << bit;
        }
        QCOMPARE(identity, static_cast<int>(frame));
    }
}

void TelemetryTests::estimatesExportProgress()
{
    ExportProgressEstimator estimator;
    const MediaRational rate{60, 1};
    const auto early = estimator.update(1, 600, 100, rate);
    QVERIFY(!early.etaAvailable);
    const auto steady = estimator.update(100, 600, 1'100, rate);
    QVERIFY(steady.etaAvailable);
    QVERIFY(qAbs(steady.throughputFps - 99.0) < 0.1);
    QVERIFY(qAbs(steady.realtimeFactor - 1.65) < 0.01);
    QVERIFY(steady.etaSeconds > 5.0 && steady.etaSeconds < 6.0);
    QCOMPARE(ExportProgressEstimator::stageProgress("rendering", 1.0), 95.0);
    QCOMPARE(ExportProgressEstimator::stageProgress("finalizing", 0.0), 97.0);
    QCOMPARE(ExportProgressEstimator::stageProgress("validating", 1.0), 99.0);
    QCOMPARE(ExportProgressEstimator::stageProgress("complete", 0.0), 100.0);
    QCOMPARE(ExportProgressEstimator::stageProgress("cancelled", 1.0), 0.0);
}

void TelemetryTests::parsesStructuredFfmpegProgress()
{
    FfmpegProgressParser parser;
    const QList<FfmpegProgress> first = parser.append(
        "frame=42\nfps=27.5\nout_time_us=700700\nspeed=0.46x\nprogress=continue\n");
    QCOMPARE(first.size(), 1);
    QCOMPARE(first.front().encodedFrames, qsizetype(42));
    QCOMPARE(first.front().outputMicroseconds, qint64(700700));
    QVERIFY(qAbs(first.front().encoderFps - 27.5) < 0.001);
    QVERIFY(qAbs(first.front().realtimeFactor - 0.46) < 0.001);
    QVERIFY(!first.front().complete);

    const QList<FfmpegProgress> split = parser.append("frame=60\nout_time_ms=1001000\nprogress=");
    QVERIFY(split.isEmpty());
    const QList<FfmpegProgress> last = parser.append("end\n");
    QCOMPARE(last.size(), 1);
    QCOMPARE(last.front().encodedFrames, qsizetype(60));
    QCOMPARE(last.front().outputMicroseconds, qint64(1001000));
    QVERIFY(last.front().complete);
}

void TelemetryTests::calculatesEncodedOutputProgress()
{
    QCOMPARE(FfmpegProgressParser::overallPercent(0.0, 10.0), 0.0);
    QVERIFY(FfmpegProgressParser::overallPercent(2.5, 10.0)
            < FfmpegProgressParser::overallPercent(5.0, 10.0));
    QCOMPARE(FfmpegProgressParser::overallPercent(5.0, 10.0), 47.5);
    QCOMPARE(FfmpegProgressParser::overallPercent(12.0, 10.0), 95.0);
    QCOMPARE(FfmpegProgressParser::overallPercent(-1.0, 10.0), 0.0);
}

void TelemetryTests::preservesFrameIdentityThroughCompletedOverlayComposition()
{
    const QString ffmpeg = FfmpegTools::ffmpegPath();
    if (ffmpeg.isEmpty()) QSKIP("FFmpeg is unavailable for the frame-identity integration test.");
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    constexpr int width = 64;
    constexpr int height = 16;
    constexpr int frameCount = 150;
    const QString primary = directory.filePath("primary.mkv");
    const QString rawOverlay = directory.filePath("identity.rgba");
    const QString overlay = directory.filePath("overlay.mkv");
    const QString composed = directory.filePath("composed.mkv");
    const QString decoded = directory.filePath("decoded.rgba");
    QFile rawFile(rawOverlay);
    QVERIFY(rawFile.open(QIODevice::WriteOnly));
    for (int frame = 0; frame < frameCount; ++frame) {
        QByteArray pixels(width * height * 4, '\0');
        for (int bit = 0; bit < 8; ++bit) {
            const int offset = bit * 4;
            const char value = (frame & (1 << bit)) ? static_cast<char>(255) : 0;
            pixels[offset] = value;
            pixels[offset + 1] = value;
            pixels[offset + 2] = value;
            pixels[offset + 3] = static_cast<char>(255);
        }
        QCOMPARE(rawFile.write(pixels), qint64(pixels.size()));
    }
    rawFile.close();
    const auto runFfmpeg = [&ffmpeg](const QStringList &arguments) {
        QProcess process;
        process.start(ffmpeg, arguments);
        QVERIFY2(process.waitForStarted(), qPrintable(process.errorString()));
        QVERIFY2(process.waitForFinished(30'000), qPrintable(process.errorString()));
        QCOMPARE(process.exitStatus(), QProcess::NormalExit);
        QVERIFY2(process.exitCode() == 0, process.readAllStandardError().constData());
    };
    runFfmpeg({"-hide_banner", "-loglevel", "error", "-y", "-f", "lavfi", "-i",
               QStringLiteral("color=c=black:s=%1x%2:r=30").arg(width).arg(height),
               "-frames:v", QString::number(frameCount), "-c:v", "ffv1", "-pix_fmt", "bgra", primary});
    runFfmpeg({"-hide_banner", "-loglevel", "error", "-y", "-f", "rawvideo", "-pixel_format",
               "rgba", "-video_size", QStringLiteral("%1x%2").arg(width).arg(height), "-framerate", "30",
               "-i", rawOverlay, "-frames:v", QString::number(frameCount), "-c:v", "ffv1", "-pix_fmt",
               "bgra", overlay});
    runFfmpeg({"-hide_banner", "-loglevel", "error", "-y", "-i", primary, "-i", overlay,
               "-filter_complex", "[0:v][1:v]overlay=0:0:shortest=1:repeatlast=0:eof_action=endall:format=rgb[v]",
               "-map", "[v]", "-c:v", "ffv1", "-pix_fmt", "bgra", composed});
    runFfmpeg({"-hide_banner", "-loglevel", "error", "-y", "-i", composed, "-f", "rawvideo",
               "-pixel_format", "rgba", decoded});
    QFile decodedFile(decoded);
    QVERIFY(decodedFile.open(QIODevice::ReadOnly));
    const QByteArray output = decodedFile.readAll();
    const qsizetype bytesPerFrame = width * height * 4;
    QCOMPARE(output.size(), frameCount * bytesPerFrame);
    for (int frame = 0; frame < frameCount; ++frame) {
        const char *pixels = output.constData() + frame * bytesPerFrame;
        int identity = 0;
        for (int bit = 0; bit < 8; ++bit) {
            if (static_cast<uchar>(pixels[bit * 4]) > 127) identity |= 1 << bit;
        }
        QCOMPARE(identity, frame);
    }
}

void TelemetryTests::decodesGps9Gpmf()
{
    QByteArray scale;
    for (const qint32 value : {10000000, 10000000, 1000, 1000, 100, 1, 1000, 100, 1}) {
        append32(scale, value);
    }
    QByteArray gps;
    for (int index = 0; index < 2; ++index) {
        append32(gps, 500000000 + index * 100);
        append32(gps, 190000000 + index * 100);
        append32(gps, 250000);
        append32(gps, 1250 + index * 250);
        append32(gps, 130);
        append32(gps, 10000);
        append32(gps, 200000 + index * 100);
        append16(gps, 150);
        append16(gps, 3);
    }
    QByteArray stream;
    stream += klvRecord("SCAL", 'l', 4, 9, scale);
    stream += klvRecord("GPS9", '?', 32, 2, gps);
    const QByteArray streamRecord = klvRecord("STRM", 0, 1, stream.size(), stream);
    const QByteArray packet = klvRecord("DEVC", 0, 1, streamRecord.size(), streamRecord);

    const GoProTelemetryResult result =
        GoProTelemetrySource::decodeGpsPackets({{packet, 10.0, 1.0}}, 20.0);
    QCOMPARE(result.gpsStream, QString("GPS9"));
    QCOMPARE(result.session.sampleCount, 2);
    const TelemetryChannel speed = result.session.channels.value("GoPro GPS speed");
    QCOMPARE(speed.timestamps, QVector<double>({10.0, 10.5}));
    QVERIFY(qAbs(speed.values[0] - 4.5F) < 0.001F);
    QVERIFY(qAbs(speed.values[1] - 5.4F) < 0.001F);
}

void TelemetryTests::rejectsMalformedGpmf()
{
    QVERIFY_THROWS_EXCEPTION(
        std::runtime_error,
        (void) GoProTelemetrySource::decodeGpsPackets({{{"broken"}, 0.0, 1.0}}, 1.0));
}

void TelemetryTests::cancelsSlowGoProProbePromptly()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString media = directory.filePath(QStringLiteral("slow.mp4"));
    QVERIFY(writeBytes(media, "media"));
    std::atomic_bool cancelled = false;
    std::thread canceller([&cancelled] {
        QThread::msleep(150);
        cancelled.store(true);
    });
    const auto join = qScopeGuard([&canceller] { canceller.join(); });
    QElapsedTimer elapsed;
    elapsed.start();
    QVERIFY_THROWS_EXCEPTION(
        OperationCancelled,
        (void) GoProTelemetrySource::load(
            media, [&cancelled] { return cancelled.load(); }, QStringLiteral(PROBE_TEST_HELPER_PATH)));
    QVERIFY2(elapsed.elapsed() < 3'000, qPrintable(QString::number(elapsed.elapsed())));
}

void TelemetryTests::boundsGoProProbeOutput()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString media = directory.filePath(QStringLiteral("large-output.mp4"));
    QVERIFY(writeBytes(media, "media"));
    try {
        (void) GoProTelemetrySource::load(media, {}, QStringLiteral(PROBE_TEST_HELPER_PATH));
        QFAIL("Expected ffprobe output to be rejected");
    } catch (const ResourceLimitError &error) {
        QVERIFY(QString::fromUtf8(error.what()).contains(QStringLiteral("ffprobe output")));
    }
}

void TelemetryTests::rejectsOutOfFileGpmfPackets()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    for (const QString &name : {QStringLiteral("outside.mp4"), QStringLiteral("overflow.mp4")}) {
        const QString media = directory.filePath(name);
        QVERIFY(writeBytes(media, "tiny"));
        try {
            (void) GoProTelemetrySource::load(media, {}, QStringLiteral(PROBE_TEST_HELPER_PATH));
            QFAIL("Expected malformed packet bounds to be rejected");
        } catch (const std::runtime_error &error) {
            QVERIFY(QString::fromUtf8(error.what()).contains(QStringLiteral("outside the media file")));
        }
    }
}

void TelemetryTests::boundsGpmfDepthAndRecordCount()
{
    QVector<GpmfPacket> tooManyPackets(GoProTelemetrySource::kMaximumPacketCount + 1);
    QVERIFY_THROWS_EXCEPTION(
        ResourceLimitError,
        (void) GoProTelemetrySource::decodeGpsPackets(tooManyPackets, 1.0));

    QByteArray nested = klvRecord("JUNK", 'c', 1, 1, QByteArray(1, 'x'));
    for (int depth = 0; depth <= GoProTelemetrySource::kMaximumContainerDepth; ++depth) {
        nested = klvRecord("DEVC", 0, 1, static_cast<quint16>(nested.size()), nested);
    }
    QVERIFY_THROWS_EXCEPTION(
        ResourceLimitError,
        (void) GoProTelemetrySource::decodeGpsPackets({{nested, 0.0, 1.0}}, 1.0));

    QByteArray manyRecords;
    manyRecords.reserve((GoProTelemetrySource::kMaximumRecordCount + 1) * 8);
    const QByteArray emptyRecord = klvRecord("JUNK", 'c', 1, 0, {});
    for (qsizetype index = 0; index <= GoProTelemetrySource::kMaximumRecordCount; ++index) {
        manyRecords += emptyRecord;
    }
    QVERIFY_THROWS_EXCEPTION(
        ResourceLimitError,
        (void) GoProTelemetrySource::decodeGpsPackets({{manyRecords, 0.0, 1.0}}, 1.0));
}

void TelemetryTests::normalizesGpmfTimestamps()
{
    const auto packetAt = [](const double pts, const qint32 speed) {
        QByteArray scale;
        for (const qint32 value : {10000000, 10000000, 1000, 1000, 100, 1, 1000, 100, 1}) append32(scale, value);
        QByteArray gps;
        for (const qint32 value : {500000000, 190000000, 250000, speed, 130, 10000, 200000}) append32(gps, value);
        append16(gps, 150);
        append16(gps, 3);
        QByteArray stream = klvRecord("SCAL", 'l', 4, 9, scale);
        stream += klvRecord("GPS9", '?', 32, 1, gps);
        const QByteArray streamRecord = klvRecord("STRM", 0, 1, stream.size(), stream);
        return GpmfPacket{klvRecord("DEVC", 0, 1, streamRecord.size(), streamRecord), pts, 1.0};
    };
    const GoProTelemetryResult result = GoProTelemetrySource::decodeGpsPackets(
        {packetAt(2.0, 1000), packetAt(1.0, 2000), packetAt(1.0, 3000)}, 3.0);
    const TelemetryChannel speed = result.session.channels.value(QStringLiteral("GoPro GPS speed"));
    QCOMPARE(speed.timestamps, QVector<double>({1.0, 2.0}));
    QVERIFY(speed.timestamps[1] > speed.timestamps[0]);
    QVERIFY(qAbs(speed.values[0] - 7.2F) < 0.001F);
}

void TelemetryTests::synchronizesGpsSpeed()
{
    const TelemetrySession video = speedSession(0.0, 60.0, 0.0);
    const TelemetrySession telemetry = speedSession(0.0, 70.0, 3.2);
    const SyncCandidate candidate = TelemetrySyncEngine::synchronize(video, telemetry);
    QVERIFY2(qAbs(candidate.offset - 3.2) <= 0.11, qPrintable(QString::number(candidate.offset)));
    QVERIFY(candidate.diagnostics.correlation > 0.99);
    QVERIFY(candidate.confidence > 0.7);
}

void TelemetryTests::cancelsSynchronizationDeterministically()
{
    const TelemetrySession video = speedSession(0.0, 600.0, 0.0);
    const TelemetrySession telemetry = speedSession(0.0, 700.0, 3.2);
    int checks = 0;
    QVERIFY_THROWS_EXCEPTION(
        OperationCancelled,
        (void) TelemetrySyncEngine::synchronize(
            video, telemetry, [&checks] { return ++checks == 20; }));
    QVERIFY(checks >= 20);
}

void TelemetryTests::reportsAmbiguousGpsSpeed()
{
    TelemetrySession video = speedSession(0.0, 30.0, 0.0);
    TelemetrySession telemetry = speedSession(0.0, 35.0, 0.0);
    std::fill(video.channels["speed"].values.begin(), video.channels["speed"].values.end(), 42.0F);
    std::fill(
        telemetry.channels["speed"].values.begin(),
        telemetry.channels["speed"].values.end(),
        42.0F);
    const SyncCandidate candidate = TelemetrySyncEngine::synchronize(video, telemetry);
    QCOMPARE(candidate.diagnostics.correlation, -1.0);
    QCOMPARE(candidate.confidence, 0.0);
}

void TelemetryTests::syncsOptionalRealRecording()
{
    const QString videoPath = qEnvironmentVariable("FLAPPEDEAR_REAL_GOPRO");
    const QString vboPath = qEnvironmentVariable("FLAPPEDEAR_REAL_VBO");
    if (videoPath.isEmpty() || vboPath.isEmpty()) {
        QSKIP("FLAPPEDEAR_REAL_GOPRO and FLAPPEDEAR_REAL_VBO are not set");
    }
    const GoProTelemetryResult video = GoProTelemetrySource::load(videoPath);
    const TelemetrySession telemetry = VboParser::parseFile(vboPath);
    const SyncCandidate candidate = TelemetrySyncEngine::synchronize(video.session, telemetry);
    qInfo().noquote()
        << QStringLiteral("real GoPro: %1 packets, %2 %3 samples; offset=%4 correlation=%5 confidence=%6")
               .arg(video.packetCount)
               .arg(video.session.sampleCount)
               .arg(video.gpsStream)
               .arg(candidate.offset, 0, 'f', 3)
               .arg(candidate.diagnostics.correlation, 0, 'f', 6)
               .arg(candidate.confidence, 0, 'f', 3);
    QVERIFY(video.packetCount > 0);
    QVERIFY(video.session.sampleCount > 100);
    QVERIFY(candidate.diagnostics.correlation > 0.8);
    QVERIFY(candidate.confidence > 0.5);
}

QTEST_MAIN(TelemetryTests)
#include "TelemetryTests.moc"
