// Sector times per lap (native/src/telemetry/SectorTiming.*, KAN-51): interpolated
// boundary crossings on shared progress, coverage, the approved revision, and
// sums that match the lap time for a complete partition.

#include "telemetry/SectorTiming.h"
#include "telemetry/TrackSegments.h"

#include <QtTest>
#include <cmath>

using namespace FlappedEar;

namespace {

constexpr double lapLength = 1000.0;
constexpr double lapStart = 10.0;
constexpr double lapEnd = 60.0; // 20 m/s, so progress p is crossed at 10 + p / 20

QString configuration() { return "compatibility-v1:" + QString(64, 'a'); }

// Projection samples every 0.5 s (10 m) from 2 m to 998 m, split at any hole.
QVector<ProgressSegment> projectedLap(const double holeFrom = -1.0, const double holeTo = -1.0)
{
    QVector<ProgressSegment> lap(1);
    for (double progress = 2.0; progress <= 998.0 + 1e-9; progress += 10.0) {
        if (progress > holeFrom && progress < holeTo) {
            if (!lap.last().samples.isEmpty()) lap.append(ProgressSegment{});
            continue;
        }
        lap.last().samples.append({lapStart + progress / 20.0, progress, true});
    }
    if (lap.last().samples.isEmpty()) lap.removeLast();
    return lap;
}

ApprovedSegmentation approvedOf(const QVector<QPair<double, double>> &bounds)
{
    QJsonArray segments;
    int number = 1;
    for (const auto &[start, end] : bounds)
        segments.append(makeTrackSegment(TrackSegmentType::Sector, QString("S%1").arg(number++), start, end, configuration()));
    return approvedSegmentation(segments, configuration());
}

const QJsonObject reference{{"lap", 3}};

} // namespace

class SectorTimingTests final : public QObject {
    Q_OBJECT
private slots:
    void completePartitionSumsToTheLapTime();
    void gapsLeaveOnlyTheAffectedSectorUntimed();
    void gateCrossingAndGappedPartitionsAreNotComplete();
    void rejectsInvalidInputs();
    void comparesSectorTimesBetweenTwoLaps();
};

void SectorTimingTests::completePartitionSumsToTheLapTime()
{
    const auto approved = approvedOf({{0.0, 305.0}, {305.0, 700.0}, {700.0, lapLength}});
    const auto times = computeLapSectorTimes(approved, lapLength, projectedLap(), lapStart, lapEnd, reference);
    QVERIFY(times.valid);
    QVERIFY(times.lapReference == reference);
    QCOMPARE(times.stamp.revision, approved.revision);
    QCOMPARE(times.stamp.calculationAlgorithm, QString(sectorTimingAlgorithm));
    QVERIFY(segmentationResultCurrent(times.stamp, approved, sectorTimingAlgorithm));
    QVERIFY(times.completePartition);
    QCOMPARE(times.sectors.size(), 3);
    QCOMPARE(times.sectors[0].segmentId, approved.segments[0].toObject().value("id").toString());

    // 305 m lies between samples (302 m at 25.1 s, 312 m at 25.6 s): interpolated.
    QVERIFY(times.sectors[0].seconds);
    QVERIFY(std::abs(*times.sectors[0].seconds - 15.25) < 1e-9);
    QVERIFY(std::abs(*times.sectors[1].seconds - 19.75) < 1e-9);
    QVERIFY(std::abs(*times.sectors[2].seconds - 15.0) < 1e-9);
    QCOMPARE(*times.sectors[0].startTime, lapStart); // gate boundaries use the lap's own timing
    QCOMPARE(*times.sectors[2].endTime, lapEnd);
    for (const auto &sector : times.sectors) {
        QVERIFY(sector.unavailableReason.isEmpty());
        QVERIFY(std::abs(sector.coveredMeters - sector.lengthMeters) < 1e-9);
    }
    QVERIFY(times.sumSeconds);
    QCOMPARE(times.lapSeconds, lapEnd - lapStart);
    QVERIFY(*times.partitionErrorSeconds <= sectorSumToleranceSeconds);
}

void SectorTimingTests::gapsLeaveOnlyTheAffectedSectorUntimed()
{
    const auto approved = approvedOf({{0.0, 305.0}, {305.0, 700.0}, {700.0, lapLength}});
    const auto times = computeLapSectorTimes(approved, lapLength, projectedLap(400.0, 500.0), lapStart, lapEnd, reference);
    QVERIFY(times.valid);
    QVERIFY(times.completePartition);
    QVERIFY(times.sectors[0].seconds);
    QVERIFY(!times.sectors[1].seconds); // never bridged across the hole
    QCOMPARE(times.sectors[1].unavailableReason, QString(sectorIncompleteCoverage));
    QVERIFY(times.sectors[1].coveredMeters < times.sectors[1].lengthMeters - 50.0);
    QVERIFY(times.sectors[2].seconds);
    QVERIFY(!times.sumSeconds); // an incomplete lap has no sum to compare
    QVERIFY(!times.partitionErrorSeconds);

    // A sector whose boundary itself falls in the hole has no crossing time.
    const auto boundaryInHole = approvedOf({{0.0, 450.0}, {450.0, lapLength}});
    const auto split = computeLapSectorTimes(boundaryInHole, lapLength, projectedLap(400.0, 500.0), lapStart, lapEnd, reference);
    QVERIFY(!split.sectors[0].seconds);
    QVERIFY(!split.sectors[0].endTime);
    QVERIFY(!split.sectors[1].seconds);
}

