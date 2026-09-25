// Time-loss observations between two laps (native/src/telemetry/TimeLoss.*,
// KAN-59): one window per approved segment, corner continuations, signed
// increments separate from the running delta, and no interval counted twice.

#include "telemetry/TimeLoss.h"
#include "telemetry/TrackSegments.h"

#include <QtTest>
#include <cmath>
#include <functional>

using namespace FlappedEar;

namespace {

constexpr double lapLength = 1000.0;
constexpr double lapStart = 10.0;

QString configuration() { return "compatibility-v1:" + QString(64, 'a'); }

// Projection samples every 10 m from 2 m to 998 m at the given elapsed time,
// split at an optional hole.
QVector<ProgressSegment> projectedLap(const std::function<double(double)> &elapsedAt,
    const double holeFrom = -1.0, const double holeTo = -1.0)
{
    QVector<ProgressSegment> lap(1);
    for (double progress = 2.0; progress <= 998.0 + 1e-9; progress += 10.0) {
        if (progress > holeFrom && progress < holeTo) {
            if (!lap.last().samples.isEmpty()) lap.append(ProgressSegment{});
            continue;
        }
        lap.last().samples.append({lapStart + elapsedAt(progress), progress, true});
    }
    if (lap.last().samples.isEmpty()) lap.removeLast();
    return lap;
}

struct Bound { TrackSegmentType type; double start; double end; };

ApprovedSegmentation approvedOf(const QVector<Bound> &bounds)
{
    QJsonArray segments;
    int number = 1;
    for (const auto &bound : bounds)
        segments.append(makeTrackSegment(bound.type, QString("W%1").arg(number++), bound.start, bound.end, configuration()));
    return approvedSegmentation(segments, configuration());
}

LapSectorTimes timesOf(const ApprovedSegmentation &approved, const std::function<double(double)> &elapsedAt,
    const double holeFrom = -1.0, const double holeTo = -1.0)
{
    return computeLapSectorTimes(approved, lapLength, projectedLap(elapsedAt, holeFrom, holeTo),
        lapStart, lapStart + elapsedAt(lapLength), QJsonObject{{"lap", 1}});
}

const auto constantSpeed = [](const double metersPerSecond) {
    return [metersPerSecond](const double progress) { return progress / metersPerSecond; };
};

// 25 m/s up to 500 m, then 50/3 m/s: 20 s + 30 s = 50 s, the same lap time
// as 20 m/s throughout, but quicker early and slower late.
double quickThenSlow(const double progress)
{
    return progress <= 500.0 ? progress / 25.0 : 20.0 + (progress - 500.0) * 3.0 / 50.0;
}

bool near(const std::optional<double> value, const double expected, const double tolerance = 1e-6)
{
    return value && std::abs(*value - expected) <= tolerance;
}

} // namespace

class TimeLossTests final : public QObject {
    Q_OBJECT
private slots:
    void incrementsTileTheLapDeltaWithContinuations();
    void separatesSignedIncrementFromCumulativeDelta();
    void leavesGappedWindowUntimedWithoutBridging();
    void rejectsLapsFromAnotherRevision();
    void ranksLossesAcrossLapsAgainstReference();
};

