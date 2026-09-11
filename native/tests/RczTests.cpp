#include "RczFixture.h"
#include "telemetry/RczParser.h"
#include "telemetry/TelemetrySource.h"
#include "telemetry/VboParser.h"
#include "telemetry/LapTiming.h"
#include <QTemporaryFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QtTest>
#include <algorithm>
#include <cmath>
#include <limits>

using namespace FlappedEar;
namespace {
TelemetrySession parse(const QByteArray &bytes, const CancellationCheck &cancel = {})
{
    QTemporaryFile file(QDir::tempPath() + "/synthetic-XXXXXX.rcz");
    if (!file.open() || file.write(bytes) != bytes.size() || !file.flush()) throw std::runtime_error("Fixture write failed");
    file.close();
    return TelemetrySource::load(file.fileName(), cancel);
}
}
class RczTests : public QObject {
    Q_OBJECT
private slots:
    void nativeChannels_data()
    {
        QTest::addColumn<bool>("compressed");
        QTest::newRow("stored") << false;
        QTest::newRow("deflated") << true;
    }
    void nativeChannels()
    {
        QFETCH(bool, compressed);
        const auto session = parse(RczFixture::zip(RczFixture::members(), compressed));
        QCOMPARE(session.sampleCount, 5);
        QCOMPARE(session.duration, 2.0);
        QCOMPARE(session.startTime, (RczFixture::origin % 86400000) / 1000.0);
        QCOMPARE(session.valueAt("speed", .2).value(), 72.0);
        QCOMPARE(session.valueAt("rpm", .25).value(), 3000.0);
        QCOMPARE(session.valueAt("rpm", .75).value(), 4000.0);
        QVERIFY(!session.valueAt("rpm", .2));
        QCOMPARE(session.valueAt("heartRate", .15).value(), 120.0);
        QCOMPARE(session.valueAt("brake", .25).value(), 0.0);
        QCOMPARE(session.valueAt("x_acc-acc", .15).value(), 1.0);
        QCOMPARE(session.valueAt("latitude", .1).value(), 50.0);
        QCOMPARE(session.valueAt("longitude", .1).value(), 20.0);
        QVERIFY(!session.valueAt("lateralG", .15));
        QVERIFY(!session.valueAt("longitudinalG", .15));
        QVERIFY(!session.valueAt("speed", 1.0));
        QCOMPARE(session.valueAt("speed", 2.0).value(), 180.0);
        QCOMPARE(session.timingGates.size(), 1);
        const auto gate = session.timingGates.first();
        QVERIFY(gate.endpointA.latitudeDegrees > 50.0);
        QVERIFY(gate.endpointB.latitudeDegrees < 50.0);
        QVERIFY(std::abs(gate.endpointB.longitudeDegrees - 20.0) < 1e-8);
        QVERIFY(TelemetrySource::supportsPath("SESSION.RCZ"));
    }
    void missingValuesAndPrimaryGps()
    {
        auto files = RczFixture::members();
        files["channel_1_300_0_4_0"] = RczFixture::ints({10000,INT_MAX,30000,40000,50000});
        files["channel2_5_200_10024_10024_3"] = RczFixture::doubles({3000,std::numeric_limits<double>::infinity()});
        // An undeclared GPS must not override the explicitly selected device.
        files["channel_1_100_0_4_0"] = RczFixture::ints({999999});
        const auto session = parse(RczFixture::zip(files));
        QVERIFY(!session.valueAt("speed", .2));
        QVERIFY(!session.valueAt("speed", .15));
        QVERIFY(!session.valueAt("rpm", 1.25));
        QCOMPARE(session.valueAt("speed", .3).value(), 108.0);
    }
    void rejectsInvalidSessions_data()
    {
        QTest::addColumn<QString>("kind");
        for (const auto *kind : {"version", "origin", "resumed", "multiple", "missing", "timestamps", "length", "encoding", "ambiguous", "path", "json", "deep-json"})
            QTest::newRow(kind) << QString::fromLatin1(kind);
    }
    void rejectsInvalidSessions()
    {
        QFETCH(QString, kind);
        auto files = RczFixture::members();
        if (kind == "version") files["session.json"].replace("\"version\":1", "\"version\":2");
        if (kind == "origin") files["sessionfragment.json"].replace("1780000000000", "1780000000001");
        if (kind == "resumed") files["session.json"].replace("\"laps\":[]", "\"laps\":[{\"sessionResume\":1}]");
        if (kind == "multiple") files["other/session.json"] = files["session.json"];
        if (kind == "missing") files.remove("channel_1_300_0_3_1");
        if (kind == "timestamps") files["channel_1_300_0_1_1"] = RczFixture::ticks({100,200,200,400,2000});
        if (kind == "length") files["channel_1_300_0_4_0"].chop(1);
        if (kind == "encoding") files["channel_1_300_0_4_9"] = files.take("channel_1_300_0_4_0");
        if (kind == "ambiguous") {
            files["channel_6_401_0_1_1"] = files["channel_6_400_0_1_1"];
            files["channel_6_401_0_41_0"] = files["channel_6_400_0_41_0"];
        }
        if (kind == "path") files["../session.json"] = files["session.json"];
        if (kind == "json") files["session.json"] = "not JSON";
        if (kind == "deep-json") files["session.json"] = "{\"nested\":" + QByteArray(26,'[') + "0" + QByteArray(26,']') + "}";
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, parse(RczFixture::zip(files)));
    }
    void rejectsCorruptArchives_data()
    {
        QTest::addColumn<QString>("kind");
        for (const auto *kind : {"truncated", "crc", "bomb", "declared-limit", "symlink", "overlap", "encrypted", "zip64"})
            QTest::newRow(kind) << QString::fromLatin1(kind);
    }
    void rejectsCorruptArchives()
    {
        QFETCH(QString, kind);
        auto zip = RczFixture::zip(RczFixture::members());
        const auto end = zip.size() - 22;
        const auto central = qFromLittleEndian<quint32>(zip.constData() + end + 16);
        if (kind == "truncated") zip.chop(5);
        if (kind == "crc") {
            qToLittleEndian<quint32>(123, zip.data() + 14);
            qToLittleEndian<quint32>(123, zip.data() + central + 16);
        }
        if (kind == "bomb" || kind == "declared-limit") {
            const quint32 size = kind == "bomb" ? 1 : static_cast<quint32>(RczParser::maximumMemberBytes + 1);
            qToLittleEndian(size, zip.data() + 22); qToLittleEndian(size, zip.data() + central + 24);
        }
        if (kind == "symlink") qToLittleEndian<quint32>(0120000U << 16, zip.data() + central + 38);
        if (kind == "overlap") qToLittleEndian<quint32>(central, zip.data() + central + 42);
        if (kind == "encrypted") qToLittleEndian<quint16>(1, zip.data() + central + 8);
        if (kind == "zip64") qToLittleEndian<quint16>(45, zip.data() + central + 6);
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, parse(zip));
    }
    void cancelsDuringDecompression()
    {
        auto files = RczFixture::members();
        // First member: multiple inflate chunks, within ordinary member limits.
        files["channel_1_300_0_1_1"] = QByteArray(2 * 1024 * 1024, '\0');
        int checks = 0;
        const int cancelAt = files.size() + 8;
        QVERIFY_THROWS_EXCEPTION(OperationCancelled, parse(RczFixture::zip(files), [&] { return ++checks >= cancelAt; }));
        QCOMPARE(checks, cancelAt);
    }
    void optionalPrivatePair()
    {
        const auto rczPath = qEnvironmentVariable("FLAPPEDEAR_REAL_RCZ");
        const auto vboPath = qEnvironmentVariable("FLAPPEDEAR_RCZ_REFERENCE_VBO");
        const auto metadataPath = qEnvironmentVariable("FLAPPEDEAR_RCZ_REFERENCE_SESSION_JSON");
        if (rczPath.isEmpty() || vboPath.isEmpty() || metadataPath.isEmpty())
            QSKIP("Private matching RCZ/VBO and extracted session.json paths not supplied.");
        const auto native = TelemetrySource::load(rczPath);
        const auto reference = VboParser::parseFile(vboPath);
        QVERIFY(native.sampleCount > reference.sampleCount);
        QCOMPARE(native.timingGates.size(), reference.timingGates.size());
        const double shift = reference.startTime - native.startTime;
        for (const auto &alias : {QString("speed"), QString("rpm"), QString("heartRate"), QString("brake")}) {
            const auto &channel = reference.channels[reference.aliases.value(alias)];
            QVector<double> errors;
            for (qsizetype i = 0; i < channel.timestamps.size(); ++i) {
                const auto value = native.valueAt(alias, channel.timestamps[i] + shift);
                if (value && std::isfinite(channel.values[i])) errors.append(std::abs(*value - channel.values[i]));
            }
            QVERIFY(errors.size() > reference.sampleCount / 2);
            std::sort(errors.begin(), errors.end());
            const double median = errors[errors.size() / 2];
            qInfo() << alias << "median/max absolute difference" << median << errors.last();
            QVERIFY(median < (alias == "rpm" ? 5.0 : .2));
        }
        const auto nativeLaps = deriveSourceLapSession(native);
        const auto referenceLaps = deriveSourceLapSession(reference);
        qInfo() << "Native samples/channels/duration/laps" << native.sampleCount << native.channels.size() << native.duration << nativeLaps.timedLaps.size();
        // RaceChrono's VBO timing row differs from its RCZ centre/travel-bearing
        // representation. Validate laps against the recording's own timestamps.
        QFile metadataFile(metadataPath);
        QVERIFY(metadataFile.open(QIODevice::ReadOnly));
        QVERIFY(metadataFile.size() <= 1024 * 1024);
        const auto metadata = QJsonDocument::fromJson(metadataFile.readAll()).object();
        const auto origin = metadata.value("firstTimestamp").toInteger();
        QVector<QPair<double,double>> expected;
        for (const auto &item : metadata.value("laps").toArray()) {
            const auto lap = item.toObject();
            if (!lap.contains("finishTimestamp") || lap.value("isInvalid").toBool()) continue;
            expected.append({(lap.value("startTimestamp").toInteger() - origin) / 1000.0,
                (lap.value("finishTimestamp").toInteger() - origin) / 1000.0});
        }
        QCOMPARE(nativeLaps.timedLaps.size(), expected.size());
        QVERIFY(!expected.isEmpty());
        double maximumLapError = 0;
        for (qsizetype i = 0; i < expected.size(); ++i) {
            QVERIFY(std::abs(nativeLaps.timedLaps[i].startTelemetryTime - expected[i].first) < .1);
            const double error = std::abs(nativeLaps.timedLaps[i].durationSeconds - (expected[i].second - expected[i].first));
            maximumLapError = std::max(maximumLapError, error);
            QVERIFY(error < .1);
        }
        qInfo() << "Maximum lap duration difference vs recorded RCZ metadata" << maximumLapError
                << "seconds; legacy VBO-derived laps" << referenceLaps.timedLaps.size();
    }
};
QTEST_GUILESS_MAIN(RczTests)
#include "RczTests.moc"