void SectorTimingTests::gateCrossingAndGappedPartitionsAreNotComplete()
{
    const auto wrapping = approvedOf({{100.0, 900.0}, {900.0, 100.0}});
    auto times = computeLapSectorTimes(wrapping, lapLength, projectedLap(), lapStart, lapEnd, reference);
    QVERIFY(times.valid);
    QVERIFY(!times.completePartition);
    QVERIFY(times.sectors[0].seconds);
    QVERIFY(std::abs(*times.sectors[0].seconds - 40.0) < 1e-9);
    QVERIFY(!times.sectors[1].seconds);
    QCOMPARE(times.sectors[1].unavailableReason, QString(sectorCrossesGate));
    QVERIFY(std::abs(times.sectors[1].lengthMeters - 200.0) < 1e-9);
    QVERIFY(!times.sumSeconds);

    const auto gapped = approvedOf({{0.0, 300.0}, {400.0, lapLength}});
    times = computeLapSectorTimes(gapped, lapLength, projectedLap(), lapStart, lapEnd, reference);
    QVERIFY(!times.completePartition);
    QVERIFY(times.sectors[0].seconds && times.sectors[1].seconds);
    QVERIFY(!times.sumSeconds);

    const auto none = computeLapSectorTimes(approvedOf({}), lapLength, projectedLap(), lapStart, lapEnd, reference);
    QVERIFY(none.valid);
    QVERIFY(none.sectors.isEmpty());
    QVERIFY(!none.completePartition);
    QVERIFY(!segmentationResultCurrent(none.stamp, approvedOf({}), sectorTimingAlgorithm)); // no revision, never current
}

void SectorTimingTests::rejectsInvalidInputs()
{
    const auto approved = approvedOf({{0.0, lapLength}});
    QVERIFY(computeLapSectorTimes(approved, lapLength, projectedLap(), lapStart, lapEnd, reference).valid);
    QVERIFY(!computeLapSectorTimes(approved, lapLength, projectedLap(), lapEnd, lapStart, reference).valid);
    QVERIFY(!computeLapSectorTimes(approved, 0.0, projectedLap(), lapStart, lapEnd, reference).valid);
    QVERIFY(!computeLapSectorTimes(approvedSegmentation(QJsonValue("bad"), configuration()), lapLength, projectedLap(),
        lapStart, lapEnd, reference).valid);
    // No projection at all: nothing is timed, even the full-lap sector.
    const auto empty = computeLapSectorTimes(approved, lapLength, {}, lapStart, lapEnd, reference);
    QVERIFY(empty.valid);
    QVERIFY(!empty.sectors[0].seconds);
}

void SectorTimingTests::comparesSectorTimesBetweenTwoLaps()
{
    // KAN-55: A minus B, from two laps' whole-lap results for the same
    // approved revision. Lap A is the existing 20 m/s fixture (50 s lap); lap
    // B is a faster, independently-projected 25 m/s lap (40 s), so the delta
    // per sector is exactly predictable.
    const auto approved = approvedOf({{0.0, 305.0}, {305.0, 700.0}, {700.0, lapLength}});
    const auto timesA = computeLapSectorTimes(approved, lapLength, projectedLap(), lapStart, lapEnd, reference);
    QVERIFY(timesA.valid && timesA.completePartition);

    constexpr double lapEndB = lapStart + lapLength / 25.0; // 25 m/s
    QVector<ProgressSegment> lapB(1);
    for (double progress = 2.0; progress <= 998.0 + 1e-9; progress += 10.0)
        lapB.last().samples.append({lapStart + progress / 25.0, progress, true});
    const auto timesB = computeLapSectorTimes(approved, lapLength, lapB, lapStart, lapEndB, reference);
    QVERIFY(timesB.valid && timesB.completePartition);

    const auto id = timesA.sectors[0].segmentId;
    const auto comparison = compareSectorTimes(timesA, timesB, id);
    QVERIFY(comparison.valid);
    QVERIFY(comparison.unavailableReason.isEmpty());
    QVERIFY(comparison.secondsDelta.has_value());
    QVERIFY2(std::abs(*comparison.secondsDelta - (*timesA.sectors[0].seconds - *timesB.sectors[0].seconds)) < 1e-9,
        qPrintable(QString("delta=%1").arg(*comparison.secondsDelta)));
    QVERIFY(*comparison.secondsDelta > 0.0); // A (slower, 20 m/s) took longer than B (25 m/s)

    // A different approved revision (one more sector) is never compared.
    const auto differentRevision = approvedOf({{0.0, 300.0}, {300.0, 305.0}, {305.0, 700.0}, {700.0, lapLength}});
    const auto timesC = computeLapSectorTimes(differentRevision, lapLength, lapB, lapStart, lapEndB, reference);
    const auto mismatched = compareSectorTimes(timesA, timesC, id);
    QVERIFY(!mismatched.valid);
    QCOMPARE(mismatched.unavailableReason, QString(sectorTimeDifferentSegmentOrRevision));

    // A segment id absent from the (same-revision) result is never fabricated.
    QVERIFY(!compareSectorTimes(timesA, timesB, "not-a-real-id").valid);
    QCOMPARE(compareSectorTimes(timesA, timesB, "not-a-real-id").unavailableReason, QString(sectorTimeSegmentNotFound));

    // Invalid input on either side withholds the whole comparison.
    QVERIFY(!compareSectorTimes(LapSectorTimes{}, timesB, id).valid);
}

QTEST_GUILESS_MAIN(SectorTimingTests)
#include "SectorTimingTests.moc"
