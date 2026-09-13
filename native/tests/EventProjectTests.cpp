#include "EventProjectFixture.h"
#include "project/EventProjectCodec.h"
#include "project/ProjectLimits.h"
#include "project/ProjectRecoveryStore.h"
#include "export/ExportOutputTransaction.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QtTest>
#include <functional>
#include <limits>
#include "telemetry/TelemetrySession.h"

using namespace FlappedEar;
namespace Fixture = EventProjectFixture;

class EventProjectTests final : public QObject {
    Q_OBJECT
private slots:
    void acceptsLegacyAndEventDocuments();
    void preservesFiniteExtremeSyncForGuardedConsumers();
    void rejectsMalformedEvents_data();
    void rejectsMalformedEvents();
    void boundsRunsAndSources();
    void rebasesInactiveAlternativeAndMissingReferences();
    void prefersMovedRelativeSourceToStaleAbsoluteFallback();
    void retainsEventRecoveryIdentity();
    void protectsInactiveSourcesFromExportOverwrite();
    void keepsMissingReferencesPortableThroughDirectorySymlinks();
};

void EventProjectTests::acceptsLegacyAndEventDocuments()
{
    const QJsonObject event = Fixture::project();
    QString error;
    QVERIFY2(ProjectLimits::validateProject(event, &error), qPrintable(error));
    const QJsonObject editor = EventProjectCodec::editorProjection(event);
    QVERIFY(ProjectLimits::validateProject(editor, &error));
    QVERIFY(!editor.contains("event"));
    QCOMPARE(editor.value("version").toInt(), 2);
    QCOMPARE(editor.value("sync").toObject().value("offset").toDouble(), 2.5);
    QCOMPARE(editor.value("sources").toObject().value("telemetry").toObject(), Fixture::reference(Fixture::runs(event)[0].toObject()));
    QCOMPARE(EventProjectCodec::editorProjection(editor), editor);
    QCOMPARE(EventProjectCodec::withEditorState(event, editor, {}, {}), event);
    QFile example(QStringLiteral(EVENT_PROJECT_FIXTURE_PATH));
    QVERIFY(example.open(QIODevice::ReadOnly));
    const auto sample = QJsonDocument::fromJson(example.readAll()).object();
    QVERIFY2(ProjectLimits::validateProject(sample, &error), qPrintable(error));
}

void EventProjectTests::preservesFiniteExtremeSyncForGuardedConsumers()
{
    auto project = Fixture::project();
    auto event = project.value("event").toObject();
    auto runs = event.value("runs").toArray();
    auto run = runs[0].toObject();
    run.insert("sync", QJsonObject{{"offset", 0.0}, {"timeScale", std::numeric_limits<double>::max()}});
    runs[0] = run; event.insert("runs", runs); project.insert("event", event);
    QString error;
    QVERIFY2(ProjectLimits::validateProject(project, &error), qPrintable(error));
    const auto reopened = QJsonDocument::fromJson(QJsonDocument(project).toJson()).object();
    QVERIFY2(ProjectLimits::validateProject(reopened, &error), qPrintable(error));
    const auto sync = EventProjectCodec::editorProjection(reopened).value("sync").toObject();
    QCOMPARE(sync.value("timeScale").toDouble(), std::numeric_limits<double>::max());
    const SyncTransform transform{sync.value("offset").toDouble(), sync.value("timeScale").toDouble()};
    QVERIFY(!videoToTelemetryTime(2, transform));
    QCOMPARE(videoToTelemetryTime(0, transform).value(), 0.0);
}

