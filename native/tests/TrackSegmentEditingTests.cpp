// Editing approved track segments (native/src/telemetry/TrackSegmentEditing.*, KAN-49):
// stable IDs, rejected overlap and empty segments, bounded undo/redo and
// crossing-safe map picking.

#include "telemetry/TrackSegmentEditing.h"
#include "telemetry/TrackSegmentReview.h"
#include "telemetry/TrackSegments.h"

#include <QtTest>
#include <limits>

using namespace FlappedEar;

namespace {

constexpr double lapLength = 1000.0;

QString reference(const QChar fill = 'a') { return "compatibility-v1:" + QString(64, fill); }

QJsonObject segment(const TrackSegmentType type, const QString &name, const double start, const double end,
    const QString &configuration = reference())
{
    return makeTrackSegment(type, name, start, end, configuration);
}

QJsonObject find(const QJsonArray &segments, const QString &id)
{
    for (const auto &value : segments)
        if (value.toObject().value("id").toString() == id) return value.toObject();
    return {};
}

double startOf(const QJsonObject &value) { return value.value("startProgressMeters").toDouble(); }
double endOf(const QJsonObject &value) { return value.value("endProgressMeters").toDouble(); }

} // namespace

class TrackSegmentEditingTests final : public QObject {
    Q_OBJECT
private slots:
    void editsKeepIdentityAndChangeTheRevision();
    void movesJoinedNeighboursOnlyWhenAsked();
    void rejectsInvalidEdits();
    void splitsKeepingTheFirstId();
    void splitsAtAndAcrossTheGate();
    void mergesAdjacentSegmentsOnly();
    void boundsTheSegmentCount();
    void historyUndoesAndRedoesWithinItsBound();
    void picksProgressButRefusesAmbiguousCrossings();
};

void TrackSegmentEditingTests::editsKeepIdentityAndChangeTheRevision()
{
    const auto a = segment(TrackSegmentType::Corner, "Turn 1", 100.0, 200.0);
    const auto b = segment(TrackSegmentType::Straight, "Back straight", 200.0, 400.0);
    const QJsonArray stored{a, b};
    const auto edited = withEditedSegment(stored, a.value("id").toString(), " Hairpin ", "straight", 100.0, 200.0,
        false, lapLength);
    QVERIFY(edited);
    QVERIFY(validTrackSegments(*edited));
    const auto renamed = find(*edited, a.value("id").toString());
    QCOMPARE(renamed.value("name").toString(), QString("Hairpin"));
    QCOMPARE(renamed.value("type").toString(), QString("straight"));
    QCOMPARE(renamed.value("trackConfigurationReference").toString(), reference());
    QVERIFY(find(*edited, b.value("id").toString()) == b);
    // Any edit changes the approved revision, so results stamped with the old one are stale.
    const auto before = approvedSegmentation(stored, reference());
    const auto after = approvedSegmentation(*edited, reference());
    QVERIFY(before.revision != after.revision);
    QVERIFY(!segmentationResultCurrent(segmentationResultStamp(before), after));
}

void TrackSegmentEditingTests::movesJoinedNeighboursOnlyWhenAsked()
{
    const auto a = segment(TrackSegmentType::Corner, "Turn 1", 100.0, 200.0);
    const auto b = segment(TrackSegmentType::Straight, "Back straight", 200.0, 400.0);
    const QJsonArray stored{a, b};
    const auto id = a.value("id").toString();

    QString error;
    QVERIFY(!withEditedSegment(stored, id, "Turn 1", "corner", 100.0, 250.0, false, lapLength, &error));
    QVERIFY2(error.contains("overlap"), qPrintable(error));

    const auto joined = withEditedSegment(stored, id, "Turn 1", "corner", 100.0, 250.0, true, lapLength);
    QVERIFY(joined);
    QCOMPARE(endOf(find(*joined, id)), 250.0);
    QCOMPARE(startOf(find(*joined, b.value("id").toString())), 250.0);
    QCOMPARE(endOf(find(*joined, b.value("id").toString())), 400.0);

    // Shrinking the joined neighbour to nothing is refused.
    QVERIFY(!withEditedSegment(stored, id, "Turn 1", "corner", 100.0, 400.0, true, lapLength, &error));
    QVERIFY2(error.contains("empty"), qPrintable(error));

    // Without the flag, a gap may open; segments need not cover the whole lap.
    const auto gap = withEditedSegment(stored, id, "Turn 1", "corner", 100.0, 150.0, false, lapLength);
    QVERIFY(gap);
    QCOMPARE(startOf(find(*gap, b.value("id").toString())), 200.0);
}

