// Automatic straight/corner proposals (native/src/telemetry/TrackSegmentProposals.*, KAN-45):
// synthetic closed loops built from exact lines and arcs, run through the real
// buildProgressAxis -> computeTrackFeatures -> proposeTrackSegments pipeline.

#include "SyntheticLoopFixture.h"
#include "telemetry/TrackSegmentProposals.h"
#include "telemetry/TrackSegments.h"

#include <QtTest>
#include <cmath>
#include <limits>
#include <numbers>

using namespace FlappedEar;
using namespace SyntheticLoop;

namespace {

QString sampleConfigurationReference() { return "compatibility-v1:" + QString(64, 'a'); }

int uncertainBoundaryCount(const TrackSegmentProposals &result)
{
    int count = 0;
    for (const auto &proposal : result.proposals) count += proposal.start.certain() ? 0 : 1;
    return count;
}

} // namespace

class TrackSegmentProposalTests final : public QObject {
    Q_OBJECT
private slots:
    void proposesAlternatingCornersAndStraightsOnAStadium();
    void convertsProposalsIntoEditableTrackSegments();
    void marksBoundariesNearAGpsGapUncertain();
    void chainsConnectedOppositeCorners();
    void chainsCornersAcrossATooShortStraight();
    void marksShortStraightBoundariesUncertain();
    void foldsSmallKinksIntoTheStraight();
    void leavesAContinuousCornerUnresolved();
    void rejectsInvalidInputs();
};

void TrackSegmentProposalTests::proposesAlternatingCornersAndStraightsOnAStadium()
{
    const auto axis = buildLoopAxis(stadium());
    QVERIFY(axis.valid);
    const auto features = computeTrackFeatures(axis, 6.0);
    QVERIFY(features.valid);
    const auto result = proposeTrackSegments(axis, features);
    QVERIFY(result.valid);
    QVERIFY(result.unresolvedReason.isEmpty());
    QCOMPARE(result.proposals.size(), 4);

    const QVector<TrackSegmentType> expected{TrackSegmentType::Corner, TrackSegmentType::Straight,
        TrackSegmentType::Corner, TrackSegmentType::Straight};
    const QStringList names{"Corner 1", "Straight 1", "Corner 2", "Straight 2"};
    for (int i = 0; i < 4; ++i) {
        const auto &proposal = result.proposals[i];
        QVERIFY(proposal.type == expected[i]);
        QCOMPARE(proposal.name, names[i]);
        QVERIFY(proposal.start.certain());
        QVERIFY(proposal.end.certain());
        QCOMPARE(proposal.start.toleranceMeters, 6.0 + axis.spacingMeters);
        if (proposal.type == TrackSegmentType::Corner) {
            QVERIFY2(std::abs(proposal.turnRadians - std::numbers::pi) < 0.15, "a semicircle turns ~pi left");
            QVERIFY(proposal.peakCurvaturePerMeter > 0.0);
        } else {
            QCOMPARE(proposal.turnRadians, 0.0);
            QVERIFY(proposal.lengthMeters > 150.0);
        }
    }
    // Corner 1's arc begins 100 m after the gate; smoothing only moves it within tolerance.
    const auto &first = result.proposals.first();
    QVERIFY(std::abs(first.start.progressMeters - 100.0) <= first.start.toleranceMeters + axis.spacingMeters);
    // The gate sits mid-straight, so only the final proposal wraps.
    QVERIFY(result.proposals.last().end.progressMeters < result.proposals.last().start.progressMeters);
    for (int i = 0; i + 1 < 4; ++i)
        QCOMPARE(result.proposals[i].end.progressMeters, result.proposals[i + 1].start.progressMeters);

    const auto again = proposeTrackSegments(axis, features);
    QCOMPARE(again.proposals.size(), result.proposals.size());
    for (int i = 0; i < 4; ++i)
        QCOMPARE(again.proposals[i].start.progressMeters, result.proposals[i].start.progressMeters);
}

