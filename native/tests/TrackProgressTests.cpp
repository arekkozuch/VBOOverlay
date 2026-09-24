// The shared cross-lap track-progress alignment axis
// (native/src/telemetry/TrackProgress.*, KAN-31/32/33/34): projecting two
// independently recorded laps onto one common, gate-anchored, crossing-safe
// arc-length axis so they can be compared corner-for-corner even on
// different racing lines.

#include "EventProjectFixture.h"
#include "telemetry/LapTiming.h"
#include "telemetry/TrackProgress.h"
#include "telemetry/VboParser.h"

#include <QtTest>
#include <cmath>
#include <numbers>

using namespace FlappedEar;

namespace {

constexpr double earthRadiusMeters = 6'371'000.0;
// At the equator (origin latitude 0), east and north both convert the same
// way, so a single helper suffices for both axes of a hand-built local shape.
double degreesForMeters(const double meters)
{
    return meters / earthRadiusMeters * 180.0 / std::numbers::pi;
}

// A synthetic axis wide enough to be legal (buildProgressAxis requires
// length in [50, 30000]) shaped like two long, close, opposite-direction
// straights joined by short connectors -- deliberately similar to a hairpin
// or a start/finish straight with a return road nearby: exactly the shape
// nearest-GPS-point matching alone cannot resolve (KAN-32). Point (0,0) is
// the trace's first point, and the timing gate is placed there too, so
// progress runs 0 (outbound start) -> 100 (outbound end) -> ~104 (connector)
// -> ~154 (return end) -> ~208 back to 0.
QVector<LapTracePoint> hairpinTracePoints()
{
    QVector<LapTracePoint> points;
    double time = 0;
    const auto add = [&](const double east, const double north) { points.append({time, east, north}); time += 1.0; };
    for (int i = 0; i <= 50; ++i) add(100.0 * i / 50.0, 0.0); // outbound leg: (0,0) -> (100,0)
    for (int i = 1; i <= 4; ++i) add(100.0, 4.0 * i / 4.0); // connector up
    for (int i = 1; i <= 50; ++i) add(100.0 - 100.0 * i / 50.0, 4.0); // return leg: (100,4) -> (0,4), opposite direction
    for (int i = 1; i < 4; ++i) add(0.0, 4.0 - 4.0 * i / 4.0); // connector down, back toward (0,0)
    return points;
}

ProgressAxis buildHairpinAxis()
{
    LapTrace trace;
    trace.points = hairpinTracePoints();
    const GeoCoordinate origin{0.0, 0.0};
    const TimingGate gate{TimingGateType::Start, "test", origin, origin, {}};
    return buildProgressAxis(trace, origin, gate);
}

// A GPS session that walks the outbound leg of the hairpin shape, hits one
// wildly-off-track outlier, then (after a real time gap) resumes further
// along the same leg.
TelemetrySession hairpinGpsSessionWithOutlierAndGap()
{
    TelemetrySession session;
    session.aliases = {{"latitude", "lat"}, {"longitude", "lon"}};
    session.channels.insert("lat", {});
    session.channels.insert("lon", {});
    auto &lat = session.channels["lat"];
    auto &lon = session.channels["lon"];
    const auto addFix = [&](const double time, const double east, const double north) {
        lat.timestamps.append(time);
        lat.values.append(static_cast<float>(degreesForMeters(north)));
        lon.timestamps.append(time);
        lon.values.append(static_cast<float>(degreesForMeters(east)));
    };
    for (int i = 0; i <= 20; ++i) addFix(i * 1.0, i * 2.0, 0.0); // t=0..20s, east 0..40m, steady 2 m/s
    addFix(21.0, 10.0, 50.0); // one sample far off either leg: a GPS outlier
    for (int i = 0; i <= 20; ++i) addFix(30.0 + i, 40.0 + i * 2.0, 0.0); // t=30..50s, east 40..80m, after a real gap
    session.duration = 50.0;
    return session;
}

struct RouteFixture {
    TelemetrySession session;
    LapSession laps;
    ProgressAxis axis;
};

// A real (synthetic-GPS) closed circuit, run through the actual lap-timing
// and route machinery, not a hand-built shape -- exercises buildProgressAxis
// against the same LapTrace production code produces.
RouteFixture buildRouteFixture(const int turns = 2)
{
    RouteFixture fixture;
    fixture.session = VboParser::parse(QString::fromUtf8(EventProjectFixture::routeVbo(240, -1.0, 0, false, 300, turns)));
    fixture.laps = deriveSourceLapSession(fixture.session);
    if (!fixture.laps.selectedStartGate || fixture.laps.lapTraces.isEmpty()) return fixture;
    const auto &gate = *fixture.laps.selectedStartGate;
    const GeoCoordinate origin{
        (gate.endpointA.latitudeDegrees + gate.endpointB.latitudeDegrees) / 2,
        (gate.endpointA.longitudeDegrees + gate.endpointB.longitudeDegrees) / 2};
    fixture.axis = buildProgressAxis(fixture.laps.lapTraces.first(), origin, gate);
    return fixture;
}

} // namespace