void TimeLossTests::incrementsTileTheLapDeltaWithContinuations()
{
    // Straight [0,300] follows the corner ending at the gate (1000 m): a continuation.
    const auto approved = approvedOf({{TrackSegmentType::Straight, 0, 300}, {TrackSegmentType::Corner, 300, 700},
        {TrackSegmentType::Straight, 700, 1000}});
    const auto withGateCorner = approvedOf({{TrackSegmentType::Straight, 0, 300}, {TrackSegmentType::Sector, 300, 900},
        {TrackSegmentType::Corner, 900, 1000}});
    const auto a = timesOf(approved, constantSpeed(20.0)); // 50 s
    const auto b = timesOf(approved, constantSpeed(25.0)); // 40 s
    const auto result = computeTimeLossObservations(approved, lapLength, a, lapStart, b, lapStart);
    QVERIFY(result.valid);
    QCOMPARE(result.stamp.calculationAlgorithm, QString(timeLossAlgorithm));
    QCOMPARE(result.windows.size(), 3);
    QCOMPARE(result.windows[0].role, QString(timeLossRoleStraight)); // no corner ends at 0 here
    QCOMPARE(result.windows[1].role, QString(timeLossRoleCorner));
    QCOMPARE(result.windows[2].role, QString(timeLossRoleContinuation));
    QCOMPARE(result.windows[2].cornerSegmentId, result.windows[1].segmentId);
    QVERIFY(near(result.windows[0].incrementSeconds, 15.0 - 12.0));
    QVERIFY(near(result.windows[1].incrementSeconds, 20.0 - 16.0));
    QVERIFY(near(result.windows[2].incrementSeconds, 15.0 - 12.0));
    // Each stretch counted once: increments sum to the lap-time difference.
    QVERIFY(result.allWindowsTimed);
    QVERIFY(std::abs(result.timedIncrementSumSeconds - 10.0) < 1e-6);
    // Running delta is continuous across adjacent windows and starts at 0.
    QVERIFY(near(result.windows[0].cumulativeAtStartSeconds, 0.0));
    for (qsizetype i = 0; i + 1 < result.windows.size(); ++i)
        QVERIFY(near(result.windows[i + 1].cumulativeAtStartSeconds, *result.windows[i].cumulativeAtEndSeconds));
    QVERIFY(near(result.windows.last().cumulativeAtEndSeconds, 10.0));

    // A corner ending at the gate continues onto the straight starting at it.
    const auto gate = computeTimeLossObservations(withGateCorner, lapLength, timesOf(withGateCorner, constantSpeed(20.0)),
        lapStart, timesOf(withGateCorner, constantSpeed(25.0)), lapStart);
    QCOMPARE(gate.windows[0].role, QString(timeLossRoleContinuation));
    QCOMPARE(gate.windows[0].cornerSegmentId, gate.windows[2].segmentId);
    QCOMPARE(gate.windows[1].role, QString(timeLossRoleSector));
}

void TimeLossTests::separatesSignedIncrementFromCumulativeDelta()
{
    const auto approved = approvedOf({{TrackSegmentType::Corner, 0, 300}, {TrackSegmentType::Straight, 300, 700},
        {TrackSegmentType::Corner, 700, 900}, {TrackSegmentType::Straight, 900, 1000}});
    // B is quick early and slow late; both laps take 50 s.
    const auto result = computeTimeLossObservations(approved, lapLength,
        timesOf(approved, constantSpeed(20.0)), lapStart, timesOf(approved, quickThenSlow), lapStart);
    QVERIFY(result.valid && result.allWindowsTimed);
    QVERIFY(near(result.windows[0].incrementSeconds, 15.0 - 12.0)); // A loses 3 s
    QVERIFY(near(result.windows[1].incrementSeconds, 20.0 - 20.0));
    QCOMPARE(result.windows[1].role, QString(timeLossRoleContinuation));
    // A is 3 s behind entering the second corner, yet gains 2 s through it.
    QVERIFY(near(result.windows[2].cumulativeAtStartSeconds, 3.0));
    QVERIFY(near(result.windows[2].incrementSeconds, 10.0 - 12.0));
    QVERIFY(near(result.windows[2].cumulativeAtEndSeconds, 1.0));
    QVERIFY(near(result.windows[3].incrementSeconds, 5.0 - 6.0));
    QVERIFY(std::abs(result.timedIncrementSumSeconds) < 1e-6);
    for (const auto &window : result.windows)
        QVERIFY(near(window.incrementSeconds, *window.cumulativeAtEndSeconds - *window.cumulativeAtStartSeconds));
}

void TimeLossTests::leavesGappedWindowUntimedWithoutBridging()
{
    const auto approved = approvedOf({{TrackSegmentType::Corner, 0, 300}, {TrackSegmentType::Straight, 300, 700},
        {TrackSegmentType::Corner, 700, 1000}});
    const auto a = timesOf(approved, constantSpeed(20.0), 400.0, 500.0); // hole inside the straight
    const auto b = timesOf(approved, constantSpeed(25.0));
    const auto result = computeTimeLossObservations(approved, lapLength, a, lapStart, b, lapStart);
    QVERIFY(result.valid);
    QVERIFY(!result.allWindowsTimed);
    QVERIFY(result.windows[0].incrementSeconds);
    QVERIFY(!result.windows[1].incrementSeconds);
    QCOMPARE(result.windows[1].unavailableReason, QString(timeLossUntimed));
    QVERIFY(result.windows[2].incrementSeconds);
    // Only timed windows are summed; the hole is not bridged into a total.
    QVERIFY(std::abs(result.timedIncrementSumSeconds - (3.0 + 3.0)) < 1e-6);
}

