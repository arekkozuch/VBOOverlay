// The versioned sector/corner segment model (KAN-43): a stable-ID, typed,
// named, progress-bounded segment tied to a track-configuration reference,
// stored and validated as plain JSON (no persisted ProgressAxis samples),
// with a pure content-hash "revision" a consumer can compare to detect any
// add/remove/reorder/edit.

#include "telemetry/TrackSegments.h"
#include "project/EventProjectCodec.h"
#include "project/ProjectLimits.h"

#include <QtTest>

#include <limits>

using namespace FlappedEar;

namespace {

// A syntactically valid compatibility-group-shaped reference, matching the
// exact format lapCompatibilityGroupId() produces -- validTrackSegment only
// checks the format, not that it currently matches a live run's configuration
// (staleness is a runtime concern for a later ticket, not a load-time
// rejection, mirroring how a stale lap reference is left unresolved rather
// than treated as document corruption).
QString sampleConfigurationReference()
{
    return "compatibility-v1:" + QString(64, 'a');
}

} // namespace

class TrackSegmentsTests final : public QObject {
    Q_OBJECT
private slots:
    void makesValidSegmentWithStableId();
    void rejectsMalformedSegments();
    void acceptsEmptyOrAbsentSegmentList();
    void rejectsUnorderedOrDuplicateSegments();
    void permitsOnlyTheFinalSegmentToWrap();
    void boundsSegmentCount();
    void revisionChangesOnAnyEdit();
    void validatesThroughEventProjectCodec();
};

void TrackSegmentsTests::makesValidSegmentWithStableId()
{
    const auto reference = sampleConfigurationReference();
    const auto segment = makeTrackSegment(TrackSegmentType::Corner, "Turn 1", 120.5, 260.0, reference);
    QVERIFY(validTrackSegment(segment));
    QCOMPARE(segment.value("type").toString(), QString("corner"));
    QCOMPARE(segment.value("name").toString(), QString("Turn 1"));
    QCOMPARE(segment.value("startProgressMeters").toDouble(), 120.5);
    QCOMPARE(segment.value("endProgressMeters").toDouble(), 260.0);
    QCOMPARE(segment.value("trackConfigurationReference").toString(), reference);
    const auto id = segment.value("id").toString();
    QVERIFY(!id.isEmpty());

    // A fresh call mints a different stable ID; the same logical segment must
    // keep its own ID across edits elsewhere, but two independently created
    // segments are never accidentally identical.
    const auto another = makeTrackSegment(TrackSegmentType::Sector, "Sector 1", 0.0, 500.0, reference);
    QVERIFY(validTrackSegment(another));
    QVERIFY(another.value("id").toString() != id);
    QCOMPARE(another.value("type").toString(), QString("sector"));
}

void TrackSegmentsTests::rejectsMalformedSegments()
{
    const auto reference = sampleConfigurationReference();
    const auto valid = makeTrackSegment(TrackSegmentType::Corner, "Turn 1", 100.0, 200.0, reference);
    QVERIFY(validTrackSegment(valid));

    auto extraKey = valid; extraKey.insert("extra", "field");
    QVERIFY(!validTrackSegment(extraKey));

    auto emptyId = valid; emptyId.insert("id", "");
    QVERIFY(!validTrackSegment(emptyId));

    auto badType = valid; badType.insert("type", "straight");
    QVERIFY(!validTrackSegment(badType));

    auto emptyName = valid; emptyName.insert("name", "   ");
    QVERIFY(!validTrackSegment(emptyName));

    auto nonFiniteStart = valid; nonFiniteStart.insert("startProgressMeters", std::numeric_limits<double>::infinity());
    QVERIFY(!validTrackSegment(nonFiniteStart));

    auto negativeStart = valid; negativeStart.insert("startProgressMeters", -1.0);
    QVERIFY(!validTrackSegment(negativeStart));

    auto tooFarEnd = valid; tooFarEnd.insert("endProgressMeters", 1'000'001.0);
    QVERIFY(!validTrackSegment(tooFarEnd));

    auto zeroLength = valid; zeroLength.insert("endProgressMeters", valid.value("startProgressMeters"));
    QVERIFY(!validTrackSegment(zeroLength));

    auto badReference = valid; badReference.insert("trackConfigurationReference", "not-a-reference");
    QVERIFY(!validTrackSegment(badReference));

    auto wrongReferenceAlgorithm = valid;
    wrongReferenceAlgorithm.insert("trackConfigurationReference", "compatibility-v2:" + QString(64, 'a'));
    QVERIFY(!validTrackSegment(wrongReferenceAlgorithm));
}

void TrackSegmentsTests::acceptsEmptyOrAbsentSegmentList()
{
    QVERIFY(validTrackSegments(QJsonValue()));
    QVERIFY(validTrackSegments(QJsonValue(QJsonValue::Null)));
    QVERIFY(validTrackSegments(QJsonArray{}));
    QVERIFY(!validTrackSegments(QJsonValue("not-an-array")));
}

void TrackSegmentsTests::rejectsUnorderedOrDuplicateSegments()
{
    const auto reference = sampleConfigurationReference();
    const auto first = makeTrackSegment(TrackSegmentType::Sector, "Sector 1", 0.0, 300.0, reference);
    const auto second = makeTrackSegment(TrackSegmentType::Corner, "Turn 1", 300.0, 420.0, reference);
    QVERIFY(validTrackSegments(QJsonArray{first, second}));

    // Listed out of start-progress order.
    QVERIFY(!validTrackSegments(QJsonArray{second, first}));

    // Duplicate IDs (same segment object appearing twice).
    QVERIFY(!validTrackSegments(QJsonArray{first, first}));

    // An array element that is itself an invalid segment.
    QJsonObject broken = first; broken.insert("name", "");
    QVERIFY(!validTrackSegments(QJsonArray{broken}));
}