class TrackProgressTests final : public QObject {
    Q_OBJECT
private slots:
    void buildsAxisAnchoredAtTheGate();
    void identicalLapsProduceZeroDelta();
    void knownDelayHasCorrectSignAndFinishLineMagnitude();
    void ambiguousEquidistantCrossingIsRejected();
    void headingRejectsOppositeDirectionParallelSection();
    void outlierAndGapBreakSegmentsWithoutBridging();
    void deltaSeriesOnlyCoversSharedValidRange();
};

void TrackProgressTests::buildsAxisAnchoredAtTheGate()
{
    const auto fixture = buildRouteFixture();
    QVERIFY2(fixture.laps.selectedStartGate.has_value(), "fixture must resolve a start gate");
    QVERIFY2(!fixture.laps.lapTraces.isEmpty(), "fixture must produce at least one lap trace");
    QVERIFY(fixture.axis.valid);
    QVERIFY(fixture.axis.points.size() >= 32);
    QCOMPARE(fixture.axis.cumulative.size(), fixture.axis.points.size());
    QCOMPARE(fixture.axis.cumulative.first(), 0.0);
    // Evenly arc-spaced by construction.
    QVERIFY(std::abs(fixture.axis.spacingMeters - fixture.axis.lengthMeters / fixture.axis.points.size()) < 1e-6);
    // Progress must stay within [0, lengthMeters) and start right at the gate.
    const GeoCoordinate origin = fixture.axis.origin;
    const auto &gate = *fixture.laps.selectedStartGate;
    const GeoCoordinate gateMidpoint{
        (gate.endpointA.latitudeDegrees + gate.endpointB.latitudeDegrees) / 2,
        (gate.endpointA.longitudeDegrees + gate.endpointB.longitudeDegrees) / 2};
    const MetricPoint gateLocal = projectCoordinate(gateMidpoint, origin);
    const double distanceFromGateToAxisStart = std::hypot(
        fixture.axis.points.first().x() - gateLocal.eastMeters, fixture.axis.points.first().y() - gateLocal.northMeters);
    QVERIFY(distanceFromGateToAxisStart < fixture.axis.spacingMeters * 2);
}

void TrackProgressTests::identicalLapsProduceZeroDelta()
{
    const auto fixture = buildRouteFixture();
    QVERIFY(fixture.axis.valid);
    QVERIFY(!fixture.laps.timedLaps.isEmpty());
    const auto &lap = fixture.laps.timedLaps.first();
    const auto trace = projectLapTrace(fixture.axis, fixture.session, lap.startTelemetryTime, lap.endTelemetryTime);
    QVERIFY(!trace.isEmpty());
    qsizetype covered = 0;
    for (const auto &segment : trace) covered += segment.samples.size();
    QVERIFY2(covered > 60, "the synthetic lap should mostly lock on");

    const auto delta = computeDeltaSeries(trace, trace, 20.0);
    QVERIFY(!delta.isEmpty());
    for (const auto &segment : delta)
        for (const auto &point : segment)
            QVERIFY(std::abs(point.deltaSeconds) < 0.05);
}

void TrackProgressTests::knownDelayHasCorrectSignAndFinishLineMagnitude()
{
    const auto fixture = buildRouteFixture();
    QVERIFY(fixture.axis.valid);
    QVERIFY(!fixture.laps.timedLaps.isEmpty());
    const auto &lap = fixture.laps.timedLaps.first();
    const auto lapA = projectLapTrace(fixture.axis, fixture.session, lap.startTelemetryTime, lap.endTelemetryTime);
    QVERIFY(!lapA.isEmpty());

    // Lap B: the identical physical path, uniformly 10% slower.
    constexpr double slowdownFactor = 1.10;
    TelemetrySession sessionB = fixture.session;
    for (auto &channel : sessionB.channels)
        for (auto &timestamp : channel.timestamps) timestamp *= slowdownFactor;
    sessionB.duration *= slowdownFactor;
    const auto lapB = projectLapTrace(
        fixture.axis, sessionB, lap.startTelemetryTime * slowdownFactor, lap.endTelemetryTime * slowdownFactor);
    QVERIFY(!lapB.isEmpty());

    const auto delta = computeDeltaSeries(lapA, lapB, 20.0);
    QVERIFY(!delta.isEmpty());
    double lastDelta = 0.0;
    bool sawClearlyAhead = false;
    for (const auto &segment : delta) {
        for (const auto &point : segment) {
            // A is faster throughout (B is uniformly slower): A must never
            // read as meaningfully behind.
            QVERIFY(point.deltaSeconds < 0.5);
            if (point.deltaSeconds < -0.2) sawClearlyAhead = true;
            lastDelta = point.deltaSeconds;
        }
    }
    QVERIFY2(sawClearlyAhead, "A should read as ahead of the 10%-slower B somewhere along the lap");
    const double actualDurationDifference = lap.durationSeconds - lap.durationSeconds * slowdownFactor; // A - B
    QVERIFY2(std::abs(lastDelta - actualDurationDifference) < 1.0,
        "the delta near the finish line should approximate the true lap-time difference");
}

