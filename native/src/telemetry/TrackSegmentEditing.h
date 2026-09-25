#pragma once

#include "telemetry/TrackSegments.h"

#include <QJsonArray>
#include <QJsonValue>
#include <QPointF>
#include <QString>
#include <QVector>
#include <optional>

namespace FlappedEar {

// Editing of approved track segments (KAN-49). Every operation takes the
// run's stored "trackSegments" and returns a complete replacement array, or
// nullopt with a reason. Results are always ordered, non-overlapping, bounded
// and valid; a segment that would become empty is refused. Stable identity:
// an edited, moved or renamed segment keeps its ID; a split keeps the ID on
// the first part; a merge keeps the first segment's ID. All stored segments
// must belong to one track configuration.
inline constexpr auto trackSegmentEditingAlgorithm = "track-segment-editing-v1";

// Renames, retypes and moves one segment. With `keepAdjacentJoined`, a
// neighbour that shared a moved boundary moves with it.
[[nodiscard]] std::optional<QJsonArray> withEditedSegment(const QJsonValue &storedSegments, const QString &id,
    const QString &name, const QString &type, double startMeters, double endMeters, bool keepAdjacentJoined,
    double lengthMeters, QString *error = nullptr);

// Splits one segment strictly inside its range. The second part gets a fresh ID.
[[nodiscard]] std::optional<QJsonArray> withSplitSegment(const QJsonValue &storedSegments, const QString &id,
    double atMeters, const QString &secondName, double lengthMeters, QString *error = nullptr);

// Merges two segments that share a boundary (in either order). The result keeps
// the earlier segment's ID and name; differing types become "sector".
[[nodiscard]] std::optional<QJsonArray> withMergedSegments(const QJsonValue &storedSegments, const QString &firstId,
    const QString &secondId, double lengthMeters, QString *error = nullptr);

struct SegmentEditStep {
    QString runId;
    QJsonArray before;
    QJsonArray after;
};

// Bounded undo/redo of whole-array segment changes within one editing session.
// A step may only be applied while the run still holds exactly the state the
// step left behind; the caller checks that and clears the history otherwise.
class SegmentEditHistory {
public:
    explicit SegmentEditHistory(qsizetype limit = 50) : m_limit(limit) {}
    void record(const QString &runId, const QJsonArray &before, const QJsonArray &after);
    [[nodiscard]] const SegmentEditStep *nextUndo() const { return m_undo.isEmpty() ? nullptr : &m_undo.last(); }
    [[nodiscard]] const SegmentEditStep *nextRedo() const { return m_redo.isEmpty() ? nullptr : &m_redo.last(); }
    void commitUndo();
    void commitRedo();
    void clear();
    [[nodiscard]] qsizetype undoCount() const { return m_undo.size(); }
    [[nodiscard]] qsizetype redoCount() const { return m_redo.size(); }

private:
    qsizetype m_limit;
    QVector<SegmentEditStep> m_undo;
    QVector<SegmentEditStep> m_redo;
};

struct ProgressMapPoint {
    double progressMeters = 0.0;
    QPointF point; // normalized map coordinates, as drawn by the track map
};

struct ProgressPick {
    std::optional<double> progressMeters;
    QString reason; // "noTrace", "farFromTrack" or "ambiguous" when no progress is returned
};

// Maps a point on the track map to track progress: the nearest sample within
// `maximumDistance`. Refused as ambiguous when another sample more than
// `separationMeters` away along the track is within `ambiguityMargin` of the
// nearest distance (a crossing, or a nearby parallel section).
[[nodiscard]] ProgressPick pickProgressAt(const QVector<ProgressMapPoint> &trace, const QPointF &target,
    double maximumDistance, double ambiguityMargin, double separationMeters, double lengthMeters);

} // namespace FlappedEar
