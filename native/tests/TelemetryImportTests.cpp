#include "RczFixture.h"
#include "telemetry/TelemetryImportPlan.h"

#include <QFile>
#include <QTemporaryDir>
#include <QtTest>
#include <algorithm>
#include <cmath>

using namespace FlappedEar;

namespace {

QString writeSource(const QTemporaryDir &directory, const QString &name, const QByteArray &bytes)
{
    const QString path = directory.filePath(name);
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size()) {
        throw std::runtime_error("Cannot create synthetic import fixture.");
    }
    file.close();
    return path;
}

QByteArray vbo(const QByteArray &speed = "72")
{
    return "[column names]\ntime latitude longitude velocity heart_rate\n[data]\n"
        "0 50 20 " + speed + " 120\n1 50.001 20.001 80 125\n";
}

// Two exports of a synthetic 31-second moving recording. The relative time
// origins differ intentionally: VBO starts at the first sample; RCZ retains
// its recording-origin-to-first-sample delay.
QByteArray movingVbo(const bool stationary = false, const bool distant = false,
                     const bool sparse = false)
{
    QByteArray text = "[column names]\ntime latitude longitude velocity\n[data]\n";
    for (int i = 0; i < 32; ++i) {
        if (sparse && i > 7 && i < 24) continue;
        text += QByteArray::number(i) + ' '
            + QByteArray::number((distant ? 51.0 : 50.0) + (stationary ? 0 : i * .0001), 'f', 6)
            + " 20 72\n";
    }
    return text;
}

QByteArray movingRcz(const bool stationary = false)
{
    auto members = RczFixture::members();
    QByteArray timestamps, coordinates, speed;
    for (int i = 0; i < 32; ++i) {
        RczFixture::append<qint64>(timestamps, RczFixture::origin + 100 + i * 1000);
        RczFixture::append<qint32>(coordinates, 300000000 + (stationary ? 0 : i * 600));
        RczFixture::append<qint32>(coordinates, 120000000);
        RczFixture::append<qint32>(speed, 20000);
    }
    members["channel_1_300_0_1_1"] = timestamps;
    members["channel_1_300_0_3_1"] = coordinates;
    members["channel_1_300_0_4_0"] = speed;
    return RczFixture::zip(members);
}

} // namespace

