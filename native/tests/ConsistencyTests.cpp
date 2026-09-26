// Timing consistency (native/src/telemetry/Consistency.*, KAN-62): median and
// interquartile range with a documented minimum population.

#include "telemetry/Consistency.h"
#include "telemetry/TrackSegments.h"

#include <QtTest>
#include <cmath>
#include <limits>

using namespace FlappedEar;

namespace {

QString configuration() { return "compatibility-v1:" + QString(64, 'a'); }

ApprovedSegmentation approvedOf(const QVector<QPair<double, double>> &bounds)
{
    QJsonArray segments;
    int number = 1;
    for (const auto &[start, end] : bounds)
        segments.append(makeTrackSegment(TrackSegmentType::Sector, QString("S%1").arg(number++), start, end, configuration()));
    return approvedSegmentation(segments, configuration());
}

TimedLapSectors lapWith(const ApprovedSegmentation &approved, const QVector<std::optional<double>> &seconds,
    const int number, const bool current = true)
{
    LapSectorTimes times;
    times.valid = true;
    times.stamp = segmentationResultStamp(approved, sectorTimingAlgorithm);
    if (!current) times.stamp.revision = "other";
    times.lapReference = QJsonObject{{"lap", number}};
    for (int index = 0; index < approved.segments.size(); ++index) {
        SectorTime sector;
        sector.segmentId = approved.segments[index].toObject().value("id").toString();
        if (index < seconds.size()) sector.seconds = seconds[index];
        times.sectors.append(sector);
    }
    return {times, 0.0};
}

bool near(const std::optional<double> value, const double expected)
{
    return value && std::abs(*value - expected) < 1e-9;
}

} // namespace

class ConsistencyTests final : public QObject {
    Q_OBJECT
private slots:
    void summarizesMedianAndInterquartileRange();
    void withholdsTooSmallPopulations();
    void spreadIgnoresASingleSlowLap();
    void summarizesEachSectorFromTimedLapsOnly();
};

void ConsistencyTests::summarizesMedianAndInterquartileRange()
{
    // Sorted 10, 20, 30, 40, 1000: q1 at position 1 (20), median at 2 (30), q3 at 3 (40).
    const auto summary = summarizeConsistency({30, 10, 1000, 20, 40});
    QVERIFY(summary.available);
    QCOMPARE(summary.count, 5);
    QVERIFY(near(summary.minimum, 10) && near(summary.q1, 20) && near(summary.median, 30));
    QVERIFY(near(summary.q3, 40) && near(summary.maximum, 1000));
    QVERIFY(near(summary.interquartileRange, 20));
    // Interpolated between samples: 4 values, q1 at position 0.75.
    const auto even = summarizeConsistency({1, 2, 3, 4});
    QVERIFY(near(even.q1, 1.75) && near(even.median, 2.5) && near(even.q3, 3.25) && near(even.interquartileRange, 1.5));
}

void ConsistencyTests::withholdsTooSmallPopulations()
{
    const auto two = summarizeConsistency({10, 11});
    QVERIFY(!two.available);
    QCOMPARE(two.count, 2);
    QCOMPARE(two.unavailableReason, QString(consistencyTooFewSamples));
    QVERIFY(!two.median && !two.interquartileRange);
    // Non-finite samples do not count toward the minimum.
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();
    QVERIFY(!summarizeConsistency({10, nan, inf, 11}).available);
    QVERIFY(summarizeConsistency({10, 11, 12}).available);
    QVERIFY(!summarizeConsistency({}).available);
}

void ConsistencyTests::spreadIgnoresASingleSlowLap()
{
    // Nine laps within 0.4 s and one warm-up lap 25 s slower.
    const auto summary = summarizeConsistency({110.0, 110.1, 110.2, 110.0, 110.3, 110.1, 110.4, 110.2, 110.1, 135.0});
    QVERIFY(summary.available);
    QVERIFY(*summary.interquartileRange < 0.25);
    QVERIFY(near(summary.maximum, 135.0));
}

void ConsistencyTests::summarizesEachSectorFromTimedLapsOnly()
{
    const auto approved = approvedOf({{0, 400}, {400, 1000}});
    const QVector<TimedLapSectors> population{
        lapWith(approved, {10.0, 20.0}, 1), lapWith(approved, {11.0, std::nullopt}, 2),
        lapWith(approved, {12.0, 21.0}, 3), lapWith(approved, {13.0, 22.0}, 4),
        lapWith(approved, {1.0, 1.0}, 5, /*current=*/false)};
    const auto sectors = computeSectorConsistency(approved, population);
    QCOMPARE(sectors.size(), 2);
    QCOMPARE(sectors[0].summary.count, 4); // the other revision is ignored
    QVERIFY(near(sectors[0].summary.median, 11.5));
    QCOMPARE(sectors[0].lapReferences.size(), 4);
    QCOMPARE(sectors[1].summary.count, 3); // lap 2 has no time in the second sector
    QVERIFY(near(sectors[1].summary.median, 21.0));
    QVERIFY(near(sectors[1].summary.interquartileRange, 1.0));
    QVERIFY(!sectors[1].lapReferences.contains(QJsonObject{{"lap", 2}}));
    QVERIFY(computeSectorConsistency({}, population).isEmpty());
}

QTEST_GUILESS_MAIN(ConsistencyTests)
#include "ConsistencyTests.moc"