void TimeLossTests::rejectsLapsFromAnotherRevision()
{
    const auto approved = approvedOf({{TrackSegmentType::Corner, 0, 500}, {TrackSegmentType::Straight, 500, 1000}});
    const auto other = approvedOf({{TrackSegmentType::Corner, 0, 400}, {TrackSegmentType::Straight, 400, 1000}});
    const auto result = computeTimeLossObservations(approved, lapLength, timesOf(approved, constantSpeed(20.0)),
        lapStart, timesOf(other, constantSpeed(25.0)), lapStart);
    QVERIFY(!result.valid);
    QCOMPARE(result.unavailableReason, QString(timeLossDifferentSegmentOrRevision));
    QVERIFY(!computeTimeLossObservations({}, lapLength, {}, lapStart, {}, lapStart).valid);
}

void TimeLossTests::ranksLossesAcrossLapsAgainstReference()
{
    const auto approved = approvedOf({{TrackSegmentType::Corner, 0, 300}, {TrackSegmentType::Straight, 300, 700},
        {TrackSegmentType::Corner, 700, 1000}});
    const auto lap = [&](const std::function<double(double)> &elapsedAt, const int number,
                         const double holeFrom = -1.0, const double holeTo = -1.0) {
        auto times = computeLapSectorTimes(approved, lapLength, projectedLap(elapsedAt, holeFrom, holeTo),
            lapStart, lapStart + elapsedAt(lapLength), QJsonObject{{"lap", number}, {"startTime", lapStart}});
        return TimedLapSectors{times, lapStart};
    };
    const auto reference = lap(constantSpeed(25.0), 0);   // 12 / 16 / 12 s
    const auto slow = lap(constantSpeed(20.0), 1);        // 15 / 20 / 15 s: +3, +4, +3
    const auto mixed = lap(quickThenSlow, 2);             // 12 / 20 / 18 s: 0, +4, +6
    const auto gapped = lap(constantSpeed(20.0), 3, 400.0, 500.0); // straight untimed
    const auto ranking = rankTimeLosses(approved, lapLength, {reference, slow, mixed, gapped}, reference);
    QVERIFY(ranking.valid);
    QCOMPARE(ranking.referenceLap, reference.times.lapReference);
    QCOMPARE(ranking.comparedLapCount, 3); // the reference lap itself is skipped
    QCOMPARE(ranking.untimedWindowCount, 1);
    // slow: 3; mixed: 2 (its first corner ties, a zero increment is not a loss); gapped: 2.
    QCOMPARE(ranking.observationCount, 7);
    QVERIFY(std::abs(ranking.losses[0].lossSeconds - 6.0) < 1e-6);
    QCOMPARE(ranking.losses[0].lapReference.value("lap").toInt(), 2);
    QCOMPARE(ranking.losses[0].window.role, QString(timeLossRoleCorner));
    for (qsizetype i = 1; i < ranking.losses.size(); ++i)
        QVERIFY(ranking.losses[i - 1].lossSeconds >= ranking.losses[i].lossSeconds);
    for (const auto &loss : ranking.losses) {
        QVERIFY(loss.lossSeconds > 0.0);
        QVERIFY(std::abs(loss.coverageLap - 1.0) < 0.05 && std::abs(loss.coverageReference - 1.0) < 0.05);
    }
    // Equal losses are ordered by track position.
    QVERIFY(std::abs(ranking.losses[1].lossSeconds - 4.0) < 1e-6 && std::abs(ranking.losses[2].lossSeconds - 4.0) < 1e-6);
    QVERIFY(ranking.losses[1].window.startProgressMeters <= ranking.losses[2].window.startProgressMeters);

    const auto truncated = rankTimeLosses(approved, lapLength, {slow, mixed}, reference, 2);
    QCOMPARE(truncated.losses.size(), 2);
    QCOMPARE(truncated.observationCount, 5);
    QVERIFY(!rankTimeLosses(approved, lapLength, {slow}, TimedLapSectors{}).valid);
}

QTEST_GUILESS_MAIN(TimeLossTests)
#include "TimeLossTests.moc"
