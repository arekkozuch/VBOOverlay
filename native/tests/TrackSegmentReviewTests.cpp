// Segment-proposal review (native/src/telemetry/TrackSegmentReview.*, KAN-48):
// approval state is derived from the persisted approved segments, and derived
// results identify the approved revision they were computed from.

#include "SyntheticLoopFixture.h"
#include "telemetry/TrackSegmentReview.h"

#include <QtTest>
#include <limits>

using namespace FlappedEar;
using namespace SyntheticLoop;

namespace {

QString configuration(const char digit = 'a') { return "compatibility-v1:" + QString(64, QChar(digit)); }

TrackSegmentProposals stadiumProposals(ProgressAxis *axisOut = nullptr)
{
    const auto axis = buildLoopAxis(stadium());
    if (axisOut) *axisOut = axis;
    return proposeTrackSegments(axis, computeTrackFeatures(axis, 6.0));
}

QJsonObject segmentFor(const TrackSegmentProposal &proposal, const QString &reference = configuration())
{
    return makeTrackSegment(proposal.type, proposal.name, proposal.start.progressMeters,
        proposal.end.progressMeters, reference);
}

} // namespace

class TrackSegmentReviewTests final : public QObject {
    Q_OBJECT
private slots:
    void derivesProposalStatesFromTheApprovedSet();
    void approvalOrdersSegmentsAndRejectsOverlap();
    void wrappingSegmentsOverlapAcrossTheGate();
    void refusesToMixTrackConfigurations();
    void revocationAndDiscardAreExplicit();
    void resultStampsIdentifyTheApprovedRevision();
    void rejectsMalformedEdits();
    void treatsMalformedStoredSegmentsAsInvalid();
    void approvingEveryStadiumProposalYieldsAValidSet();
};

void TrackSegmentReviewTests::derivesProposalStatesFromTheApprovedSet()
{
    ProgressAxis axis;
    const auto proposals = stadiumProposals(&axis);
    QVERIFY(proposals.valid);
    QCOMPARE(proposals.proposals.size(), 4);

    auto nothing = approvedSegmentation(QJsonValue(), configuration());
    QVERIFY(nothing.valid);
    QVERIFY(nothing.revision.isEmpty());
    auto items = reviewSegmentProposals(proposals.proposals, {}, {1}, nothing, axis.lengthMeters);
    QCOMPARE(items.size(), 4);
    QVERIFY(items[0].state == SegmentReviewState::Proposed);
    QVERIFY(items[1].state == SegmentReviewState::Rejected);

    const auto approvedSegment = segmentFor(proposals.proposals[0]);
    const auto stored = withApprovedSegment(QJsonValue(), approvedSegment, axis.lengthMeters);
    QVERIFY(stored);
    const auto approved = approvedSegmentation(*stored, configuration());
    QVERIFY(!approved.revision.isEmpty());
    items = reviewSegmentProposals(proposals.proposals, {}, {1}, approved, axis.lengthMeters);
    QVERIFY(items[0].state == SegmentReviewState::Approved);
    QCOMPARE(items[0].approvedSegmentId, approvedSegment.value("id").toString());
    QVERIFY(items[1].state == SegmentReviewState::Rejected); // only touches the approved corner
    QVERIFY(items[2].state == SegmentReviewState::Proposed);

    // An edited neighbour that now reaches into the approved corner is superseded, not approvable.
    auto edited = proposals.proposals;
    edited[1].start.progressMeters -= 20.0;
    items = reviewSegmentProposals(edited, {1}, {}, approved, axis.lengthMeters);
    QVERIFY(items[1].edited);
    QVERIFY(items[1].state == SegmentReviewState::Superseded);

    // Approval matches exact bounds and type, not just the name.
    auto moved = proposals.proposals;
    moved[0].start.progressMeters += 1.0;
    items = reviewSegmentProposals(moved, {0}, {}, approved, axis.lengthMeters);
    QVERIFY(items[0].state == SegmentReviewState::Superseded);
    QCOMPARE(segmentReviewStateName(SegmentReviewState::Superseded), QString("superseded"));
}

