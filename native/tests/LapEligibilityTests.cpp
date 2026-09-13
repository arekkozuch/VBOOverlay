#include "EventProjectFixture.h"
#include "telemetry/LapTiming.h"
#include "telemetry/OutingLaps.h"
#include "telemetry/TelemetryRenderContext.h"
#include "telemetry/VboParser.h"

#include <QtTest>
#include <limits>
#include <algorithm>

using namespace FlappedEar;

namespace {
TelemetrySession interiorGapFixture()
{
    auto session = VboParser::parse(QString::fromUtf8(EventProjectFixture::lapsVbo()));
    // Keep valid GPS before and after each injected interior defect. In the
    // sparse fixture every point also arms or finalizes a gate passage, so
    // removing one tests passage rejection rather than interior lap coverage.
    for (auto &channel : session.channels) {
        const auto times = channel.timestamps;
        const auto values = channel.values;
        channel.timestamps.clear();
        channel.values.clear();
        channel.cadenceStatisticsValid = false;
        for (qsizetype index = 1; index < times.size(); ++index) {
            for (double time = times[index - 1]; time < times[index]; time += 0.25) {
                const double fraction = (time - times[index - 1]) / (times[index] - times[index - 1]);
                channel.timestamps.append(time);
                channel.values.append(static_cast<float>(values[index - 1]
                    + (values[index] - values[index - 1]) * fraction));
            }
        }
        channel.timestamps.append(times.last());
        channel.values.append(values.last());
    }
    return session;
}
}

class LapEligibilityTests final : public QObject {
    Q_OBJECT
private slots:
    void ranksCompatibleLapsDeterministically();
    void rankingReportsAllAppliedExclusions();
    void separatesCompatibilityGroups_data();
    void separatesCompatibilityGroups();
    void preservesIndependentCompatibilityReasons();
    void preservesTimingWhenDensifyingFixture();
    void retainsMeasuredLapWithInteriorGpsGap();
    void rejectsInvalidAndUnpairedCoordinates_data();
    void rejectsInvalidAndUnpairedCoordinates();
    void exposesNoBestWhenEveryLapHasMissingGps();
    void rendererNeverUsesIneligibleCompletedLap();
    void propagatesOutingEligibilityAndBest();
    void preservesNormalLapRanking();
    void rendererOmitsDeltaAtMissingCurrentCoordinate();
};

void LapEligibilityTests::ranksCompatibleLapsDeterministically()
{
    const QJsonObject config{{"layoutId", "Full"}, {"direction", "clockwise"}, {"gateRevision", "gates-v1:" + QString(64, 'a')}};
    auto opposite = config; opposite.insert("direction", "counterclockwise");
    QHash<QString, QJsonObject> configs{{"a", config}, {"b", config}, {"c", opposite}, {"unknown", {}}};
    const auto make = [](QString run, double start, double duration, int number, std::optional<qint64> clock) {
        OutingLapRow row; row.runId = run; row.runName = run; row.type = LapSectionType::Lap;
        row.start = start; row.end = start + duration; row.lapNumber = number; row.timestampMilliseconds = clock;
        row.referenceEligible = true;
        row.reference = makeLapReference(row, "event", run + "-source", QByteArray(64, 'a'), QByteArray(64, 'b'));
        return row;
    };
    auto a = make("a", 20, 5, 2, 2000), b = make("b", 10, 5, 1, 1000);
    const auto fasterOther = make("c", 0, 1, 1, 0), fasterUnknown = make("unknown", 0, .5, 1, 0);
    QVector<OutingLapRow> rows{a, b, fasterOther, fasterUnknown};
    const auto group = lapCompatibilityGroupId(config);
    const auto ranked = rankOutingLaps(rows, group, configs, {});
    QCOMPARE(ranked.value("state").toString(), QString("available"));
    QCOMPARE(ranked.value("eligibleLapCount").toInt(), 2);
    QCOMPARE(ranked.value("tieCount").toInt(), 2);
    QCOMPARE(ranked.value("bestOfDay").toObject().value("reference").toObject(), b.reference);
    QCOMPARE(ranked.value("runs").toArray().first().toObject().value("runId").toString(), QString("b"));
    std::reverse(rows.begin(), rows.end());
    QCOMPARE(rankOutingLaps(rows, group, configs, {}), ranked);
    // With equal or absent clocks, stable run identity wins, not input order/name/number.
    a.timestampMilliseconds.reset(); b.timestampMilliseconds.reset();
    a.runName = "Zulu"; b.runName = "Alpha"; a.lapNumber = 99; b.lapNumber = 1;
    QCOMPARE(rankOutingLaps({b, a}, group, configs, {}).value("bestOfDay").toObject().value("reference").toObject(), a.reference);
    a.timestampMilliseconds = 3000;
    QCOMPARE(rankOutingLaps({b, a}, group, configs, {}).value("bestOfDay").toObject().value("reference").toObject(), a.reference);
    // Within one run, the earlier telemetry bounds win an otherwise exact tie.
    auto earlier = make("a", 0, 5, 77, {}); a.timestampMilliseconds.reset();
    QCOMPARE(rankOutingLaps({a, earlier}, group, configs, {}).value("bestOfDay").toObject().value("reference").toObject(), earlier.reference);
    QVERIFY(rankOutingLaps(rows, "", configs, {}).value("bestOfDay").isNull());
    QCOMPARE(rankOutingLaps(rows, "", configs, {}).value("state").toString(), QString("selection-required"));
}