void TrackSegmentEditingTests::rejectsInvalidEdits()
{
    const auto a = segment(TrackSegmentType::Corner, "Turn 1", 100.0, 200.0);
    const QJsonArray stored{a};
    const auto id = a.value("id").toString();
    QVERIFY(!withEditedSegment(stored, id, "Turn 1", "corner", 150.0, 150.0, false, lapLength)); // empty
    QVERIFY(!withEditedSegment(stored, id, "Turn 1", "corner", 100.0, lapLength + 1.0, false, lapLength));
    QVERIFY(!withEditedSegment(stored, id, "Turn 1", "corner", std::numeric_limits<double>::quiet_NaN(), 10.0, false, lapLength));
    QVERIFY(!withEditedSegment(stored, id, " ", "corner", 100.0, 200.0, false, lapLength));
    QVERIFY(!withEditedSegment(stored, id, "Turn 1", "chicane", 100.0, 200.0, false, lapLength));
    QVERIFY(!withEditedSegment(stored, "missing", "Turn 1", "corner", 100.0, 200.0, false, lapLength));
    QVERIFY(!withEditedSegment(QJsonValue("not an array"), id, "Turn 1", "corner", 100.0, 200.0, false, lapLength));

    // Mixed configurations must be resolved first; progress is not comparable across them.
    const QJsonArray mixed{a, segment(TrackSegmentType::Straight, "Other layout", 300.0, 400.0, reference('b'))};
    QString error;
    QVERIFY(!withEditedSegment(mixed, id, "Turn 1", "corner", 100.0, 150.0, false, lapLength, &error));
    QVERIFY(error.contains("different track configuration"));
}

void TrackSegmentEditingTests::splitsKeepingTheFirstId()
{
    const auto a = segment(TrackSegmentType::Corner, "Turn 1", 100.0, 200.0);
    const auto b = segment(TrackSegmentType::Straight, "Back straight", 200.0, 400.0);
    const QJsonArray stored{a, b};
    const auto id = b.value("id").toString();
    const auto split = withSplitSegment(stored, id, 300.0, "Back straight 2", lapLength);
    QVERIFY(split);
    QCOMPARE(split->size(), 3);
    QVERIFY(validTrackSegments(*split));
    const auto first = find(*split, id);
    QCOMPARE(startOf(first), 200.0);
    QCOMPARE(endOf(first), 300.0);
    QCOMPARE(first.value("name").toString(), QString("Back straight"));
    const auto second = split->at(2).toObject();
    QVERIFY(second.value("id").toString() != id);
    QCOMPARE(second.value("name").toString(), QString("Back straight 2"));
    QCOMPARE(second.value("type").toString(), QString("straight"));
    QCOMPARE(startOf(second), 300.0);
    QCOMPARE(endOf(second), 400.0);

    QString error;
    QVERIFY(!withSplitSegment(stored, id, 200.0, "X", lapLength, &error)); // on its start
    QVERIFY(!withSplitSegment(stored, id, 400.0, "X", lapLength, &error)); // on its end
    QVERIFY(!withSplitSegment(stored, id, 450.0, "X", lapLength, &error)); // outside
    QVERIFY(!withSplitSegment(stored, id, 300.0, " ", lapLength, &error)); // unnamed part
    QVERIFY(!withSplitSegment(stored, "missing", 300.0, "X", lapLength, &error));
}

void TrackSegmentEditingTests::splitsAtAndAcrossTheGate()
{
    const auto wrap = segment(TrackSegmentType::Straight, "Main straight", 900.0, 50.0);
    const auto a = segment(TrackSegmentType::Corner, "Turn 1", 50.0, 200.0);
    const QJsonArray stored{a, wrap};
    const auto id = wrap.value("id").toString();

    const auto afterGate = withSplitSegment(stored, id, 20.0, "Main straight 2", lapLength);
    QVERIFY(afterGate);
    QVERIFY(validTrackSegments(*afterGate));
    QCOMPARE(startOf(find(*afterGate, id)), 900.0);
    QCOMPARE(endOf(find(*afterGate, id)), 20.0); // still crosses the gate, still last
    QCOMPARE(afterGate->last().toObject().value("id").toString(), id);
    QCOMPARE(startOf(afterGate->first().toObject()), 20.0);

    const auto atGate = withSplitSegment(stored, id, lapLength, "Main straight 2", lapLength);
    QVERIFY(atGate);
    QVERIFY(validTrackSegments(*atGate));
    QCOMPARE(endOf(find(*atGate, id)), lapLength); // no longer crosses
    QCOMPARE(startOf(atGate->first().toObject()), 0.0);
    QCOMPARE(endOf(atGate->first().toObject()), 50.0);
}