void EventProjectTests::rejectsMalformedEvents_data()
{
    QTest::addColumn<QJsonObject>("project");
    const auto row = [](const char *name, const std::function<void(QJsonObject &)> &mutate) {
        QJsonObject project = Fixture::project();
        mutate(project);
        QTest::newRow(name) << project;
    };
    for (const QString &key : {QStringLiteral("sources"), QStringLiteral("sync"), QStringLiteral("videoPath"), QStringLiteral("vboPath")}) {
        row(qPrintable("root-" + key), [key](QJsonObject &p) { p.insert(key, QJsonObject{}); });
    }
    row("v2-event", [](QJsonObject &p) { p.insert("version", 2); });
    row("fractional-version", [](QJsonObject &p) { p.insert("version", 3.1); });
    row("missing-event", [](QJsonObject &p) { p.remove("event"); });
    row("empty-runs", [](QJsonObject &p) { Fixture::setRuns(p, {}); });
    row("unknown-active", [](QJsonObject &p) { auto e = p.value("event").toObject(); e.insert("activeRunId", "missing"); p.insert("event", e); });
    row("duplicate-run", [](QJsonObject &p) { auto runs = Fixture::runs(p); runs.append(runs[0]); Fixture::setRuns(p, runs); });
    const auto runRow = [&row](const char *name, const std::function<void(QJsonObject &)> &mutate) {
        row(name, [mutate](QJsonObject &p) { auto runs = Fixture::runs(p); auto run = runs[0].toObject(); mutate(run); runs[0] = run; Fixture::setRuns(p, runs); });
    };
    runRow("foreign-primary", [](QJsonObject &r) { r.insert("primaryTelemetrySourceId", "run-b-source"); });
    runRow("empty-name", [](QJsonObject &r) { r.insert("name", "  "); });
    runRow("long-id", [](QJsonObject &r) { r.insert("id", QString(129, 'x')); });
    runRow("bad-sync", [](QJsonObject &r) { r.insert("sync", QJsonObject{{"offset", "2.5"}, {"timeScale", 1.0}}); });
    runRow("zero-scale", [](QJsonObject &r) { r.insert("sync", QJsonObject{{"offset", 0.0}, {"timeScale", 0.0}}); });
    runRow("null-sync", [](QJsonObject &r) { r.insert("sync", QJsonValue::Null); });
    runRow("no-sources", [](QJsonObject &r) { r.insert("sources", QJsonObject{}); });
    runRow("bad-video", [](QJsonObject &r) { auto s = r.value("sources").toObject(); s.insert("video", QJsonObject{}); r.insert("sources", s); });
    runRow("duplicate-source", [](QJsonObject &r) { auto s = r.value("sources").toObject(); auto t = s.value("telemetry").toArray(); t.append(t[0]); s.insert("telemetry", t); r.insert("sources", s); });
    runRow("cross-run-source-id", [](QJsonObject &r) { auto s = r.value("sources").toObject(); auto t = s.value("telemetry").toArray(); auto a = t[1].toObject(); a.insert("id", "run-b-source"); t[1] = a; s.insert("telemetry", t); r.insert("sources", s); });
    runRow("relative-as-absolute", [](QJsonObject &r) { auto s = r.value("sources").toObject(); s.insert("video", QJsonObject{{"absolutePath", "camera.mp4"}}); r.insert("sources", s); });
    runRow("bad-fingerprint", [](QJsonObject &r) { auto s = r.value("sources").toObject(); s.insert("video", QJsonObject{{"relativePath", "camera.mp4"}, {"fingerprint", 3}}); r.insert("sources", s); });
}

void EventProjectTests::rejectsMalformedEvents()
{
    QFETCH(QJsonObject, project);
    QString error;
    QVERIFY(!ProjectLimits::validateProject(project, &error));
    QVERIFY(!error.isEmpty());
}