void LapEligibilityTests::rankingReportsAllAppliedExclusions()
{
    const QJsonObject config{{"layoutId", "Full"}, {"direction", "clockwise"}, {"gateRevision", "gates-v1:" + QString(64, 'a')}};
    const auto group = lapCompatibilityGroupId(config);
    QHash<QString, QJsonObject> configs{{"a", config}, {"b", config}};
    OutingLapRow a; a.runId = "a"; a.runName = "Morning"; a.type = LapSectionType::Lap;
    a.start = 0; a.end = 4; a.lapNumber = 1; a.referenceEligible = true;
    a.reference = makeLapReference(a, "event", "a-source", QByteArray(64, 'a'), QByteArray(64, 'b'));
    auto b = a; b.runId = "b"; b.runName = "Afternoon"; b.end = 5;
    b.reference = makeLapReference(b, "event", "b-source", QByteArray(64, 'a'), QByteArray(64, 'b'));
    const QJsonArray exclusions{QJsonObject{{"reference", a.reference}, {"reason", "Traffic"}}};
    a.referenceIssue = LapReferenceIssue::GpsGap; a.referenceEligible = false;
    auto ranked = rankOutingLaps({a, b}, group, configs, exclusions);
    QCOMPARE(ranked.value("bestOfDay").toObject().value("reference").toObject(), b.reference);
    QCOMPARE(ranked.value("lapCount").toInt(), 2); QCOMPARE(ranked.value("eligibleLapCount").toInt(), 1);
    const auto rejected = ranked.value("excludedLaps").toArray().first().toObject();
    QCOMPARE(rejected.value("reasons").toArray(), (QJsonArray{"incomplete-gps", "user-exclusion"}));
    QCOMPARE(rejected.value("userReason").toString(), QString("Traffic"));
    QCOMPARE(ranked.value("runs").toArray().last().toObject().value("state").toString(), QString("no-eligible-laps"));
    ranked = rankOutingLaps({a, b}, group, configs, exclusions, {"b"});
    QCOMPARE(ranked.value("state").toString(), QString("no-eligible-laps"));
    QVERIFY(ranked.value("bestOfDay").isNull()); QCOMPARE(ranked.value("eligibleLapCount").toInt(), 0);
    QCOMPARE(ranked.value("excludedLaps").toArray().size(), 2);
    for (const auto &run : ranked.value("runs").toArray()) QVERIFY(run.toObject().value("bestLap").isNull());
    b.reference = {}; ranked = rankOutingLaps({b}, group, configs, {});
    QVERIFY(ranked.value("bestOfDay").isNull());
    QVERIFY(ranked.value("excludedLaps").toArray().first().toObject().value("reasons").toArray().contains("invalid-reference"));
    QVector<OutingLapRow> tooMany(maximumOutingLapRows + 1);
    QVERIFY_THROWS_EXCEPTION(ResourceLimitError, static_cast<void>(rankOutingLaps(tooMany, group, configs, {})));
}