void TrackProgressTests::ambiguousEquidistantCrossingIsRejected()
{
    const auto axis = buildHairpinAxis();
    QVERIFY(axis.valid);
    ProjectionContext context;
    context.hasLock = true;
    // Near the apex (progress ~100, where the outbound leg ends and the
    // close-by return leg begins a few meters later in progress) rather than
    // the middle of the straights: that is where the two branches are close
    // in both space AND progress, so a bounded search window actually sees
    // both -- the real shape of a hairpin/parallel-section ambiguity.
    context.lastProgressMeters = 90.0;
    context.lastTelemetryTime = 0.0;

    // Exactly between the outbound leg (north=0) and the return leg
    // (north=4) right at the apex: a pure nearest-point match cannot tell
    // them apart.
    const QPointF equidistant(98.0, 2.0);
    const QPointF movingOutbound(1.0, 0.0);
    const auto result = projectSample(axis, equidistant, 0.5, 20.0, movingOutbound, context);
    QVERIFY2(!result.valid, "an equidistant crossing must return unavailable alignment, not a guess");
    // Rejection must not corrupt the lock for the next, unambiguous sample.
    QVERIFY(context.hasLock);
    QCOMPARE(context.lastProgressMeters, 90.0);
}

void TrackProgressTests::headingRejectsOppositeDirectionParallelSection()
{
    const auto axis = buildHairpinAxis();
    QVERIFY(axis.valid);
    ProjectionContext context;
    context.hasLock = true;
    context.lastProgressMeters = 90.0; // approaching the apex along the outbound leg
    context.lastTelemetryTime = 0.0;
    const QPointF movingOutbound(1.0, 0.0);

    // Closer to the return leg (0.5m) than the outbound leg (3.5m) by pure
    // distance -- an ambiguity-gap check alone would accept this as the
    // return leg. But the car is moving in the outbound leg's direction, and
    // the return leg runs the opposite way there: sample order/direction
    // (KAN-32) must reject it rather than jump to the wrong branch.
    const QPointF closerToReturnLeg(98.0, 3.5);
    const auto rejected = projectSample(axis, closerToReturnLeg, 0.5, 20.0, movingOutbound, context);
    QVERIFY2(!rejected.valid, "nearest-point distance alone must not override a direction conflict");

    // The same context, given a point that is actually on the outbound leg,
    // still locks on normally -- the heading check does not just reject
    // everything near the ambiguous zone.
    const QPointF onOutboundLeg(98.0, 0.2);
    const auto accepted = projectSample(axis, onOutboundLeg, 0.5, 20.0, movingOutbound, context);
    QVERIFY(accepted.valid);
    QVERIFY(accepted.progressMeters > 93.0 && accepted.progressMeters < 100.0);
}

void TrackProgressTests::outlierAndGapBreakSegmentsWithoutBridging()
{
    const auto axis = buildHairpinAxis();
    QVERIFY(axis.valid);
    const auto session = hairpinGpsSessionWithOutlierAndGap();
    const auto trace = projectLapTrace(axis, session, 0, session.duration);

    QVERIFY2(trace.size() >= 2, "the outlier and the real time gap must each break the segment");
    qsizetype totalSamples = 0;
    for (const auto &segment : trace) {
        totalSamples += segment.samples.size();
        // No segment may contain a meaningful backward jump: continuity is
        // enforced per-segment, and a break must never bridge a gap by
        // interpolating across it.
        for (qsizetype i = 1; i < segment.samples.size(); ++i)
            QVERIFY(segment.samples[i].progressMeters >= segment.samples[i - 1].progressMeters - 3.0);
    }
    // 21 samples before the outlier, 21 after the gap; the outlier itself
    // (and any sample it could have contaminated) must not appear.
    QVERIFY2(totalSamples <= 42, "the off-track outlier must not be counted as a locked sample");
    QVERIFY2(totalSamples >= 30, "most of the on-track samples should still lock");
}

void TrackProgressTests::deltaSeriesOnlyCoversSharedValidRange()
{
    QVector<ProgressSegment> lapA(1);
    lapA[0].samples = {{0.0, 0.0, true}, {10.0, 100.0, true}}; // covers progress [0,100] over 10s
    QVector<ProgressSegment> lapB(1);
    lapB[0].samples = {{5.0, 50.0, true}, {15.0, 150.0, true}}; // covers progress [50,150] over 10s

    const auto delta = computeDeltaSeries(lapA, lapB, 10.0);
    QVERIFY(!delta.isEmpty());
    bool sawNearFifty = false, sawNearHundred = false;
    for (const auto &segment : delta) {
        for (const auto &point : segment) {
            // Only the overlap [50,100] is covered by both laps; nothing
            // outside it may appear rather than extrapolating a guess.
            QVERIFY(point.progressMeters >= 50.0 - 1e-6 && point.progressMeters <= 100.0 + 1e-6);
            if (point.progressMeters <= 51.0) sawNearFifty = true;
            if (point.progressMeters >= 99.0) sawNearHundred = true;
        }
    }
    QVERIFY(sawNearFifty);
    QVERIFY(sawNearHundred);
}

QTEST_GUILESS_MAIN(TrackProgressTests)
#include "TrackProgressTests.moc"
