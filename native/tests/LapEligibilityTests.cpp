#include "EventProjectFixture.h"
#include "telemetry/LapTiming.h"
#include "telemetry/OutingLaps.h"
#include "telemetry/TelemetryRenderContext.h"
#include "telemetry/VboParser.h"

#include <QtTest>
#include <limits>

using namespace FlappedEar;

class LapEligibilityTests final : public QObject {
    Q_OBJECT
private slots:
    void retainsMeasuredLapWithInteriorGpsGap();
    void rejectsInvalidAndUnpairedCoordinates_data();
    void rejectsInvalidAndUnpairedCoordinates();
    void exposesNoBestWhenEveryLapHasMissingGps();
    void rendererNeverUsesIneligibleCompletedLap();
    void propagatesOutingEligibilityAndBest();
    void preservesNormalLapRanking();
    void rendererOmitsDeltaAtMissingCurrentCoordinate();
};

void LapEligibilityTests::retainsMeasuredLapWithInteriorGpsGap()
{
    auto session = VboParser::parse(QString::fromUtf8(EventProjectFixture::lapsVbo()));
    for (const auto &name : {QStringLiteral("latitude"), QStringLiteral("longitude")}) {
        auto &channel = session.channels[session.aliases.value(name)];
        for (auto &time : channel.timestamps) if (time >= 3.0) time += 10.0;
    }
    session.duration += 10.0;
    const auto laps = deriveSourceLapSession(session);
    QCOMPARE(laps.timedLaps.size(), 3);
    QVERIFY(laps.timedLaps[0].durationSeconds > 13.9);
    QCOMPARE(laps.timedLaps[0].referenceIssue, LapReferenceIssue::GpsGap);
    QVERIFY(!laps.timedLaps[0].referenceEligible());
    QCOMPARE(laps.lapTraces.size(), 2);
    for (const auto &trace : laps.lapTraces) QVERIFY(trace.lapNumber != 1);
    QVERIFY(laps.fastestLapIndex.has_value());
    QVERIFY(laps.timedLaps[*laps.fastestLapIndex].referenceEligible());
}

void LapEligibilityTests::rejectsInvalidAndUnpairedCoordinates_data()
{
    QTest::addColumn<int>("kind");
    QTest::newRow("missing-coordinate") << 0;
    QTest::newRow("out-of-range-coordinate") << 1;
    QTest::newRow("unaligned-coordinate-clock") << 2;
}

void LapEligibilityTests::rejectsInvalidAndUnpairedCoordinates()
{
    QFETCH(int, kind);
    auto session = VboParser::parse(QString::fromUtf8(EventProjectFixture::lapsVbo()));
    auto &latitude = session.channels[session.aliases.value("latitude")];
    auto &longitude = session.channels[session.aliases.value("longitude")];
    if (kind == 0) latitude.values[3] = std::numeric_limits<float>::quiet_NaN();
    if (kind == 1) latitude.values[3] = 100.0F;
    if (kind == 2) longitude.timestamps[3] += 0.01;
    const auto laps = deriveSourceLapSession(session);
    QCOMPARE(laps.timedLaps.size(), 3);
    QCOMPARE(laps.timedLaps[0].referenceIssue, LapReferenceIssue::InvalidGps);
    QCOMPARE(laps.lapTraces.size(), 2);
    QVERIFY(laps.fastestLapIndex.has_value());
    QVERIFY(*laps.fastestLapIndex != 0);
}

void LapEligibilityTests::exposesNoBestWhenEveryLapHasMissingGps()
{
    auto session = VboParser::parse(QString::fromUtf8(EventProjectFixture::lapsVbo()));
    auto &latitude = session.channels[session.aliases.value("latitude")];
    for (int index : {3, 7, 11}) latitude.values[index] = std::numeric_limits<float>::quiet_NaN();
    const auto laps = deriveSourceLapSession(session);
    QCOMPARE(laps.timedLaps.size(), 3);
    QVERIFY(!laps.fastestLapIndex.has_value());
    QVERIFY(laps.lapTraces.isEmpty());
    const auto rows = outingLapRows(session, laps, "run", "Run", 0);
    QCOMPARE(rows.size(), 5);
    for (const auto &row : rows) {
        QVERIFY(!row.bestOfRun);
        QVERIFY(!row.referenceEligible);
        if (row.type == LapSectionType::Lap)
            QCOMPARE(row.referenceIssue, LapReferenceIssue::InvalidGps);
    }
}