void LapEligibilityTests::separatesCompatibilityGroups_data()
{
    QTest::addColumn<QString>("field"); QTest::addColumn<QJsonValue>("value");
    QTest::addColumn<QString>("reason"); QTest::addColumn<bool>("resolved");
    QTest::newRow("different-layout") << QString("layoutId") << QJsonValue("Short") << QString("changed-layout") << true;
    QTest::newRow("opposite-direction") << QString("direction") << QJsonValue("counterclockwise") << QString("opposite-direction") << true;
    QTest::newRow("different-gates") << QString("gateRevision") << QJsonValue("gates-v1:" + QString(64, 'b')) << QString("changed-timing-gate") << true;
    QTest::newRow("unknown-layout") << QString("layoutId") << QJsonValue(QJsonValue::Null) << QString("layout-unresolved") << false;
    QTest::newRow("blank-layout") << QString("layoutId") << QJsonValue(" ") << QString("layout-unresolved") << false;
    QTest::newRow("unknown-direction") << QString("direction") << QJsonValue("unknown") << QString("direction-unresolved") << false;
    QTest::newRow("unknown-gates") << QString("gateRevision") << QJsonValue(QJsonValue::Null) << QString("timing-gate-unresolved") << false;
    QTest::newRow("malformed-gates") << QString("gateRevision") << QJsonValue("gates-v1:bad") << QString("timing-gate-unresolved") << false;
}

void LapEligibilityTests::separatesCompatibilityGroups()
{
    QFETCH(QString, field); QFETCH(QJsonValue, value); QFETCH(QString, reason); QFETCH(bool, resolved);
    const QJsonObject reference{{"layoutId", "Full"}, {"direction", "clockwise"}, {"gateRevision", "gates-v1:" + QString(64, 'a')}};
    auto changed = reference; changed.insert(field, value);
    QVERIFY(!lapCompatibilityGroupId(reference).isEmpty());
    QVERIFY(lapCompatibilityGroupId(changed) != lapCompatibilityGroupId(reference));
    QCOMPARE(!lapCompatibilityGroupId(changed).isEmpty(), resolved);
    QCOMPARE(lapCompatibilityReasons(changed, reference), QStringList{reason});
    QCOMPARE(lapCompatibilityReasons(reference, changed), QStringList{reason});
    if (!resolved) QVERIFY(!lapCompatibilityReasons(changed, changed).isEmpty());
}

void LapEligibilityTests::preservesIndependentCompatibilityReasons()
{
    const QJsonObject reference{{"layoutId", "Full"}, {"direction", "clockwise"}, {"gateRevision", "gates-v1:" + QString(64, 'a')}};
    auto anotherSource = reference; anotherSource.insert("sourceId", "another"); anotherSource.insert("sourceFingerprint", QJsonObject{{"changed", true}});
    QCOMPARE(lapCompatibilityGroupId(reference), lapCompatibilityGroupId(anotherSource));
    QVERIFY(lapCompatibilityReasons(reference, anotherSource).isEmpty());
    const QJsonObject changed{{"layoutId", "Short"}, {"direction", "counterclockwise"}, {"gateRevision", "gates-v1:" + QString(64, 'b')}};
    QCOMPARE(lapCompatibilityReasons(changed, reference, LapReferenceIssue::GpsGap, true),
        (QStringList{"changed-layout", "opposite-direction", "changed-timing-gate", "incomplete-gps", "user-exclusion"}));
    QCOMPARE(lapCompatibilityReasons(reference, reference, LapReferenceIssue::InvalidGps, true),
        (QStringList{"invalid-gps", "user-exclusion"}));
    QVERIFY(lapCompatibilityGroupId({}).isEmpty());
    QCOMPARE(lapCompatibilityReasons({}, {}), (QStringList{"layout-unresolved", "direction-unresolved", "timing-gate-unresolved"}));
}

