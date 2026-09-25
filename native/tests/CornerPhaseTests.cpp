// Corner entry/apex/exit and per-lap minimum speed (native/src/telemetry/CornerPhases.*, KAN-46):
// geometric phases come from the shared axis; the minimum-speed location is
// measured per lap and must never be conflated with the apex.

#include "SyntheticLoopFixture.h"
#include "telemetry/CornerPhases.h"
#include "telemetry/TrackSegmentProposals.h"

#include <QtTest>
#include <cmath>
#include <functional>
#include <limits>
#include <numbers>

using namespace FlappedEar;
using namespace SyntheticLoop;

namespace {

constexpr double earthRadiusMeters = 6'371'000.0;
double degreesForMeters(const double meters) { return meters / earthRadiusMeters * 180.0 / std::numbers::pi; }

// Tight 60-degree arc between two gentle ones: one short high-curvature region.
QVector<Step> singleApexHalf()
{
    return {straight(100), arc(60, 50), arc(60, 12), arc(60, 50), straight(100)};
}

// Two tight 45-degree arcs joined by a long gentle one: two separate regions.
QVector<Step> doubleApexHalf()
{
    return {straight(100), arc(45, 12), arc(90, 60), arc(45, 12), straight(100)};
}

struct Corner {
    ProgressAxis axis;
    TrackFeatures features;
    TrackSegmentProposal proposal;
};

Corner firstCorner(const QVector<Step> &half)
{
    Corner corner;
    corner.axis = buildLoopAxis(half);
    corner.features = computeTrackFeatures(corner.axis, 6.0);
    const auto proposals = proposeTrackSegments(corner.axis, corner.features);
    for (const auto &proposal : proposals.proposals) {
        if (proposal.type == TrackSegmentType::Corner) { corner.proposal = proposal; break; }
    }
    return corner;
}

struct Lap {
    TelemetrySession session;
    double endTime = 0.0;
};

// Drives one lap of the loop at 20 Hz, gate to gate, with speed v(s) m/s as
// a function of path distance s. Fixes whose s falls in `gpsGap` are dropped;
// speed samples whose s falls in `speedGap` are NaN (no data).
Lap driveLap(const QVector<Step> &half, const std::function<double(double)> &speed,
    const QPair<double, double> gpsGap = {-1.0, -1.0}, const QPair<double, double> speedGap = {-1.0, -1.0})
{
    auto points = loopPoints(half);
    points.append(points.first());
    QVector<double> cumulative{0.0};
    for (qsizetype i = 1; i < points.size(); ++i)
        cumulative.append(cumulative.last() + std::hypot(points[i].x() - points[i - 1].x(), points[i].y() - points[i - 1].y()));
    const double length = cumulative.last();

    Lap lap;
    auto &session = lap.session;
    session.aliases = {{"latitude", "lat"}, {"longitude", "lon"}, {"speed", "velocity"}};
    session.channels.insert("lat", {});
    session.channels.insert("lon", {});
    TelemetryChannel speedChannel;
    speedChannel.name = "velocity";
    speedChannel.unit = "km/h";
    session.channels.insert("velocity", speedChannel);
    auto &lat = session.channels["lat"];
    auto &lon = session.channels["lon"];
    auto &velocity = session.channels["velocity"];

    constexpr double dt = 0.05;
    double s = 0.0;
    double time = 0.0;
    qsizetype segment = 1;
    while (s < length) {
        while (segment + 1 < cumulative.size() && cumulative[segment] < s) ++segment;
        const double fraction = (s - cumulative[segment - 1]) / (cumulative[segment] - cumulative[segment - 1]);
        const QPointF position = points[segment - 1] + (points[segment] - points[segment - 1]) * fraction;
        if (!(s >= gpsGap.first && s <= gpsGap.second)) {
            lat.timestamps.append(time);
            lat.values.append(static_cast<float>(degreesForMeters(position.y())));
            lon.timestamps.append(time);
            lon.values.append(static_cast<float>(degreesForMeters(position.x())));
        }
        const bool speedMissing = s >= speedGap.first && s <= speedGap.second;
        velocity.timestamps.append(time);
        velocity.values.append(speedMissing ? std::numeric_limits<float>::quiet_NaN()
                                            : static_cast<float>(speed(s) * 3.6));
        s += speed(s) * dt;
        time += dt;
    }
    session.duration = time;
    lap.endTime = time - dt;
    return lap;
}

// Minimum 12 m/s at path distance 185 m, 30 m/s elsewhere.
double speedDipAt185(const double s) { return 30.0 - 18.0 * std::exp(-std::pow((s - 185.0) / 15.0, 2.0)); }

} // namespace

class CornerPhaseTests final : public QObject {
    Q_OBJECT
private slots:
    void entryAndExitReuseCornerBoundaries();
    void placesASingleApexAtTheHighCurvatureRegion();
    void leavesMultipleApexesUnresolved();
    void widensABroadConstantRadiusApex();
    void locatesMinimumSpeedSeparatelyFromTheApex();
    void leavesMinimumSpeedUnresolvedOnIncompleteData();
    void rejectsInvalidInputs();
};