void TrackSegmentProposalTests::convertsProposalsIntoEditableTrackSegments()
{
    const auto axis = buildLoopAxis(stadium());
    const auto result = proposeTrackSegments(axis, computeTrackFeatures(axis, 6.0));
    QVERIFY(result.valid);
    auto segments = proposalsToTrackSegments(result, sampleConfigurationReference());
    QCOMPARE(segments.size(), 4);
    QVERIFY(validTrackSegments(segments));
    QCOMPARE(segments[0].toObject().value("type").toString(), QString("corner"));
    QCOMPARE(segments[1].toObject().value("type").toString(), QString("straight"));
    QVERIFY(segments[0].toObject().value("id").toString() != segments[1].toObject().value("id").toString());

    // Proposals carry no lock: rename and move a boundary like any other segment.
    auto edited = segments[0].toObject();
    edited["name"] = "Hairpin";
    edited["startProgressMeters"] = edited.value("startProgressMeters").toDouble() + 3.0;
    segments[0] = edited;
    QVERIFY(validTrackSegments(segments));

    QVERIFY(proposalsToTrackSegments(result, "not-a-reference").isEmpty());
    QVERIFY(proposalsToTrackSegments(TrackSegmentProposals{}, sampleConfigurationReference()).isEmpty());
}

void TrackSegmentProposalTests::marksBoundariesNearAGpsGapUncertain()
{
    const auto axis = buildLoopAxis(stadium());
    const auto features = computeTrackFeatures(axis, 6.0);
    const auto result = proposeTrackSegments(axis, features, {{90.0, 105.0}});
    QVERIFY(result.valid);
    QCOMPARE(result.proposals.size(), 4);
    QVERIFY(result.proposals[0].start.uncertaintyReasons.contains(proposalUncertainGpsGap));
    QVERIFY(result.proposals[3].end.uncertaintyReasons.contains(proposalUncertainGpsGap));
    QCOMPARE(uncertainBoundaryCount(result), 1);

    // A gap across the gate wraps.
    const auto wrapping = proposeTrackSegments(axis, features, {{axis.lengthMeters - 5.0, 95.0}});
    QVERIFY(wrapping.proposals[0].start.uncertaintyReasons.contains(proposalUncertainGpsGap));
}

void TrackSegmentProposalTests::chainsConnectedOppositeCorners()
{
    // KAN-116: each 80 m straight on the long sides carries a left-right
    // chicane with no straight between; the chicane is one corner chain.
    const auto axis = buildLoopAxis(
        {straight(100), arc(180, 40), straight(80), arc(45, 25), arc(-45, 25), straight(80)});
    QVERIFY(axis.valid);
    const auto result = proposeTrackSegments(axis, computeTrackFeatures(axis, 6.0));
    QVERIFY(result.valid);

    int corners = 0;
    int chains = 0;
    int straights = 0;
    for (const auto &proposal : result.proposals) {
        QVERIFY(!proposal.start.uncertaintyReasons.contains(proposalUncertainConnectedCorners));
        if (proposal.type == TrackSegmentType::Straight) {
            ++straights;
            QCOMPARE(proposal.chainedCorners, 0);
            continue;
        }
        ++corners;
        if (proposal.chainedCorners > 1) {
            ++chains;
            QCOMPARE(proposal.chainedCorners, 2);
            QVERIFY2(proposal.name.startsWith("Corners "), qPrintable(proposal.name));
            // Left then right: the chain's net heading change is close to zero.
            QVERIFY(std::abs(proposal.turnRadians) < 0.2);
        } else {
            QCOMPARE(proposal.chainedCorners, 1);
            QVERIFY(std::abs(proposal.turnRadians - std::numbers::pi) < 0.15);
        }
    }
    QCOMPARE(chains, 2);
    QCOMPARE(corners, 4);
    QCOMPARE(straights, 4); // corners and straights still alternate
    for (qsizetype i = 0; i < result.proposals.size(); ++i) {
        const auto &next = result.proposals[(i + 1) % result.proposals.size()];
        QVERIFY(result.proposals[i].type != next.type);
    }
    QCOMPARE(uncertainBoundaryCount(result), 0);
    // Corner numbering counts the corners inside a chain.
    QStringList names;
    for (const auto &proposal : result.proposals) if (proposal.type == TrackSegmentType::Corner) names << proposal.name;
    QCOMPARE(names.size(), 4);
    QVERIFY(names.contains("Corner 1") && names.contains("Corners 2–3") && names.contains("Corner 4") && names.contains("Corners 5–6"));
}

void TrackSegmentProposalTests::chainsCornersAcrossATooShortStraight()
{
    // Two 90-degree corners separated by a 16 m straight (a few metres of it
    // survive the 6 m smoothing, below the 20 m minimum) at each end: one
    // same-direction chain per end.
    const auto axis = buildLoopAxis(
        {straight(100), arc(90, 30), straight(16), arc(90, 30), straight(100)});
    QVERIFY(axis.valid);
    const auto result = proposeTrackSegments(axis, computeTrackFeatures(axis, 6.0));
    QVERIFY(result.valid);
    QCOMPARE(result.proposals.size(), 4);
    for (const auto &proposal : result.proposals) {
        if (proposal.type != TrackSegmentType::Corner) continue;
        QCOMPARE(proposal.chainedCorners, 2);
        QVERIFY(std::abs(proposal.turnRadians - std::numbers::pi) < 0.15);
    }
}