class TelemetryImportTests : public QObject {
    Q_OBJECT
private slots:
    void reportsCompletedFileProgress()
    {
        QTemporaryDir directory; QVERIFY(directory.isValid());
        const auto good = writeSource(directory, "good.vbo", vbo());
        const auto bad = writeSource(directory, "bad.rcz", "invalid");
        QList<qsizetype> counts;
        const auto plan = prepareTelemetryImport({good, good, bad}, {}, {},
            [&counts](qsizetype processed, qsizetype total) { if (total == 3) counts.append(processed); });
        QCOMPARE(counts, QList<qsizetype>({0, 1, 2, 3}));
        QCOMPARE(plan.files.size(), 3);
    }
    void importsIndependentRunsWithoutCombiningClocks()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto a = writeSource(directory, "first.VBO", vbo());
        const auto b = writeSource(directory, "second.rcz", RczFixture::zip(RczFixture::members()));
        const auto plan = prepareTelemetryImport({a, b});
        QCOMPARE(plan.runs.size(), 2);
        QCOMPARE(plan.files.size(), 2);
        QCOMPARE(plan.files[0].status, TelemetryImportFileStatus::Ready);
        QCOMPARE(plan.files[1].status, TelemetryImportFileStatus::Ready);
        const auto &first = plan.runs[0];
        const auto &second = plan.runs[1];
        QVERIFY(first.id != second.id);
        QCOMPARE(first.telemetry->duration, 1.0);
        QCOMPARE(second.telemetry->duration, 2.0);
        QCOMPARE(first.telemetry->valueAt("heartRate", 0).value(), 120.0);
        QCOMPARE(second.telemetry->valueAt("heartRate", .15).value(), 120.0);
        QVERIFY(!first.telemetry->valueAt("brake", 0));
        QVERIFY(!second.telemetry->valueAt("speed", 1));
        QCOMPARE(first.laps.status, LapSessionStatus::NoSourceStartGate);
        const auto directLaps = deriveSourceLapSession(*second.telemetry);
        QCOMPARE(second.laps.status, directLaps.status);
        QCOMPARE(second.laps.timedLaps.size(), directLaps.timedLaps.size());
        QCOMPARE(first.format, QStringLiteral("vbo"));
        QCOMPARE(second.format, QStringLiteral("rcz"));
        QCOMPARE(first.contentSha256.size(), 32);
        QVERIFY(first.sourceId.startsWith("sha256:"));
        QVERIFY(plan.possibleSameRuns.isEmpty());
    }

    void deduplicatesBytesNotFilenamesAndRetainsEveryResult()
    {
        QTemporaryDir directory;
        const auto a = writeSource(directory, "session.vbo", vbo());
        const auto copy = writeSource(directory, "renamed.VBO", vbo());
        const auto other = writeSource(directory, "different.vbo", vbo("73"));
        const auto plan = prepareTelemetryImport({a, copy, other, a});
        QCOMPARE(plan.runs.size(), 2);
        QCOMPARE(plan.files.size(), 4);
        QCOMPARE(plan.files[1].status, TelemetryImportFileStatus::Duplicate);
        QCOMPARE(plan.files[3].status, TelemetryImportFileStatus::Duplicate);
        QCOMPARE(plan.files[1].requestedPath, copy);
        QCOMPARE(plan.files[0].runId, plan.files[1].runId);
        QVERIFY(plan.files[0].runId != plan.files[2].runId);
        const auto reordered = prepareTelemetryImport({other, copy});
        QCOMPARE(reordered.runs[0].id, plan.runs[1].id);
        QCOMPARE(reordered.runs[1].id, plan.runs[0].id);
    }

    void retainsCompleteLapsWithinTheirOwningRun()
    {
        QTemporaryDir directory;
        // The existing directional-lap regression, imported through the batch API.
        const QByteArray bytes =
            "[laptiming]\nStart 21.0000 52.0000 21.0000 52.0002 start\n"
            "[column names]\ntime latitude longitude\n[data]\n"
            "0 52.0001 21.0002\n1 52.0001 21.0002\n2 52.0001 20.9998\n"
            "3 52.0008 20.9998\n4 52.0008 21.0002\n5 52.0001 21.0002\n"
            "6 52.0001 20.9998\n7 52.0008 20.9998\n8 52.0008 21.0002\n"
            "10 52.0001 21.0002\n11 52.0001 20.9998\n12 52.0008 20.9998\n"
            "13 52.0008 21.0002\n14 52.0001 21.0002\n15 52.0001 20.9998\n";
        const auto a = writeSource(directory, "laps.vbo", bytes);
        const auto b = writeSource(directory, "other.vbo", vbo());
        const auto plan = prepareTelemetryImport({a, b});
        QCOMPARE(plan.runs.size(), 2);
        const auto &laps = plan.runs[0].laps;
        QCOMPARE(laps.status, LapSessionStatus::Available);
        QCOMPARE(laps.timedLaps.size(), 3);
        QCOMPARE(laps.lapTraces.size(), 3);
        QVERIFY(std::abs(laps.timedLaps[0].durationSeconds - 4.0) < .001);
        QVERIFY(std::abs(laps.timedLaps[1].durationSeconds - 5.0) < .001);
        QVERIFY(std::abs(laps.timedLaps[2].durationSeconds - 4.0) < .001);
        QCOMPARE(laps.fastestLapIndex, std::optional<qsizetype>(0));
        QVERIFY(plan.runs[1].laps.timedLaps.isEmpty());
    }

    void preservesSelectedFormatForLinkedSource()
    {
#ifdef Q_OS_WIN
        QSKIP("POSIX file-link dispatch check; macOS is the target for this iteration.");
#else
        QTemporaryDir directory;
        const auto backing = writeSource(directory, "recording.data", vbo());
        const auto selected = directory.filePath("selected.vbo");
        QVERIFY(QFile::link(backing, selected));
        const auto plan = prepareTelemetryImport({selected});
        QCOMPARE(plan.runs.size(), 1);
        QCOMPARE(plan.runs[0].format, QStringLiteral("vbo"));
        QCOMPARE(plan.runs[0].sourcePath, selected);
#endif
    }

    void isolatesFailuresAndDoesNotDeduplicateUnparsedContent()
    {
        QTemporaryDir directory;
        const auto good = writeSource(directory, "good.vbo", vbo());
        const auto bad = writeSource(directory, "bad.vbo", "not telemetry");
        const auto badCopy = writeSource(directory, "bad-copy.vbo", "not telemetry");
        const auto wrongFormat = writeSource(directory, "wrong.rcz", vbo());
        const auto empty = writeSource(directory, "empty.vbo", {});
        const auto plan = prepareTelemetryImport({bad, good, badCopy, wrongFormat, empty,
                                                  directory.filePath("missing.vbo"), directory.path(), "wrong.csv"});
        QCOMPARE(plan.runs.size(), 1);
        QCOMPARE(plan.files.size(), 8);
        QCOMPARE(plan.files[1].status, TelemetryImportFileStatus::Ready);
        for (int index : {0, 2, 3, 4, 5, 6, 7}) {
            QCOMPARE(plan.files[index].status, TelemetryImportFileStatus::Error);
            QVERIFY(plan.files[index].runId.isEmpty());
            QVERIFY(!plan.files[index].message.isEmpty());
        }
    }

    void enforcesFileAndBatchBudgets()
    {
        QTemporaryDir directory;
        const auto a = writeSource(directory, "a.vbo", vbo());
        const auto b = writeSource(directory, "b.vbo", vbo("73"));
        TelemetryImportLimits limits;
        limits.maximumFiles = 1;
        QVERIFY_THROWS_EXCEPTION(ResourceLimitError, (void)prepareTelemetryImport({a, b}, limits));
        limits = {};
        limits.maximumFileBytes = vbo().size() - 1;
        QCOMPARE(prepareTelemetryImport({a}, limits).files[0].status, TelemetryImportFileStatus::Error);
        limits = {};
        limits.maximumBatchBytes = vbo().size();
        const auto plan = prepareTelemetryImport({a, b}, limits);
        QCOMPARE(plan.runs.size(), 1);
        QCOMPARE(plan.files[0].status, TelemetryImportFileStatus::Ready);
        QCOMPARE(plan.files[1].status, TelemetryImportFileStatus::Error);
        QVERIFY(plan.files[1].message.contains("input-byte"));
        // Repeated selections consume the IO budget as well.
        QCOMPARE(prepareTelemetryImport({a, a}, limits).files[1].status, TelemetryImportFileStatus::Error);
    }

    void boundsAdmittedChannelSamples()
    {
        QTemporaryDir directory;
        const auto a = writeSource(directory, "a.vbo", vbo());
        const auto b = writeSource(directory, "b.vbo", vbo("73"));
        TelemetryImportLimits limits;
        limits.maximumRetainedChannelSamples = 8; // four channels, two samples
        const auto plan = prepareTelemetryImport({a, b, a}, limits);
        QCOMPARE(plan.runs.size(), 1);
        QCOMPARE(plan.files[0].status, TelemetryImportFileStatus::Ready);
        QCOMPARE(plan.files[1].status, TelemetryImportFileStatus::Error);
        QVERIFY(plan.files[1].message.contains("decoded-sample"));
        QCOMPARE(plan.files[2].status, TelemetryImportFileStatus::Duplicate);
        --limits.maximumRetainedChannelSamples;
        QVERIFY(prepareTelemetryImport({a}, limits).runs.isEmpty());
    }

    void rejectsInvalidLimits()
    {
        TelemetryImportLimits limits;
        limits.maximumFiles = 0;
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument, (void)prepareTelemetryImport({}, limits));
        limits = {};
        ++limits.maximumFileBytes;
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument, (void)prepareTelemetryImport({}, limits));
        limits = {};
        ++limits.maximumBatchBytes;
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument, (void)prepareTelemetryImport({}, limits));
        limits = {};
        ++limits.maximumRetainedChannelSamples;
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument, (void)prepareTelemetryImport({}, limits));
        QVERIFY(prepareTelemetryImport({}).runs.isEmpty());
    }

    void cancellationNeverReturnsPartialSuccess()
    {
        QTemporaryDir directory;
        const auto a = writeSource(directory, "a.vbo", vbo());
        QVERIFY_THROWS_EXCEPTION(OperationCancelled, (void)prepareTelemetryImport({a}, {}, [] { return true; }));
        // Same callback checkpoints on repeated reads: cancel well after the
        // first file completed, including parsing/digest/matching preparation.
        int singleChecks = 0;
        (void)prepareTelemetryImport({a}, {}, [&] { ++singleChecks; return false; });
        const auto b = writeSource(directory, "b.vbo", vbo("73"));
        int checks = 0;
        QVERIFY_THROWS_EXCEPTION(OperationCancelled,
            (void)prepareTelemetryImport({a, b}, {}, [&] { return ++checks >= singleChecks; }));
        QCOMPARE(checks, singleChecks);
    }

    void reportsCrossFormatCandidatesWithoutMerging()
    {
        QTemporaryDir directory;
        const auto v = writeSource(directory, "unrelated-name.vbo", movingVbo());
        const auto r = writeSource(directory, "another-name.rcz", movingRcz());
        const auto plan = prepareTelemetryImport({v, r});
        QCOMPARE(plan.runs.size(), 2);
        QCOMPARE(plan.possibleSameRuns.size(), 1);
        const auto &match = plan.possibleSameRuns[0];
        QCOMPARE(match.firstRunId, plan.runs[0].id);
        QCOMPARE(match.secondRunId, plan.runs[1].id);
        QCOMPARE(match.comparedGpsSamples, 32);
        QVERIFY(match.maximumSeparationMeters < .01);
        QVERIFY(match.gpsDurationDifferenceSeconds < .001);
        QVERIFY(match.reviewReason.contains("Confirm recording date"));
        QCOMPARE(plan.files[0].status, TelemetryImportFileStatus::Ready);
        QCOMPARE(plan.files[1].status, TelemetryImportFileStatus::Ready);
    }

    void rejectsContentChangedBetweenHashAndParse()
    {
        QTemporaryDir directory;
        const auto a = writeSource(directory, "a.vbo", vbo());
        int checks = 0;
        const auto plan = prepareTelemetryImport({a}, {}, [&] {
            // Entry, file, digest chunk and digest completion precede the
            // existing parser's entry cancellation check for this tiny file.
            if (++checks == 5) (void)writeSource(directory, "a.vbo", vbo("73"));
            return false;
        });
        QVERIFY(checks >= 5);
        QVERIFY(plan.runs.isEmpty());
        QCOMPARE(plan.files[0].status, TelemetryImportFileStatus::Error);
        QVERIFY(plan.files[0].message.contains("changed during import"));
    }

    void rejectsWeakMatchEvidence_data()
    {
        QTest::addColumn<QString>("kind");
        for (const auto *kind : {"stationary", "distant", "sparse", "same-format", "duration"})
            QTest::newRow(kind) << QString::fromLatin1(kind);
    }
    void rejectsWeakMatchEvidence()
    {
        QFETCH(QString, kind);
        QTemporaryDir directory;
        auto text = movingVbo(kind == "stationary", kind == "distant", kind == "sparse");
        if (kind == "duration") text.replace("31 50.003100", "40 50.003100");
        const auto a = writeSource(directory, "same-name.vbo", text);
        const auto b = kind == "same-format"
            ? writeSource(directory, "other.vbo", movingVbo() + "\n")
            : writeSource(directory, "same-name.rcz", movingRcz(kind == "stationary"));
        const auto plan = prepareTelemetryImport({a, b});
        QCOMPARE(plan.runs.size(), 2);
        QVERIFY(plan.possibleSameRuns.isEmpty());
    }

    void preservesAmbiguousOneToManyMatchesForReview()
    {
        QTemporaryDir directory;
        const auto a = writeSource(directory, "a.vbo", movingVbo());
        const auto b = writeSource(directory, "b.vbo", movingVbo() + "\n");
        const auto r = writeSource(directory, "recording.rcz", movingRcz());
        const auto plan = prepareTelemetryImport({a, b, r});
        QCOMPARE(plan.runs.size(), 3);
        QCOMPARE(plan.possibleSameRuns.size(), 2);
    }
};

QTEST_GUILESS_MAIN(TelemetryImportTests)
#include "TelemetryImportTests.moc"