void CornerPhaseTests::entryAndExitReuseCornerBoundaries()
{
    const auto corner = firstCorner(singleApexHalf());
    QVERIFY(corner.proposal.type == TrackSegmentType::Corner);
    const auto phases = proposeCornerGeometryPhases(corner.axis, corner.features, corner.proposal);
    QVERIFY(phases.valid);
    QCOMPARE(phases.entry.method, QString(cornerPhaseCurvatureOnset));
    QCOMPARE(phases.entry.progressMeters, corner.proposal.start.progressMeters);
    QCOMPARE(phases.entry.toleranceMeters, corner.proposal.start.toleranceMeters);
    QCOMPARE(phases.exit.method, QString(cornerPhaseCurvatureRelease));
    QCOMPARE(phases.exit.progressMeters, corner.proposal.end.progressMeters);
    QVERIFY(phases.entry.resolved());
    QVERIFY(phases.exit.resolved());
    QCOMPARE(phases.entry.evidence.value("source").toString(), QString(trackSegmentProposalAlgorithm));
}

void CornerPhaseTests::placesASingleApexAtTheHighCurvatureRegion()
{
    const auto corner = firstCorner(singleApexHalf());
    const auto phases = proposeCornerGeometryPhases(corner.axis, corner.features, corner.proposal);
    QVERIFY(phases.valid);
    QVERIFY(phases.apex.resolved());
    QCOMPARE(phases.apex.method, QString(cornerPhasePeakCurvature));
    QVERIFY(phases.apex.uncertaintyReasons.isEmpty());
    QCOMPARE(phases.apexCandidatesMeters.size(), 1);
    // The tight arc spans path distance ~152.4..165.0 m; its centre is ~158.7 m.
    QVERIFY2(std::abs(phases.apex.progressMeters - 158.7) <= phases.apex.toleranceMeters,
        qPrintable(QString("apex at %1 m").arg(phases.apex.progressMeters)));
    QVERIFY(phases.apex.evidence.value("peakCurvaturePerMeter").toDouble() > 1.0 / 20.0);
}

void CornerPhaseTests::leavesMultipleApexesUnresolved()
{
    const auto corner = firstCorner(doubleApexHalf());
    const auto phases = proposeCornerGeometryPhases(corner.axis, corner.features, corner.proposal);
    QVERIFY(phases.valid);
    QCOMPARE(phases.apex.unresolvedReason, QString(cornerPhaseMultipleApexes));
    QVERIFY(!phases.apex.resolved());
    QCOMPARE(phases.apexCandidatesMeters.size(), 2);
    // Tight arcs centred at path distance ~104.7 m and ~208.4 m.
    QVERIFY(std::abs(phases.apexCandidatesMeters[0] - 104.7) < 8.0);
    QVERIFY(std::abs(phases.apexCandidatesMeters[1] - 208.4) < 8.0);
    QCOMPARE(phases.apex.evidence.value("candidatesMeters").toArray().size(), 2);
    QVERIFY(phases.entry.resolved());
    QVERIFY(phases.exit.resolved());
}

void CornerPhaseTests::widensABroadConstantRadiusApex()
{
    const auto corner = firstCorner(stadium());
    const auto phases = proposeCornerGeometryPhases(corner.axis, corner.features, corner.proposal);
    QVERIFY(phases.valid);
    QVERIFY(phases.apex.resolved());
    QVERIFY(phases.apex.uncertaintyReasons.contains(cornerPhaseBroadPeak));
    // A 125.7 m semicircle from 100 m: its middle is ~162.8 m and the whole arc is one region.
    QVERIFY(std::abs(phases.apex.progressMeters - 162.8) < 6.0);
    QVERIFY(phases.apex.toleranceMeters > 40.0);
}

void CornerPhaseTests::locatesMinimumSpeedSeparatelyFromTheApex()
{
    const auto corner = firstCorner(singleApexHalf());
    const auto lap = driveLap(singleApexHalf(), speedDipAt185);
    const auto trace = projectLapTrace(corner.axis, lap.session, 0.0, lap.endTime);
    QVERIFY(!trace.isEmpty());
    const auto minimum = locateMinimumSpeed(corner.axis, corner.proposal, trace, lap.session, 1.0);
    QVERIFY2(minimum.resolved(), qPrintable(minimum.unresolvedReason));
    QCOMPARE(minimum.method, QString(cornerPhaseMinimumSpeed));
    QVERIFY2(std::abs(minimum.progressMeters - 185.0) <= minimum.toleranceMeters + 2.0,
        qPrintable(QString("minimum speed at %1 m").arg(minimum.progressMeters)));
    QVERIFY(minimum.toleranceMeters < 10.0);
    QVERIFY(minimum.uncertaintyReasons.isEmpty());
    QCOMPARE(minimum.evidence.value("channel").toString(), QString("velocity"));
    QCOMPARE(minimum.evidence.value("unit").toString(), QString("km/h"));
    QVERIFY(std::abs(minimum.evidence.value("minimumValue").toDouble() - 12.0 * 3.6) < 1.0);

    const auto phases = proposeCornerGeometryPhases(corner.axis, corner.features, corner.proposal);
    QVERIFY(phases.apex.resolved());
    QVERIFY2(std::abs(phases.apex.progressMeters - minimum.progressMeters) > 20.0,
        "the apex is geometry; the minimum-speed point is where this lap was slowest");
}

