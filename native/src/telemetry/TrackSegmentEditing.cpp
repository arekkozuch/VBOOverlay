#include "telemetry/TrackSegmentEditing.h"

#include "telemetry/TrackSegmentReview.h"

#include <QJsonObject>

#include <algorithm>
#include <cmath>
#include <limits>

namespace FlappedEar {
namespace {

// Bounds survive a JSON round trip exactly; this only absorbs arithmetic noise.
constexpr double boundaryEpsilon = 1e-6;

bool fail(QString *error, const QString &reason)
{
    if (error) *error = reason;
    return false;
}

// Progress 0 and `length` are the same point on the loop (the gate).
bool sameBoundary(const double a, const double b, const double length)
{
    return std::abs(a - b) <= boundaryEpsilon || (std::abs(a - length) <= boundaryEpsilon && std::abs(b) <= boundaryEpsilon)
        || (std::abs(a) <= boundaryEpsilon && std::abs(b - length) <= boundaryEpsilon);
}

// Distance travelled from `from` to `to` in the direction of the lap.
double forward(const double from, const double to, const double length)
{
    return to >= from ? to - from : to + length - from;
}

double startOf(const QJsonObject &segment) { return segment.value("startProgressMeters").toDouble(); }
double endOf(const QJsonObject &segment) { return segment.value("endProgressMeters").toDouble(); }

std::optional<TrackSegmentType> typeFromName(const QString &name)
{
    for (const auto type : {TrackSegmentType::Sector, TrackSegmentType::Corner, TrackSegmentType::Straight})
        if (trackSegmentTypeName(type) == name) return type;
    return std::nullopt;
}

bool validLength(const double length, QString *error)
{
    return std::isfinite(length) && length > 0.0 ? true : fail(error, "The track axis is unavailable.");
}

bool withinAxis(const double meters, const double length)
{
    return std::isfinite(meters) && meters >= 0.0 && meters <= length;
}

std::optional<QVector<QJsonObject>> load(const QJsonValue &storedSegments, QString *error)
{
    if (!validTrackSegments(storedSegments)) {
        fail(error, "The stored approved segments are invalid.");
        return std::nullopt;
    }
    QVector<QJsonObject> segments;
    for (const auto &value : storedSegments.toArray()) segments.append(value.toObject());
    for (const auto &segment : segments) {
        if (segment.value("trackConfigurationReference") != segments.first().value("trackConfigurationReference")) {
            fail(error, "Segments approved for a different track configuration must be discarded first.");
            return std::nullopt;
        }
    }
    return segments;
}

qsizetype indexOf(const QVector<QJsonObject> &segments, const QString &id)
{
    for (qsizetype index = 0; index < segments.size(); ++index)
        if (segments[index].value("id").toString() == id) return index;
    return -1;
}

std::optional<QJsonArray> finalize(QVector<QJsonObject> segments, const double length, QString *error)
{
    for (const auto &segment : segments) {
        if (std::abs(startOf(segment) - endOf(segment)) <= boundaryEpsilon) {
            fail(error, QString("“%1” would become empty.").arg(segment.value("name").toString()));
            return std::nullopt;
        }
        if (!validTrackSegment(segment)) {
            fail(error, QString("“%1” would be invalid.").arg(segment.value("name").toString()));
            return std::nullopt;
        }
    }
    for (qsizetype a = 0; a < segments.size(); ++a) {
        for (qsizetype b = a + 1; b < segments.size(); ++b) {
            if (progressRangesOverlap({startOf(segments[a]), endOf(segments[a])},
                    {startOf(segments[b]), endOf(segments[b])}, length)) {
                fail(error, QString("“%1” would overlap “%2”.")
                    .arg(segments[a].value("name").toString(), segments[b].value("name").toString()));
                return std::nullopt;
            }
        }
    }
    if (segments.size() > maximumTrackSegments) {
        fail(error, QString("At most %1 segments can be approved.").arg(maximumTrackSegments));
        return std::nullopt;
    }
    // Start order, with the one segment that crosses the gate last.
    std::stable_sort(segments.begin(), segments.end(), [](const QJsonObject &a, const QJsonObject &b) {
        const bool aWraps = endOf(a) < startOf(a);
        const bool bWraps = endOf(b) < startOf(b);
        return aWraps != bWraps ? bWraps : startOf(a) < startOf(b);
    });
    QJsonArray result;
    for (const auto &segment : segments) result.append(segment);
    if (!validTrackSegments(result)) {
        fail(error, "Only one segment may cross the start/finish line.");
        return std::nullopt;
    }
    return result;
}

} // namespace

std::optional<QJsonArray> withEditedSegment(const QJsonValue &storedSegments, const QString &id, const QString &name,
    const QString &type, const double startMeters, const double endMeters, const bool keepAdjacentJoined,
    const double lengthMeters, QString *error)
{
    if (!validProposalEdit(name, startMeters, endMeters, lengthMeters, error)) return std::nullopt;
    if (!typeFromName(type)) {
        fail(error, "Choose corner, straight or sector.");
        return std::nullopt;
    }
    auto segments = load(storedSegments, error);
    if (!segments) return std::nullopt;
    const auto index = indexOf(*segments, id);
    if (index < 0) {
        fail(error, "This segment is no longer approved.");
        return std::nullopt;
    }
    const auto original = (*segments)[index];
    const double oldStart = startOf(original);
    const double oldEnd = endOf(original);
    if (keepAdjacentJoined) {
        for (qsizetype other = 0; other < segments->size(); ++other) {
            if (other == index) continue;
            auto &neighbour = (*segments)[other];
            if (!sameBoundary(startMeters, oldStart, lengthMeters) && sameBoundary(endOf(neighbour), oldStart, lengthMeters))
                neighbour.insert("endProgressMeters", startMeters <= boundaryEpsilon ? lengthMeters : startMeters);
            if (!sameBoundary(endMeters, oldEnd, lengthMeters) && sameBoundary(startOf(neighbour), oldEnd, lengthMeters))
                neighbour.insert("startProgressMeters", endMeters >= lengthMeters - boundaryEpsilon ? 0.0 : endMeters);
        }
    }
    auto &segment = (*segments)[index];
    segment.insert("name", name.trimmed());
    segment.insert("type", type);
    segment.insert("startProgressMeters", startMeters);
    segment.insert("endProgressMeters", endMeters);
    return finalize(*segments, lengthMeters, error);
}

std::optional<QJsonArray> withSplitSegment(const QJsonValue &storedSegments, const QString &id, const double atMeters,
    const QString &secondName, const double lengthMeters, QString *error)
{
    if (!validLength(lengthMeters, error)) return std::nullopt;
    auto segments = load(storedSegments, error);
    if (!segments) return std::nullopt;
    const auto index = indexOf(*segments, id);
    if (index < 0) {
        fail(error, "This segment is no longer approved.");
        return std::nullopt;
    }
    const auto original = (*segments)[index];
    const double start = startOf(original);
    const double end = endOf(original);
    const double before = withinAxis(atMeters, lengthMeters) ? forward(start, atMeters, lengthMeters) : 0.0;
    const double after = withinAxis(atMeters, lengthMeters) ? forward(atMeters, end, lengthMeters) : 0.0;
    if (!(before > boundaryEpsilon) || !(after > boundaryEpsilon)
        || std::abs(before + after - forward(start, end, lengthMeters)) > boundaryEpsilon) {
        fail(error, "Split inside the segment, away from its ends.");
        return std::nullopt;
    }
    const bool atGate = atMeters <= boundaryEpsilon || atMeters >= lengthMeters - boundaryEpsilon;
    const auto second = makeTrackSegment(*typeFromName(original.value("type").toString()), secondName.trimmed(),
        atGate ? 0.0 : atMeters, end, original.value("trackConfigurationReference").toString());
    if (second.isEmpty()) {
        fail(error, "Enter a name of 1–160 characters for the new segment.");
        return std::nullopt;
    }
    (*segments)[index].insert("endProgressMeters", atGate ? lengthMeters : atMeters);
    segments->append(second);
    return finalize(*segments, lengthMeters, error);
}

std::optional<QJsonArray> withMergedSegments(const QJsonValue &storedSegments, const QString &firstId,
    const QString &secondId, const double lengthMeters, QString *error)
{
    if (!validLength(lengthMeters, error)) return std::nullopt;
    auto segments = load(storedSegments, error);
    if (!segments) return std::nullopt;
    auto first = indexOf(*segments, firstId);
    auto second = indexOf(*segments, secondId);
    if (first < 0 || second < 0 || first == second) {
        fail(error, "Choose two different approved segments.");
        return std::nullopt;
    }
    if (!sameBoundary(endOf((*segments)[first]), startOf((*segments)[second]), lengthMeters)) std::swap(first, second);
    const auto earlier = (*segments)[first];
    const auto later = (*segments)[second];
    if (!sameBoundary(endOf(earlier), startOf(later), lengthMeters)) {
        fail(error, "Only segments that share a boundary can be merged.");
        return std::nullopt;
    }
    if (sameBoundary(startOf(earlier), endOf(later), lengthMeters)) {
        fail(error, "Merging would cover the whole lap; a segment needs distinct start and end.");
        return std::nullopt;
    }
    auto merged = earlier;
    merged.insert("endProgressMeters", endOf(later));
    if (earlier.value("type") != later.value("type")) merged.insert("type", trackSegmentTypeName(TrackSegmentType::Sector));
    (*segments)[first] = merged;
    segments->removeAt(second);
    return finalize(*segments, lengthMeters, error);
}

void SegmentEditHistory::record(const QString &runId, const QJsonArray &before, const QJsonArray &after)
{
    if (before == after) return;
    m_redo.clear();
    m_undo.append({runId, before, after});
    while (m_undo.size() > std::max<qsizetype>(1, m_limit)) m_undo.removeFirst();
}

void SegmentEditHistory::commitUndo()
{
    if (m_undo.isEmpty()) return;
    m_redo.append(m_undo.takeLast());
}

void SegmentEditHistory::commitRedo()
{
    if (m_redo.isEmpty()) return;
    m_undo.append(m_redo.takeLast());
}

void SegmentEditHistory::clear()
{
    m_undo.clear();
    m_redo.clear();
}

ProgressPick pickProgressAt(const QVector<ProgressMapPoint> &trace, const QPointF &target, const double maximumDistance,
    const double ambiguityMargin, const double separationMeters, const double lengthMeters)
{
    if (trace.isEmpty() || !std::isfinite(target.x()) || !std::isfinite(target.y())
        || !std::isfinite(lengthMeters) || lengthMeters <= 0.0)
        return {std::nullopt, "noTrace"};
    const auto distanceTo = [&target](const ProgressMapPoint &sample) {
        return std::hypot(sample.point.x() - target.x(), sample.point.y() - target.y());
    };
    qsizetype nearest = 0;
    double nearestDistance = std::numeric_limits<double>::infinity();
    for (qsizetype index = 0; index < trace.size(); ++index) {
        const double distance = distanceTo(trace[index]);
        if (distance < nearestDistance) {
            nearestDistance = distance;
            nearest = index;
        }
    }
    if (!(nearestDistance <= maximumDistance)) return {std::nullopt, "farFromTrack"};
    const double progress = trace[nearest].progressMeters;
    for (const auto &sample : trace) {
        const double apart = std::abs(sample.progressMeters - progress);
        if (std::min(apart, lengthMeters - apart) > separationMeters
            && distanceTo(sample) <= nearestDistance + ambiguityMargin)
            return {std::nullopt, "ambiguous"};
    }
    return {progress, {}};
}

} // namespace FlappedEar