void TrackSegmentProposalTests::marksShortStraightBoundariesUncertain()
{
    // Each end is two 90-degree corners separated by a 44 m straight.
    const auto axis = buildLoopAxis(
        {straight(100), arc(90, 30), straight(44), arc(90, 30), straight(100)});
    QVERIFY(axis.valid);
    const auto result = proposeTrackSegments(axis, computeTrackFeatures(axis, 6.0));
    QVERIFY(result.valid);
    QCOMPARE(result.proposals.size(), 8);

    int shortStraights = 0;
    for (const auto &proposal : result.proposals) {
        if (proposal.type != TrackSegmentType::Straight || proposal.lengthMeters >= result.options.certainStraightMeters)
            continue;
        ++shortStraights;
        QVERIFY(proposal.lengthMeters >= result.options.connectedStraightMeters);
        QVERIFY(proposal.start.uncertaintyReasons.contains(proposalUncertainShortStraight));
        QVERIFY(proposal.end.uncertaintyReasons.contains(proposalUncertainShortStraight));
    }
    QCOMPARE(shortStraights, 2);
    QCOMPARE(uncertainBoundaryCount(result), 4);
}

void TrackSegmentProposalTests::foldsSmallKinksIntoTheStraight()
{
    const auto axis = buildLoopAxis({straight(100), arc(180, 40), straight(60), arc(10, 30), straight(40),
        arc(-10, 30), straight(60)});
    QVERIFY(axis.valid);
    const auto result = proposeTrackSegments(axis, computeTrackFeatures(axis, 6.0));
    QVERIFY(result.valid);
    QCOMPARE(result.proposals.size(), 4);
    QCOMPARE(uncertainBoundaryCount(result), 0);
}

void TrackSegmentProposalTests::leavesAContinuousCornerUnresolved()
{
    const auto axis = buildLoopAxis({arc(180, 50)});
    QVERIFY(axis.valid);
    const auto result = proposeTrackSegments(axis, computeTrackFeatures(axis, 6.0));
    QVERIFY(result.valid);
    QVERIFY(result.proposals.isEmpty());
    QCOMPARE(result.unresolvedReason, QString("continuousCorner"));
    QVERIFY(proposalsToTrackSegments(result, sampleConfigurationReference()).isEmpty());
}

void TrackSegmentProposalTests::rejectsInvalidInputs()
{
    const auto axis = buildLoopAxis(stadium());
    const auto features = computeTrackFeatures(axis, 6.0);
    QVERIFY(proposeTrackSegments(axis, features).valid);

    QVERIFY(!proposeTrackSegments(ProgressAxis{}, features).valid);
    QVERIFY(!proposeTrackSegments(axis, TrackFeatures{}).valid);
    const auto otherAxis = buildLoopAxis({arc(180, 50)});
    QVERIFY(!proposeTrackSegments(axis, computeTrackFeatures(otherAxis, 6.0)).valid);

    const double nan = std::numeric_limits<double>::quiet_NaN();
    SegmentProposalOptions options;
    options.cornerCurvaturePerMeter = nan;
    QVERIFY(!proposeTrackSegments(axis, features, {}, options).valid);
    options = {};
    options.minimumCornerTurnRadians = 0.0;
    QVERIFY(!proposeTrackSegments(axis, features, {}, options).valid);
    options = {};
    options.certainStraightMeters = options.connectedStraightMeters - 1.0;
    QVERIFY(!proposeTrackSegments(axis, features, {}, options).valid);

    QVERIFY(!proposeTrackSegments(axis, features, {{nan, 10.0}}).valid);
    QVERIFY(!proposeTrackSegments(axis, features, {{-1.0, 10.0}}).valid);
    QVERIFY(!proposeTrackSegments(axis, features, {{10.0, axis.lengthMeters + 1.0}}).valid);
    QVERIFY(!proposeTrackSegments(axis, features,
        QVector<ProgressRange>(maximumProposalGpsGaps + 1, ProgressRange{1.0, 2.0})).valid);
}

QTEST_GUILESS_MAIN(TrackSegmentProposalTests)
#include "TrackSegmentProposalTests.moc"