void LapEligibilityTests::preservesTimingWhenDensifyingFixture()
{
    const auto sparse = deriveSourceLapSession(
        VboParser::parse(QString::fromUtf8(EventProjectFixture::lapsVbo())));
    const auto dense = deriveSourceLapSession(interiorGapFixture());
    QCOMPARE(dense.acceptedPasses.size(), sparse.acceptedPasses.size());
    QCOMPARE(dense.timedLaps.size(), 3);
    QCOMPARE(dense.lapTraces.size(), 3);
    for (qsizetype index = 0; index < sparse.timedLaps.size(); ++index) {
        QVERIFY(qAbs(dense.timedLaps[index].startTelemetryTime
                     - sparse.timedLaps[index].startTelemetryTime) < 0.001);
        QVERIFY(qAbs(dense.timedLaps[index].endTelemetryTime
                     - sparse.timedLaps[index].endTelemetryTime) < 0.001);
        QVERIFY(dense.timedLaps[index].referenceEligible());
    }
}

void LapEligibilityTests::retainsMeasuredLapWithInteriorGpsGap()
{
    auto session = interiorGapFixture();
    for (const auto &name : {QStringLiteral("latitude"), QStringLiteral("longitude")}) {
        auto &channel = session.channels[session.aliases.value(name)];
        for (auto &time : channel.timestamps) if (time >= 3.5) time += 10.0;
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
    auto session = interiorGapFixture();
    auto &latitude = session.channels[session.aliases.value("latitude")];
    auto &longitude = session.channels[session.aliases.value("longitude")];
    const auto interior = latitude.timestamps.indexOf(3.5);
    QVERIFY(interior >= 0);
    if (kind == 0) latitude.values[interior] = std::numeric_limits<float>::quiet_NaN();
    if (kind == 1) latitude.values[interior] = 100.0F;
    if (kind == 2) longitude.timestamps[interior] += 0.01;
    const auto laps = deriveSourceLapSession(session);
    QCOMPARE(laps.timedLaps.size(), 3);
    QCOMPARE(laps.timedLaps[0].referenceIssue, LapReferenceIssue::InvalidGps);
    QCOMPARE(laps.lapTraces.size(), 2);
    QVERIFY(laps.fastestLapIndex.has_value());
    QVERIFY(*laps.fastestLapIndex != 0);
}

void LapEligibilityTests::exposesNoBestWhenEveryLapHasMissingGps()
{
    auto session = interiorGapFixture();
    auto &latitude = session.channels[session.aliases.value("latitude")];
    for (double time : {3.5, 7.5, 12.5}) {
        const auto index = latitude.timestamps.indexOf(time);
        QVERIFY(index >= 0);
        latitude.values[index] = std::numeric_limits<float>::quiet_NaN();
    }
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
    auto session = interiorGapFixture();
    auto &latitude = session.channels[session.aliases.value("latitude")];
    const auto interior = latitude.timestamps.indexOf(3.5);
    QVERIFY(interior >= 0);
    latitude.values[interior] = std::numeric_limits<float>::quiet_NaN();
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
    auto session = interiorGapFixture();
    auto &latitude = session.channels[session.aliases.value("latitude")];
    const auto interior = latitude.timestamps.indexOf(3.5);
    QVERIFY(interior >= 0);
    latitude.values[interior] = std::numeric_limits<float>::quiet_NaN();
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
    auto session = interiorGapFixture();
    auto &latitude = session.channels[session.aliases.value("latitude")];
    const auto interior = latitude.timestamps.indexOf(12.5);
    QVERIFY(interior >= 0);
    latitude.values[interior] = std::numeric_limits<float>::quiet_NaN();
    const auto laps = deriveSourceLapSession(session);
    TelemetryRenderContext context;
    context.setSession(&session);
    context.setLapSession(laps);
    context.setTime(12.5);
    const auto state = context.lapTiming();
    QCOMPARE(state.value("bestLapNumber").toInt(), 1);
    QVERIFY(!state.contains("liveDeltaSeconds"));
    QVERIFY(!state.contains("referenceSpeedKmh"));
}

QTEST_GUILESS_MAIN(LapEligibilityTests)
#include "LapEligibilityTests.moc"