void TrackSegmentEditingTests::mergesAdjacentSegmentsOnly()
{
    const auto a = segment(TrackSegmentType::Corner, "Turn 1", 100.0, 200.0);
    const auto b = segment(TrackSegmentType::Straight, "Back straight", 200.0, 400.0);
    const auto c = segment(TrackSegmentType::Corner, "Turn 2", 500.0, 600.0);
    const QJsonArray stored{a, b, c};
    const auto merged = withMergedSegments(stored, b.value("id").toString(), a.value("id").toString(), lapLength);
    QVERIFY(merged);
    QCOMPARE(merged->size(), 2);
    const auto result = find(*merged, a.value("id").toString());
    QCOMPARE(startOf(result), 100.0);
    QCOMPARE(endOf(result), 400.0);
    QCOMPARE(result.value("name").toString(), QString("Turn 1"));
    QCOMPARE(result.value("type").toString(), QString("sector")); // corner + straight
    QVERIFY(find(*merged, b.value("id").toString()).isEmpty());

    QString error;
    QVERIFY(!withMergedSegments(stored, b.value("id").toString(), c.value("id").toString(), lapLength, &error));
    QVERIFY(error.contains("share a boundary"));
    QVERIFY(!withMergedSegments(stored, a.value("id").toString(), a.value("id").toString(), lapLength));

    // Across the gate: [900, 1000] + [0, 50] becomes one gate-crossing segment.
    const auto beforeGate = segment(TrackSegmentType::Straight, "Main A", 900.0, lapLength);
    const auto afterGate = segment(TrackSegmentType::Straight, "Main B", 0.0, 50.0);
    const auto acrossGate = withMergedSegments(QJsonArray{afterGate, beforeGate},
        beforeGate.value("id").toString(), afterGate.value("id").toString(), lapLength);
    QVERIFY(acrossGate);
    QCOMPARE(acrossGate->size(), 1);
    QCOMPARE(startOf(acrossGate->first().toObject()), 900.0);
    QCOMPARE(endOf(acrossGate->first().toObject()), 50.0);
    QCOMPARE(acrossGate->first().toObject().value("type").toString(), QString("straight"));

    // Two halves of the lap cannot become one segment with equal start and end.
    const auto half1 = segment(TrackSegmentType::Sector, "S1", 0.0, 500.0);
    const auto half2 = segment(TrackSegmentType::Sector, "S2", 500.0, lapLength);
    QVERIFY(!withMergedSegments(QJsonArray{half1, half2}, half1.value("id").toString(),
        half2.value("id").toString(), lapLength, &error));
    QVERIFY(error.contains("whole lap"));
}

void TrackSegmentEditingTests::boundsTheSegmentCount()
{
    QJsonArray stored;
    for (qsizetype i = 0; i < maximumTrackSegments; ++i)
        stored.append(segment(TrackSegmentType::Sector, QString("S%1").arg(i), i * 10.0, i * 10.0 + 10.0));
    QVERIFY(validTrackSegments(stored));
    QString error;
    QVERIFY(!withSplitSegment(stored, stored.first().toObject().value("id").toString(), 5.0, "Extra", lapLength, &error));
    QVERIFY(error.contains("At most"));
}

void TrackSegmentEditingTests::historyUndoesAndRedoesWithinItsBound()
{
    const QJsonArray s0;
    const QJsonArray s1{segment(TrackSegmentType::Corner, "Turn 1", 100.0, 200.0)};
    const QJsonArray s2{s1.first(), segment(TrackSegmentType::Straight, "Back", 200.0, 400.0)};
    SegmentEditHistory history(2);
    QVERIFY(!history.nextUndo());
    history.record("run", s0, s0); // no change: nothing recorded
    QVERIFY(!history.nextUndo());
    history.record("run", s0, s1);
    history.record("run", s1, s2);
    QVERIFY(history.nextUndo()->after == s2);
    QVERIFY(history.nextUndo()->before == s1);
    history.commitUndo();
    QVERIFY(history.nextUndo()->before == s0);
    QVERIFY(history.nextRedo()->after == s2);
    history.commitRedo();
    QVERIFY(history.nextUndo()->after == s2);
    QVERIFY(!history.nextRedo());

    // A new edit clears redo; the oldest step falls off beyond the bound.
    history.commitUndo();
    history.record("run", s1, s0);
    QVERIFY(!history.nextRedo());
    history.record("run", s0, s2);
    QCOMPARE(history.undoCount(), 2);
    history.clear();
    QVERIFY(!history.nextUndo());
    QVERIFY(!history.nextRedo());
}

void TrackSegmentEditingTests::picksProgressButRefusesAmbiguousCrossings()
{
    // A figure-eight crossing: (0,0)->(1,1) is progress 0..100, (1,0)->(0,1) is 100..200.
    QVector<ProgressMapPoint> trace;
    for (int i = 0; i <= 100; ++i) trace.append({double(i), QPointF(i / 100.0, i / 100.0)});
    for (int i = 0; i <= 100; ++i) trace.append({100.0 + i, QPointF(1.0 - i / 100.0, i / 100.0)});
    constexpr double length = 200.0;

    const auto picked = pickProgressAt(trace, {0.25, 0.26}, 0.03, 0.01, 30.0, length);
    QVERIFY(picked.progressMeters);
    QVERIFY(std::abs(*picked.progressMeters - 25.0) <= 1.0);

    const auto crossing = pickProgressAt(trace, {0.5, 0.5}, 0.03, 0.01, 30.0, length);
    QVERIFY(!crossing.progressMeters);
    QCOMPARE(crossing.reason, QString("ambiguous"));

    const auto far = pickProgressAt(trace, {0.9, 0.3}, 0.03, 0.01, 30.0, length);
    QVERIFY(!far.progressMeters);
    QCOMPARE(far.reason, QString("farFromTrack"));

    QCOMPARE(pickProgressAt({}, {0.5, 0.5}, 0.03, 0.01, 30.0, length).reason, QString("noTrace"));
}

QTEST_GUILESS_MAIN(TrackSegmentEditingTests)
#include "TrackSegmentEditingTests.moc"
