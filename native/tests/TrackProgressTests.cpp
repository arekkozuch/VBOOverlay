// Distance-into-lap parameterization (native/src/telemetry/LapDistance.*),
// used by the A/B lap comparison overlay to line up two laps' channels by
// meters travelled instead of raw time or lap-time fraction. This is
// deliberately simpler than the shared cross-lap track-progress alignment
// axis (KAN-31/32/33) still to land here: each lap is parameterized against
// its own trace only, with no attempt to project one lap onto another's
// reference line.

#include "EventProjectFixture.h"
#include "telemetry/LapDistance.h"
#include "telemetry/VboParser.h"

#include <QtTest>
#include <cmath>
#include <numbers>

using namespace FlappedEar;

namespace {

// A synthetic east-only trace so hand-computed distances are exact. Point 2's
// longitude is deliberately NaN to model a GPS outage: latitude still has a
// finite sample there (so it stays part of the same sampledSegments run), but
// the missing longitude must stop that one sample from contributing.
TelemetrySession eastTrace()
{
    TelemetrySession session;
    session.duration = 3;
    session.aliases = {{"latitude", "lat"}, {"longitude", "lon"}};
    session.channels.insert("lat", {});
    session.channels.insert("lon", {});
    auto &lat = session.channels["lat"];
    auto &lon = session.channels["lon"];
    // Exact longitude delta for a given east offset in meters, matching
    // projectCoordinate()'s own eastMeters = dLonRadians * R * cos(latitude).
    constexpr double earthRadiusMeters = 6'371'000.0;
    constexpr double latitudeDegrees = 52.0;
    const auto degreesEast = [](const double meters) {
        return meters / (earthRadiusMeters * std::cos(latitudeDegrees * std::numbers::pi / 180.0))
            * 180.0 / std::numbers::pi;
    };
    lat.timestamps = {0, 1, 2, 3};
    lat.values = {52.0f, 52.0f, 52.0f, 52.0f};
    lon.timestamps = {0, 1, 2, 3};
    lon.values = {21.0f, static_cast<float>(21.0 + degreesEast(10)),
        std::numeric_limits<float>::quiet_NaN(), static_cast<float>(21.0 + degreesEast(30))};
    return session;
}

}

class TrackProgressTests final : public QObject {
    Q_OBJECT
private slots:
    void accumulatesDistanceAlongTheTrace();
    void gapDoesNotBridgeDistance();
    void timeAtDistanceInterpolatesAndClamps();
    void invalidProfileReturnsNullopt();
    void realisticRouteProducesAPlausibleLapDistance();
};

void TrackProgressTests::accumulatesDistanceAlongTheTrace()
{
    // Drop the NaN sample so this case is a plain, uninterrupted 0/10/30m walk.
    auto session = eastTrace();
    session.channels["lon"].values[2] = session.channels["lon"].values[1];
    const auto profile = buildLapDistanceProfile(session, 0, 3);
    QVERIFY(profile.valid);
    QCOMPARE(profile.times.size(), profile.distances.size());
    for (qsizetype i = 1; i < profile.distances.size(); ++i)
        QVERIFY(profile.distances[i] >= profile.distances[i - 1]);
    QCOMPARE(profile.distances.first(), 0.0);
    QVERIFY(std::abs(profile.distances[1] - 10.0) < 0.5);
    QVERIFY(std::abs(profile.totalMeters - 30.0) < 0.5);
}

void TrackProgressTests::gapDoesNotBridgeDistance()
{
    const auto session = eastTrace();
    const auto profile = buildLapDistanceProfile(session, 0, 3);
    QVERIFY(profile.valid);
    // The NaN-longitude sample at t=2 must be dropped entirely, not counted
    // and not used to bridge a straight line to the next valid fix at t=3.
    QCOMPARE(profile.times, (QVector<double>{0, 1, 3}));
    QVERIFY(std::abs(profile.distances[0] - 0.0) < 0.5);
    QVERIFY(std::abs(profile.distances[1] - 10.0) < 0.5);
    // Distance freezes across the gap: t=3 must not add the 20m from the
    // (unknown) path between the last known fix and this one.
    QCOMPARE(profile.distances[1], profile.distances[2]);
    QVERIFY(std::abs(profile.totalMeters - 10.0) < 0.5);
}

void TrackProgressTests::timeAtDistanceInterpolatesAndClamps()
{
    auto session = eastTrace();
    session.channels["lon"].values[2] = session.channels["lon"].values[1];
    const auto profile = buildLapDistanceProfile(session, 0, 3);
    QVERIFY(profile.valid);
    const auto atStart = timeAtDistance(profile, 0);
    QVERIFY(atStart); QCOMPARE(*atStart, 0.0);
    const auto atEnd = timeAtDistance(profile, profile.totalMeters);
    QVERIFY(atEnd); QCOMPARE(*atEnd, 3.0);
    const auto atHalf = timeAtDistance(profile, profile.totalMeters / 2);
    QVERIFY(atHalf); QVERIFY(*atHalf > 0.0 && *atHalf < 3.0);
    // Out-of-range distances clamp to the nearest end rather than failing.
    const auto negative = timeAtDistance(profile, -50);
    QVERIFY(negative); QCOMPARE(*negative, 0.0);
    const auto beyond = timeAtDistance(profile, profile.totalMeters + 1000);
    QVERIFY(beyond); QCOMPARE(*beyond, 3.0);
}

void TrackProgressTests::invalidProfileReturnsNullopt()
{
    const LapDistanceProfile empty;
    QVERIFY(!empty.valid);
    QVERIFY(!timeAtDistance(empty, 10).has_value());

    TelemetrySession noGps;
    noGps.duration = 10;
    QVERIFY(!buildLapDistanceProfile(noGps, 0, 10).valid);
    QVERIFY(!buildLapDistanceProfile(noGps, 10, 0).valid); // end <= start is invalid
}

void TrackProgressTests::realisticRouteProducesAPlausibleLapDistance()
{
    const auto session = VboParser::parse(QString::fromUtf8(EventProjectFixture::routeVbo()));
    QVERIFY(session.duration > 0);
    // routeVbo() lays out `turns` laps of 48s each at the default radiusX=300.
    const auto profile = buildLapDistanceProfile(session, 0, 48);
    QVERIFY(profile.valid);
    // A ~300x180m-radius ellipse has a circumference on the order of 1.5km;
    // generous bounds guard against a unit/scale regression, not exact shape.
    QVERIFY(profile.totalMeters > 500 && profile.totalMeters < 4000);
    for (qsizetype i = 1; i < profile.distances.size(); ++i)
        QVERIFY(profile.distances[i] >= profile.distances[i - 1]);
}

QTEST_GUILESS_MAIN(TrackProgressTests)
#include "TrackProgressTests.moc"