void CornerPhaseTests::leavesMinimumSpeedUnresolvedOnIncompleteData()
{
    const auto corner = firstCorner(singleApexHalf());

    const auto gpsGapLap = driveLap(singleApexHalf(), speedDipAt185, {170.0, 200.0});
    const auto gpsGap = locateMinimumSpeed(corner.axis, corner.proposal,
        projectLapTrace(corner.axis, gpsGapLap.session, 0.0, gpsGapLap.endTime), gpsGapLap.session, 1.0);
    QCOMPARE(gpsGap.unresolvedReason, QString(cornerPhaseIncompleteCoverage));
    QVERIFY(gpsGap.evidence.value("missingSamples").toInt() > 0);

    const auto speedGapLap = driveLap(singleApexHalf(), speedDipAt185, {-1.0, -1.0}, {175.0, 195.0});
    const auto speedGap = locateMinimumSpeed(corner.axis, corner.proposal,
        projectLapTrace(corner.axis, speedGapLap.session, 0.0, speedGapLap.endTime), speedGapLap.session, 1.0);
    QCOMPARE(speedGap.unresolvedReason, QString(cornerPhaseIncompleteCoverage));

    auto noSpeed = driveLap(singleApexHalf(), speedDipAt185);
    const auto trace = projectLapTrace(corner.axis, noSpeed.session, 0.0, noSpeed.endTime);
    noSpeed.session.aliases.remove("speed");
    QCOMPARE(locateMinimumSpeed(corner.axis, corner.proposal, trace, noSpeed.session, 1.0).unresolvedReason,
        QString(cornerPhaseSpeedChannelMissing));

    const auto flatLap = driveLap(singleApexHalf(), [](double) { return 25.0; });
    QCOMPARE(locateMinimumSpeed(corner.axis, corner.proposal,
                 projectLapTrace(corner.axis, flatLap.session, 0.0, flatLap.endTime), flatLap.session, 1.0)
                 .unresolvedReason,
        QString(cornerPhaseFlatSpeed));

    auto wrapping = corner.proposal;
    wrapping.start.progressMeters = corner.axis.lengthMeters - 30.0;
    wrapping.end.progressMeters = 30.0;
    const auto normal = driveLap(singleApexHalf(), speedDipAt185);
    QCOMPARE(locateMinimumSpeed(corner.axis, wrapping,
                 projectLapTrace(corner.axis, normal.session, 0.0, normal.endTime), normal.session, 1.0)
                 .unresolvedReason,
        QString(cornerPhaseCrossesGate));
}

void CornerPhaseTests::rejectsInvalidInputs()
{
    const auto corner = firstCorner(singleApexHalf());
    QVERIFY(proposeCornerGeometryPhases(corner.axis, corner.features, corner.proposal).valid);
    QVERIFY(!proposeCornerGeometryPhases(ProgressAxis{}, corner.features, corner.proposal).valid);
    QVERIFY(!proposeCornerGeometryPhases(corner.axis, TrackFeatures{}, corner.proposal).valid);

    auto straightProposal = corner.proposal;
    straightProposal.type = TrackSegmentType::Straight;
    QVERIFY(!proposeCornerGeometryPhases(corner.axis, corner.features, straightProposal).valid);
    auto outOfAxis = corner.proposal;
    outOfAxis.end.progressMeters = corner.axis.lengthMeters + 1.0;
    QVERIFY(!proposeCornerGeometryPhases(corner.axis, corner.features, outOfAxis).valid);

    CornerPhaseOptions options;
    options.apexSeparationRatio = options.apexRegionRatio;
    QVERIFY(!proposeCornerGeometryPhases(corner.axis, corner.features, corner.proposal, options).valid);
    options = {};
    options.flatSpeedFraction = std::numeric_limits<double>::quiet_NaN();
    QVERIFY(!proposeCornerGeometryPhases(corner.axis, corner.features, corner.proposal, options).valid);

    const auto lap = driveLap(singleApexHalf(), speedDipAt185);
    const auto trace = projectLapTrace(corner.axis, lap.session, 0.0, lap.endTime);
    QCOMPARE(locateMinimumSpeed(corner.axis, corner.proposal, trace, lap.session, 0.0).unresolvedReason,
        QString(cornerPhaseInvalidInput));
    QCOMPARE(locateMinimumSpeed(corner.axis, corner.proposal, trace, lap.session, 1e-6).unresolvedReason,
        QString(cornerPhaseInvalidInput));
    QCOMPARE(locateMinimumSpeed(corner.axis, straightProposal, trace, lap.session, 1.0).unresolvedReason,
        QString(cornerPhaseInvalidInput));
}

QTEST_GUILESS_MAIN(CornerPhaseTests)
#include "CornerPhaseTests.moc"
