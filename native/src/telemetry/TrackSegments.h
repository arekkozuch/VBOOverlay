#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QString>

namespace FlappedEar {

enum class TrackSegmentType { Sector, Corner, Straight };

// Portable segments use exact progress bounds and a track-configuration
// reference, never persisted sample geometry (ProgressAxis points/cumulative
// stay in memory only). Algorithm changes that can alter validation must
// bump this tag. v2 accepts the "straight" type.
inline constexpr auto trackSegmentAlgorithm = "track-segment-v2";

[[nodiscard]] QString trackSegmentTypeName(TrackSegmentType type);

// A fresh stable ID is minted for a new segment; editing an existing
// segment's name or bounds must preserve its existing "id" rather than
// calling this again.
[[nodiscard]] QJsonObject makeTrackSegment(TrackSegmentType type, const QString &name,
    double startProgressMeters, double endProgressMeters, const QString &trackConfigurationReference);
[[nodiscard]] bool validTrackSegment(const QJsonObject &segment);

// Segments must be listed in non-decreasing start-progress order. Only the
// final (highest-start) segment may wrap across the start/finish line
// (endProgressMeters < startProgressMeters) -- that is the one physically
// meaningful case, a segment covering the gate itself.
[[nodiscard]] bool validTrackSegments(const QJsonValue &value);

// Pure content hash, never persisted: recompute and compare against a
// previously observed value to detect any add/remove/reorder/edit -- the
// "version edits" a consumer (e.g. a future cached-metric invalidation)
// checks for.
[[nodiscard]] QString trackSegmentSetRevision(const QJsonArray &segments);

inline constexpr qsizetype maximumTrackSegments = 64;

} // namespace FlappedEar