void TrackSegmentReviewTests::approvalOrdersSegmentsAndRejectsOverlap()
{
    ProgressAxis axis;
    const auto proposals = stadiumProposals(&axis);
    const auto third = segmentFor(proposals.proposals[2]);
    const auto first = segmentFor(proposals.proposals[0]);
    auto stored = withApprovedSegment(QJsonValue(), third, axis.lengthMeters);
    QVERIFY(stored);
    stored = withApprovedSegment(*stored, first, axis.lengthMeters);
    QVERIFY(stored);
    QCOMPARE(stored->size(), 2);
    QCOMPARE(stored->at(0).toObject().value("id"), first.value("id"));
    QVERIFY(validTrackSegments(*stored));

    QString error;
    const auto overlapping = makeTrackSegment(TrackSegmentType::Sector, "Sector 1",
        proposals.proposals[0].start.progressMeters + 5.0, proposals.proposals[1].end.progressMeters, configuration());
    QVERIFY(!withApprovedSegment(*stored, overlapping, axis.lengthMeters, &error));
    QVERIFY(error.contains("Overlaps"));
    QVERIFY(!withApprovedSegment(*stored, first, axis.lengthMeters, &error));
    QVERIFY(error.contains("already approved"));

    // A segment that only touches its neighbours' boundaries is accepted.
    QVERIFY(withApprovedSegment(*stored, segmentFor(proposals.proposals[1]), axis.lengthMeters));
}

void TrackSegmentReviewTests::wrappingSegmentsOverlapAcrossTheGate()
{
    const double length = 1000.0;
    QVERIFY(progressRangesOverlap({900.0, 50.0}, {20.0, 100.0}, length));
    QVERIFY(progressRangesOverlap({900.0, 50.0}, {950.0, 980.0}, length));
    QVERIFY(!progressRangesOverlap({900.0, 50.0}, {50.0, 900.0}, length));
    QVERIFY(!progressRangesOverlap({0.0, 100.0}, {100.0, 200.0}, length));
    QVERIFY(progressRangesOverlap({800.0, 100.0}, {900.0, 50.0}, length));

    const auto wrap = makeTrackSegment(TrackSegmentType::Straight, "Main straight", 900.0, 50.0, configuration());
    const auto early = makeTrackSegment(TrackSegmentType::Corner, "Turn 1", 20.0, 100.0, configuration());
    auto stored = withApprovedSegment(QJsonValue(), wrap, length);
    QVERIFY(stored);
    QVERIFY(!withApprovedSegment(*stored, early, length));
    const auto later = makeTrackSegment(TrackSegmentType::Corner, "Turn 1", 50.0, 100.0, configuration());
    stored = withApprovedSegment(*stored, later, length);
    QVERIFY(stored);
    QCOMPARE(stored->last().toObject().value("id"), wrap.value("id")); // the wrap stays last
}

void TrackSegmentReviewTests::refusesToMixTrackConfigurations()
{
    const auto old = makeTrackSegment(TrackSegmentType::Corner, "Old turn", 10.0, 50.0, configuration('b'));
    const QJsonArray stored{old};
    const auto approved = approvedSegmentation(stored, configuration());
    QVERIFY(approved.valid);
    QVERIFY(approved.segments.isEmpty());
    QVERIFY(approved.revision.isEmpty());
    QCOMPARE(approved.otherConfigurationSegments, 1);

    QString error;
    const auto current = makeTrackSegment(TrackSegmentType::Corner, "Turn 1", 300.0, 350.0, configuration());
    QVERIFY(!withApprovedSegment(stored, current, 1000.0, &error));
    QVERIFY(error.contains("different track configuration"));
}

void TrackSegmentReviewTests::revocationAndDiscardAreExplicit()
{
    const auto a = makeTrackSegment(TrackSegmentType::Corner, "Turn 1", 10.0, 50.0, configuration());
    const auto b = makeTrackSegment(TrackSegmentType::Corner, "Turn 2", 100.0, 150.0, configuration());
    const QJsonArray stored{a, b};
    const auto revoked = withoutApprovedSegment(stored, a.value("id").toString());
    QVERIFY(revoked);
    QCOMPARE(revoked->size(), 1);
    QVERIFY(!withoutApprovedSegment(stored, "missing-id"));
    QVERIFY(!withoutApprovedSegment(QJsonValue(42), a.value("id").toString()));

    const auto other = makeTrackSegment(TrackSegmentType::Corner, "Old", 200.0, 250.0, configuration('b'));
    const auto kept = withoutOtherConfigurations(QJsonArray{a, other}, configuration());
    QCOMPARE(kept.size(), 1);
    QCOMPARE(kept.first().toObject().value("id"), a.value("id"));
}