void TrackSegmentsTests::permitsOnlyTheFinalSegmentToWrap()
{
    const auto reference = sampleConfigurationReference();
    const auto straight = makeTrackSegment(TrackSegmentType::Sector, "Sector 1", 0.0, 300.0, reference);
    const auto corner = makeTrackSegment(TrackSegmentType::Corner, "Turn 1", 300.0, 420.0, reference);
    // The final segment covers the start/finish line: it wraps from near the
    // end of the lap back past progress 0.
    const auto wrapping = makeTrackSegment(TrackSegmentType::Corner, "Last corner", 950.0, 40.0, reference);
    QVERIFY(validTrackSegments(QJsonArray{straight, corner, wrapping}));

    // The same wrap in a non-final position must be rejected -- with only one
    // segment allowed to wrap (the last), this also covers "two wraps": the
    // one that isn't last is always rejected as a non-final wrap.
    QVERIFY(!validTrackSegments(QJsonArray{wrapping, straight, corner}));
}

void TrackSegmentsTests::boundsSegmentCount()
{
    const auto reference = sampleConfigurationReference();
    QJsonArray segments;
    for (qsizetype i = 0; i < maximumTrackSegments; ++i)
        segments.append(makeTrackSegment(TrackSegmentType::Corner,
            QString("Turn %1").arg(i + 1), double(i) * 10.0, double(i) * 10.0 + 5.0, reference));
    QVERIFY(validTrackSegments(segments));

    segments.append(makeTrackSegment(TrackSegmentType::Corner, "One too many",
        double(maximumTrackSegments) * 10.0, double(maximumTrackSegments) * 10.0 + 5.0, reference));
    QVERIFY(!validTrackSegments(segments));
}

void TrackSegmentsTests::revisionChangesOnAnyEdit()
{
    const auto reference = sampleConfigurationReference();
    const auto first = makeTrackSegment(TrackSegmentType::Sector, "Sector 1", 0.0, 300.0, reference);
    const auto second = makeTrackSegment(TrackSegmentType::Corner, "Turn 1", 300.0, 420.0, reference);
    const QJsonArray original{first, second};

    const auto revision = trackSegmentSetRevision(original);
    QVERIFY(!revision.isEmpty());
    QCOMPARE(trackSegmentSetRevision(original), revision); // deterministic for identical content.

    QJsonObject renamed = second; renamed.insert("name", "Turn 1 (hairpin)");
    QVERIFY(trackSegmentSetRevision(QJsonArray{first, renamed}) != revision);

    QJsonObject resized = second; resized.insert("endProgressMeters", 430.0);
    QVERIFY(trackSegmentSetRevision(QJsonArray{first, resized}) != revision);

    QVERIFY(trackSegmentSetRevision(QJsonArray{first}) != revision); // removal.
    QVERIFY(trackSegmentSetRevision(QJsonArray{first, second, second}) != revision); // addition (ignoring validity).
    QVERIFY(trackSegmentSetRevision(QJsonArray{second, first}) != revision); // reorder.
}

void TrackSegmentsTests::validatesThroughEventProjectCodec()
{
    const QJsonObject source{{"id", "run-a-source"},
        {"reference", QJsonObject{{"relativePath", "run-a.vbo"}}}};
    QJsonObject run{{"id", "run-a"}, {"name", "Morning"}, {"primaryTelemetrySourceId", "run-a-source"},
        {"sources", QJsonObject{{"telemetry", QJsonArray{source}}}},
        {"sync", QJsonObject{{"offset", 0.0}, {"timeScale", 1.0}}}};
    const QJsonObject event{{"id", "event-1"}, {"name", "Track day"}, {"activeRunId", "run-a"},
        {"runs", QJsonArray{run}}};
    const QJsonObject baseProject{{"version", 3}, {"event", event}};
    QString error;
    QVERIFY2(EventProjectCodec::validate(baseProject, &error), qPrintable(error));

    const auto reference = sampleConfigurationReference();
    const auto segment = makeTrackSegment(TrackSegmentType::Corner, "Turn 1", 100.0, 200.0, reference);
    run.insert("trackSegments", QJsonArray{segment});
    auto validEvent = event; auto runs = validEvent.value("runs").toArray(); runs[0] = run; validEvent.insert("runs", runs);
    QJsonObject validProject{{"version", 3}, {"event", validEvent}};
    QVERIFY2(EventProjectCodec::validate(validProject, &error), qPrintable(error));

    QJsonObject brokenSegment = segment; brokenSegment.insert("name", "");
    auto brokenRun = run; brokenRun.insert("trackSegments", QJsonArray{brokenSegment});
    auto invalidEvent = event; auto invalidRuns = invalidEvent.value("runs").toArray();
    invalidRuns[0] = brokenRun; invalidEvent.insert("runs", invalidRuns);
    QJsonObject invalidProject{{"version", 3}, {"event", invalidEvent}};
    QVERIFY(!EventProjectCodec::validate(invalidProject, &error));
    QVERIFY(error.contains("Track segments"));
}

QTEST_GUILESS_MAIN(TrackSegmentsTests)
#include "TrackSegmentsTests.moc"