void LapEligibilityTests::rendererNeverUsesIneligibleCompletedLap()
{
    auto session = VboParser::parse(QString::fromUtf8(EventProjectFixture::lapsVbo()));
    auto &latitude = session.channels[session.aliases.value("latitude")];
    latitude.values[3] = std::numeric_limits<float>::quiet_NaN();
    const auto laps = deriveSourceLapSession(session);
    TelemetryRenderContext context;
    context.setSession(&session);
    context.setLapSession(laps);
    context.setTime(6.0);
    const auto first = context.lapTiming();
    QCOMPARE(first.value("lastLapNumber").toInt(), 1);
    QVERIFY(!first.contains("bestLapNumber"));
    QVERIFY(!first.contains("liveDeltaSeconds"));
    QVERIFY(!first.contains("lastDeltaToBestSeconds"));
    context.setTime(11.0);
    QCOMPARE(context.lapTiming().value("bestLapNumber").toInt(), 2);
}

void LapEligibilityTests::propagatesOutingEligibilityAndBest()
{
    auto session = VboParser::parse(QString::fromUtf8(EventProjectFixture::lapsVbo()));
    session.channels[session.aliases.value("latitude")].values[3] = std::numeric_limits<float>::quiet_NaN();
    const auto laps = deriveSourceLapSession(session);
    const auto rows = outingLapRows(session, laps, "run", "Run", 0);
    QCOMPARE(rows.size(), 5);
    QVERIFY(!rows[1].referenceEligible);
    QCOMPARE(rows[1].referenceIssue, LapReferenceIssue::InvalidGps);
    QVERIFY(!rows[1].bestOfRun);
    QCOMPARE(rows[3].lapNumber, 3);
    QVERIFY(rows[3].referenceEligible);
    QVERIFY(rows[3].bestOfRun);
    QVERIFY(!rows.first().bestOfRun);
    QVERIFY(!rows.last().bestOfRun);
}

void LapEligibilityTests::preservesNormalLapRanking()
{
    const auto session = VboParser::parse(QString::fromUtf8(EventProjectFixture::lapsVbo()));
    const auto laps = deriveSourceLapSession(session);
    QCOMPARE(laps.timedLaps.size(), 3);
    QCOMPARE(laps.lapTraces.size(), 3);
    QCOMPARE(laps.fastestLapIndex, std::optional<qsizetype>(0));
    for (const auto &lap : laps.timedLaps) QVERIFY(lap.referenceEligible());
    QVERIFY(qAbs(laps.timedLaps[1].deltaToBestSeconds - 1.0) < 0.001);
    const auto rows = outingLapRows(session, laps, "run", "Run", 0);
    QVERIFY(rows[1].bestOfRun);
    QVERIFY(!rows[2].bestOfRun);
    QVERIFY(!rows[3].bestOfRun);
}

void LapEligibilityTests::rendererOmitsDeltaAtMissingCurrentCoordinate()
{
    auto session = VboParser::parse(QString::fromUtf8(EventProjectFixture::lapsVbo()));
    session.channels[session.aliases.value("latitude")].values[11] = std::numeric_limits<float>::quiet_NaN();
    const auto laps = deriveSourceLapSession(session);
    TelemetryRenderContext context;
    context.setSession(&session);
    context.setLapSession(laps);
    context.setTime(12.0);
    const auto state = context.lapTiming();
    QCOMPARE(state.value("bestLapNumber").toInt(), 1);
    QVERIFY(!state.contains("liveDeltaSeconds"));
    QVERIFY(!state.contains("referenceSpeedKmh"));
}

QTEST_GUILESS_MAIN(LapEligibilityTests)
#include "LapEligibilityTests.moc"
