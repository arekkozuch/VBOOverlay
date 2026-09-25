#pragma once

#include "telemetry/CornerPhases.h"
#include "telemetry/TelemetrySession.h"
#include "telemetry/TrackProgress.h"
#include "telemetry/TrackSegmentReview.h"

#include <QString>
#include <QStringList>
#include <optional>

namespace FlappedEar {

// Corner Analyzer speeds for one approved segment on one lap (KAN-52): entry
// and exit speeds at the segment boundaries, the speed at the geometric apex
// and, separately, the lap's minimum speed inside the segment. The apex is
// never taken to be the minimum-speed point.
inline constexpr auto cornerSpeedsAlgorithm = "corner-speeds-v1";

// Speed samples further apart than this along the corner make every value
// explicitly limited ("sparseSamples"), not silently precise.
inline constexpr double sparseSampleSpacingMeters = 10.0;

inline constexpr auto cornerSpeedNotACorner = "notACorner";
inline constexpr auto cornerSpeedSparseSamples = "sparseSamples";

struct CornerSpeedValue {
    std::optional<double> value; // in the speed channel's unit
    double progressMeters = 0.0;
    std::optional<double> telemetryTime;
    QString unavailableReason;   // set when `value` is absent
    QStringList limitations;     // present values that are less certain
};

struct CornerSpeeds {
    QString segmentId;
    QString name;
    QString type;
    QString channel;
    QString unit;
    QString provenance; // "measured" when read from the recorded speed channel, otherwise "unavailable"
    CornerSpeedValue entry;
    CornerSpeedValue apex;
    CornerSpeedValue minimum;
    CornerSpeedValue exit;
    double lengthMeters = 0.0;
    double coveredMeters = 0.0;
    double meanSampleSpacingMeters = 0.0; // 0 when it could not be measured
    SegmentationResultStamp stamp;
    bool valid = false;
};

// `axis`/`features` are the shared progress axis the segments were approved
// on and its computeTrackFeatures; `lapTrace` is the lap's projection on it.
[[nodiscard]] CornerSpeeds computeCornerSpeeds(const ProgressAxis &axis, const TrackFeatures &features,
    const ApprovedSegmentation &approved, const QString &segmentId, const QVector<ProgressSegment> &lapTrace,
    const TelemetrySession &session);

} // namespace FlappedEar
