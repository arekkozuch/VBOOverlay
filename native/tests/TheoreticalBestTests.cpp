// Theoretical best across a population of laps (native/src/telemetry/
// TheoreticalBest.*, KAN-56): the fastest valid time per approved sector,
// with its source lap, and aggregate suppression when a sector has no
// coverage anywhere in the population.

#include "telemetry/TheoreticalBest.h"
#include "telemetry/TrackSegments.h"

#include <QtTest>
#include <cmath>

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

// A lap whose sector times are exactly `secondsPerSector`, tagged with the
// given lap reference and (by default) the current approved revision.
LapSectorTimes lapWith(const ApprovedSegmentation &approved, const QVector<double> &secondsPerSector,
    const QJsonObject &reference, const bool current = true)
{
    LapSectorTimes times;
    times.valid = true;
    times.stamp = segmentationResultStamp(approved, sectorTimingAlgorithm);
    if (!current) times.stamp.revision = "different-revision";
    times.lapReference = reference;
    for (int index = 0; index < approved.segments.size(); ++index) {
        const auto segment = approved.segments[index].toObject();
        SectorTime sector;
        sector.segmentId = segment.value("id").toString();
        sector.name = segment.value("name").toString();
        if (index < secondsPerSector.size()) sector.seconds = secondsPerSector[index];
        else sector.unavailableReason = sectorIncompleteCoverage;
        times.sectors.append(sector);
    }
    return times;
}

} // namespace

class TheoreticalBestTests final : public QObject {
    Q_OBJECT
private slots:
    void selectsFastestSectorAcrossPopulationWithSource();
    void suppressesTotalButKeepsSectorsWhenOneIsUncovered();
    void ignoresLapsFromADifferentRevision();
    void withholdsEverythingWithoutApprovedSegmentation();
};

void TheoreticalBestTests::selectsFastestSectorAcrossPopulationWithSource()
{
    const auto approved = approvedOf({{0.0, 300.0}, {300.0, 700.0}, {700.0, 1000.0}});
    const QJsonObject refA{{"lap", 1}}, refB{{"lap", 2}}, refC{{"lap", 3}};
    // A is fastest in sector 1, B fastest in sector 2, C fastest in sector 3.
    const auto a = lapWith(approved, {10.0, 25.0, 20.0}, refA);
    const auto b = lapWith(approved, {15.0, 18.0, 22.0}, refB);
    const auto c = lapWith(approved, {14.0, 20.0, 12.0}, refC);
    const auto best = computeTheoreticalBest(approved, {a, b, c});
    QVERIFY(best.valid);
    QVERIFY(best.unavailableReason.isEmpty());
    QCOMPARE(best.sectors.size(), 3);
    QVERIFY(best.sectors[0].seconds.has_value());
    QCOMPARE(*best.sectors[0].seconds, 10.0);
    QCOMPARE(best.sectors[0].sourceLapReference, refA);
    QCOMPARE(*best.sectors[1].seconds, 18.0);
    QCOMPARE(best.sectors[1].sourceLapReference, refB);
    QCOMPARE(*best.sectors[2].seconds, 12.0);
    QCOMPARE(best.sectors[2].sourceLapReference, refC);
    QVERIFY(best.totalSeconds.has_value());
    QVERIFY(std::abs(*best.totalSeconds - 40.0) < 1e-9);
    QCOMPARE(best.stamp.calculationAlgorithm, QString(theoreticalBestAlgorithm));
}

void TheoreticalBestTests::suppressesTotalButKeepsSectorsWhenOneIsUncovered()
{
    const auto approved = approvedOf({{0.0, 300.0}, {300.0, 700.0}, {700.0, 1000.0}});
    const QJsonObject refA{{"lap", 1}};
    // Only sector 1 and 3 are ever timed; sector 2 has no valid time anywhere.
    const auto a = lapWith(approved, {10.0}, refA);
    LapSectorTimes third = a; third.sectors[2].seconds = 12.0; third.sectors[1].seconds.reset();
    const auto best = computeTheoreticalBest(approved, {a, third});
    QVERIFY(best.valid);
    QCOMPARE(best.unavailableReason, QString(theoreticalBestIncompleteCoverage));
    QVERIFY(!best.totalSeconds.has_value());
    QVERIFY(best.sectors[0].seconds.has_value());
    QVERIFY(!best.sectors[1].seconds.has_value());
    QCOMPARE(best.sectors[1].unavailableReason, QString(theoreticalBestIncompleteCoverage));
    QVERIFY(best.sectors[2].seconds.has_value());
}

void TheoreticalBestTests::ignoresLapsFromADifferentRevision()
{
    const auto approved = approvedOf({{0.0, 1000.0}});
    const QJsonObject refFast{{"lap", 1}}, refCurrent{{"lap", 2}};
    // The faster lap is tagged with a stale/different revision and must never win.
    const auto stale = lapWith(approved, {1.0}, refFast, /*current=*/false);
    const auto current = lapWith(approved, {50.0}, refCurrent);
    const auto best = computeTheoreticalBest(approved, {stale, current});
    QVERIFY(best.sectors[0].seconds.has_value());
    QCOMPARE(*best.sectors[0].seconds, 50.0);
    QCOMPARE(best.sectors[0].sourceLapReference, refCurrent);
}

void TheoreticalBestTests::withholdsEverythingWithoutApprovedSegmentation()
{
    const auto best = computeTheoreticalBest({}, {});
    QVERIFY(!best.valid);
    QCOMPARE(best.unavailableReason, QString(theoreticalBestNoApprovedSegmentation));
    QVERIFY(best.sectors.isEmpty());
    QVERIFY(!best.totalSeconds.has_value());
}

QTEST_GUILESS_MAIN(TheoreticalBestTests)
#include "TheoreticalBestTests.moc"