void EventProjectTests::boundsRunsAndSources()
{
    QJsonObject project = Fixture::project();
    QJsonArray runs;
    for (int i = 0; i < 64; ++i) runs.append(Fixture::run(i == 0 ? "run-a" : QString::number(i), "a.vbo", 0));
    Fixture::setRuns(project, runs);
    QString error;
    QVERIFY(ProjectLimits::validateProject(project, &error));
    runs.append(Fixture::run("excess", "a.vbo", 0));
    Fixture::setRuns(project, runs);
    QVERIFY(!ProjectLimits::validateProject(project, &error));
    project = Fixture::project();
    runs = Fixture::runs(project);
    auto first = runs[0].toObject();
    auto sources = first.value("sources").toObject();
    auto telemetry = sources.value("telemetry").toArray();
    for (int i = 2; i < 9; ++i) telemetry.append(Fixture::source(QString::number(i), "a.rcz"));
    sources.insert("telemetry", telemetry); first.insert("sources", sources); runs[0] = first; Fixture::setRuns(project, runs);
    QVERIFY(!ProjectLimits::validateProject(project, &error));
    // 64 runs with two sources each fit; one more source breaches the total ceiling.
    runs = {};
    for (int i = 0; i < 64; ++i) {
        auto run = Fixture::run(i == 0 ? "run-a" : QString::number(i), "a.vbo", 0);
        auto s = run.value("sources").toObject();
        auto t = s.value("telemetry").toArray();
        t.append(Fixture::source(QStringLiteral("alternative-%1").arg(i), "a.rcz"));
        s.insert("telemetry", t); run.insert("sources", s); runs.append(run);
    }
    Fixture::setRuns(project, runs);
    QVERIFY(ProjectLimits::validateProject(project, &error));
    auto last = runs.last().toObject();
    auto lastSources = last.value("sources").toObject();
    auto lastTelemetry = lastSources.value("telemetry").toArray();
    lastTelemetry.append(Fixture::source("excess", "b.rcz"));
    lastSources.insert("telemetry", lastTelemetry); last.insert("sources", lastSources);
    runs[runs.size() - 1] = last; Fixture::setRuns(project, runs);
    QVERIFY(!ProjectLimits::validateProject(project, &error));
}

void EventProjectTests::rebasesInactiveAlternativeAndMissingReferences()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString oldPath = directory.filePath("old/event.fetproject");
    const QString newPath = directory.filePath("new/event.fetproject");
    QJsonObject project = Fixture::project();
    auto originalRuns = Fixture::runs(project);
    auto inactive = originalRuns[1].toObject();
    auto inactiveSources = inactive.value("sources").toObject();
    inactiveSources.insert("video", QJsonObject{{"relativePath", "run-b.mp4"}, {"futureVideo", true}});
    inactive.insert("sources", inactiveSources); originalRuns[1] = inactive; Fixture::setRuns(project, originalRuns);
    QJsonObject editor = EventProjectCodec::editorProjection(project);
    auto sources = editor.value("sources").toObject();
    for (const QString &key : {QStringLiteral("video"), QStringLiteral("telemetry")}) {
        const auto ref = ProjectSourceReferenceCodec::fromProject(editor, key, {});
        auto object = sources.value(key).toObject();
        const auto known = EventProjectCodec::referenceForSave(ref, oldPath, newPath);
        for (auto it = known.begin(); it != known.end(); ++it) object.insert(it.key(), it.value());
        sources.insert(key, object);
    }
    editor.insert("sources", sources);
    auto sync = editor.value("sync").toObject(); sync.insert("offset", 9.0); editor.insert("sync", sync);
    const auto saved = EventProjectCodec::withEditorState(project, editor, oldPath, newPath);
    QString error;
    QVERIFY2(ProjectLimits::validateProject(saved, &error), qPrintable(error));
    QVERIFY(!saved.contains("sources")); QVERIFY(!saved.contains("sync"));
    const auto runs = Fixture::runs(saved);
    QCOMPARE(Fixture::reference(runs[0].toObject()).value("relativePath").toString(), QStringLiteral("../old/run-a.vbo"));
    QCOMPARE(Fixture::reference(runs[0].toObject(), 1).value("relativePath").toString(), QStringLiteral("../old/run-a.rcz"));
    QCOMPARE(Fixture::reference(runs[1].toObject()).value("relativePath").toString(), QStringLiteral("../old/run-b.vbo"));
    QCOMPARE(Fixture::reference(runs[1].toObject()).value("futureReference").toInt(), 42);
    const auto inactiveVideo = runs[1].toObject().value("sources").toObject().value("video").toObject();
    QCOMPARE(inactiveVideo.value("relativePath").toString(), QStringLiteral("../old/run-b.mp4"));
    QVERIFY(inactiveVideo.value("futureVideo").toBool());
    QCOMPARE(runs[0].toObject().value("sync").toObject().value("offset").toDouble(), 9.0);
    QCOMPARE(runs[1].toObject().value("sync"), Fixture::runs(project)[1].toObject().value("sync"));
    QCOMPARE(saved.value("futureRoot"), project.value("futureRoot"));
    QCOMPARE(saved.value("event").toObject().value("id"), project.value("event").toObject().value("id"));
    const auto second = EventProjectCodec::withEditorState(saved, EventProjectCodec::editorProjection(saved), newPath, newPath);
    QCOMPARE(second, saved);
}