void TrackSegmentReviewTests::resultStampsIdentifyTheApprovedRevision()
{
    const auto a = makeTrackSegment(TrackSegmentType::Corner, "Turn 1", 10.0, 50.0, configuration());
    const auto b = makeTrackSegment(TrackSegmentType::Corner, "Turn 2", 100.0, 150.0, configuration());
    const auto first = approvedSegmentation(QJsonArray{a}, configuration());
    const auto stamp = segmentationResultStamp(first);
    QCOMPARE(stamp.revision, first.revision);
    QCOMPARE(stamp.trackConfigurationReference, configuration());
    QVERIFY(segmentationResultCurrent(stamp, first));
    QVERIFY(segmentationResultCurrent(stamp, approvedSegmentation(QJsonArray{a}, configuration())));

    // Any approval, revocation or edit produces a different revision.
    QVERIFY(!segmentationResultCurrent(stamp, approvedSegmentation(QJsonArray{a, b}, configuration())));
    auto renamed = a;
    renamed.insert("name", "Hairpin");
    QVERIFY(!segmentationResultCurrent(stamp, approvedSegmentation(QJsonArray{renamed}, configuration())));
    // Same segments under another configuration are a different segmentation.
    auto moved = a;
    moved.insert("trackConfigurationReference", configuration('b'));
    QVERIFY(!segmentationResultCurrent(stamp, approvedSegmentation(QJsonArray{moved}, configuration('b'))));
    // Nothing approved is never current, even against an empty stamp.
    const auto none = approvedSegmentation(QJsonArray{}, configuration());
    QVERIFY(!segmentationResultCurrent(segmentationResultStamp(none), none));
    QVERIFY(!segmentationResultCurrent(stamp, approvedSegmentation(QJsonValue("bad"), configuration())));
}

void TrackSegmentReviewTests::rejectsMalformedEdits()
{
    constexpr double nan = std::numeric_limits<double>::quiet_NaN();
    constexpr double inf = std::numeric_limits<double>::infinity();
    QVERIFY(validProposalEdit("Turn 1", 10.0, 50.0, 1000.0));
    QVERIFY(validProposalEdit("Main straight", 900.0, 50.0, 1000.0)); // wraps the gate
    QString error;
    QVERIFY(!validProposalEdit("  ", 10.0, 50.0, 1000.0, &error));
    QVERIFY(!error.isEmpty());
    QVERIFY(!validProposalEdit(QString(161, 'x'), 10.0, 50.0, 1000.0));
    QVERIFY(!validProposalEdit(QString("Turn") + QChar::Null, 10.0, 50.0, 1000.0));
    QVERIFY(!validProposalEdit("Turn 1", nan, 50.0, 1000.0));
    QVERIFY(!validProposalEdit("Turn 1", 10.0, inf, 1000.0));
    QVERIFY(!validProposalEdit("Turn 1", -1.0, 50.0, 1000.0));
    QVERIFY(!validProposalEdit("Turn 1", 10.0, 1000.5, 1000.0));
    QVERIFY(!validProposalEdit("Turn 1", 50.0, 50.0, 1000.0, &error));
    QVERIFY(error.contains("empty"));
    QVERIFY(!validProposalEdit("Turn 1", 10.0, 50.0, 0.0));
    QVERIFY(!validProposalEdit("Turn 1", 10.0, 50.0, nan));
}

void TrackSegmentReviewTests::treatsMalformedStoredSegmentsAsInvalid()
{
    const auto malformed = approvedSegmentation(QJsonArray{QJsonObject{{"id", "x"}}}, configuration());
    QVERIFY(!malformed.valid);
    QVERIFY(malformed.revision.isEmpty());
    const auto segment = makeTrackSegment(TrackSegmentType::Corner, "Turn 1", 10.0, 50.0, configuration());
    QVERIFY(!withApprovedSegment(QJsonValue("bad"), segment, 1000.0));
    QVERIFY(!withApprovedSegment(QJsonValue(), QJsonObject{{"id", "x"}}, 1000.0));

    QJsonArray full;
    for (int i = 0; i < maximumTrackSegments; ++i)
        full.append(makeTrackSegment(TrackSegmentType::Sector, "S", i * 10.0, i * 10.0 + 5.0, configuration()));
    QString error;
    QVERIFY(!withApprovedSegment(full,
        makeTrackSegment(TrackSegmentType::Sector, "S", 900.0, 905.0, configuration()), 1000.0, &error));
    QVERIFY(error.contains("At most"));
}

void TrackSegmentReviewTests::approvingEveryStadiumProposalYieldsAValidSet()
{
    ProgressAxis axis;
    const auto proposals = stadiumProposals(&axis);
    QJsonValue stored;
    for (const auto &proposal : proposals.proposals) {
        QString error;
        const auto next = withApprovedSegment(stored, segmentFor(proposal), axis.lengthMeters, &error);
        QVERIFY2(next, qPrintable(error));
        stored = *next;
    }
    QVERIFY(validTrackSegments(stored));
    const auto approved = approvedSegmentation(stored, configuration());
    QCOMPARE(approved.segments.size(), 4);
    for (const auto &item : reviewSegmentProposals(proposals.proposals, {}, {}, approved, axis.lengthMeters))
        QVERIFY(item.state == SegmentReviewState::Approved);
}

QTEST_GUILESS_MAIN(TrackSegmentReviewTests)
#include "TrackSegmentReviewTests.moc"