void EventProjectTests::prefersMovedRelativeSourceToStaleAbsoluteFallback()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    // Save As requires an existing destination directory. Resolve its real path
    // too on macOS, where the temporary root can be reached through /var.
    QVERIFY(QDir().mkpath(directory.filePath("new")));
    const QString actual = directory.filePath("a.vbo");
    QFile file(actual); QVERIFY(file.open(QIODevice::WriteOnly)); file.close();
    const auto json = EventProjectCodec::referenceForSave(
        {"a.vbo", directory.filePath("stale/a.vbo"), {{"digest", "retained"}}},
        directory.filePath("event.fetproject"), directory.filePath("new/event.fetproject"));
    QCOMPARE(json.value("relativePath").toString(), QStringLiteral("../a.vbo"));
    QCOMPARE(json.value("fingerprint").toObject().value("digest").toString(), QStringLiteral("retained"));
}

void EventProjectTests::retainsEventRecoveryIdentity()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ProjectRecoveryStore store(directory.filePath("recovery.json"));
    const ProjectRecoverySnapshot snapshot{directory.filePath("event.fetproject"), "event-document", 5, 4,
        "2026-09-12T12:00:00.000Z", Fixture::project(), true};
    QString error;
    QVERIFY2(store.write(snapshot, &error), qPrintable(error));
    ProjectRecoverySnapshot restored;
    QVERIFY2(store.load(&restored, &error), qPrintable(error));
    QCOMPARE(restored.project, snapshot.project);
    QCOMPARE(restored.documentId, snapshot.documentId);
    QCOMPARE(restored.revision, quint64(5));
    QCOMPARE(restored.lastSavedRevision, quint64(4));
}

void EventProjectTests::protectsInactiveSourcesFromExportOverwrite()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto project = Fixture::project();
    const auto paths = EventProjectCodec::referencedPaths(project, directory.filePath("event.fetproject"));
    QCOMPARE(paths.size(), 4);
    for (const QString &path : paths) {
        QFile file(path); QVERIFY(file.open(QIODevice::WriteOnly)); file.close();
        ExportOutputTransaction transaction;
        QCOMPARE(transaction.prepare(path, directory.filePath("active-video.mp4"), paths, true).status,
                 ExportOutputTransaction::PreparationStatus::Error);
    }
}

void EventProjectTests::keepsMissingReferencesPortableThroughDirectorySymlinks()
{
#ifdef Q_OS_UNIX
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString real = directory.filePath("real");
    const QString alias = directory.filePath("alias");
    QVERIFY(QDir().mkpath(real));
    QVERIFY(QFile::link(real, alias));
    const auto rebased = EventProjectCodec::referenceForSave(
        {"missing.vbo", {}, {}}, QDir(alias).filePath("event.fetproject"), QDir(alias).filePath("saved.fetproject"));
    QCOMPARE(rebased.value("relativePath").toString(), QStringLiteral("missing.vbo"));
    QCOMPARE(rebased.value("absolutePath").toString(), QDir(QFileInfo(real).canonicalFilePath()).filePath("missing.vbo"));
#else
    QSKIP("Directory symlink portability is covered on macOS/Unix.");
#endif
}

QTEST_GUILESS_MAIN(EventProjectTests)
#include "EventProjectTests.moc"
