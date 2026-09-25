#include "telemetry/TrackSegments.h"

#include <QCryptographicHash>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSet>
#include <QStringList>
#include <QUuid>

#include <cmath>

namespace FlappedEar {
namespace {

bool validSegmentText(const QJsonValue &value, const qsizetype limit)
{
    return value.isString() && !value.toString().trimmed().isEmpty()
        && !value.toString().contains(QChar::Null) && value.toString().size() <= limit;
}

constexpr qsizetype maximumTrackSegmentIdCharacters = 128;
constexpr qsizetype maximumTrackSegmentNameCharacters = 160;
// Far beyond any real track length; this only keeps an imported/edited
// document from carrying an unbounded or non-finite progress value, matching
// the rationale behind ProjectLimits::maximumComparisonRangeMeters.
constexpr double maximumTrackSegmentProgressMeters = 1'000'000.0;

} // namespace

QString trackSegmentTypeName(const TrackSegmentType type)
{
    switch (type) {
    case TrackSegmentType::Sector: return "sector";
    case TrackSegmentType::Corner: return "corner";
    case TrackSegmentType::Straight: return "straight";
    }
    return {};
}

QJsonObject makeTrackSegment(const TrackSegmentType type, const QString &name,
    const double startProgressMeters, const double endProgressMeters, const QString &trackConfigurationReference)
{
    const QJsonObject segment{{"id", QUuid::createUuid().toString(QUuid::WithoutBraces)},
        {"type", trackSegmentTypeName(type)}, {"name", name},
        {"startProgressMeters", startProgressMeters}, {"endProgressMeters", endProgressMeters},
        {"trackConfigurationReference", trackConfigurationReference}};
    return validTrackSegment(segment) ? segment : QJsonObject{};
}

bool validTrackSegment(const QJsonObject &segment)
{
    if (segment.size() != 6) return false;
    if (!validSegmentText(segment.value("id"), maximumTrackSegmentIdCharacters)) return false;
    const auto type = segment.value("type");
    if (!type.isString() || !QStringList{"sector", "corner", "straight"}.contains(type.toString())) return false;
    if (!validSegmentText(segment.value("name"), maximumTrackSegmentNameCharacters)) return false;
    const auto start = segment.value("startProgressMeters");
    const auto end = segment.value("endProgressMeters");
    if (!start.isDouble() || !end.isDouble()
        || !std::isfinite(start.toDouble()) || !std::isfinite(end.toDouble())) return false;
    if (start.toDouble() < 0.0 || start.toDouble() > maximumTrackSegmentProgressMeters
        || end.toDouble() < 0.0 || end.toDouble() > maximumTrackSegmentProgressMeters) return false;
    if (start.toDouble() == end.toDouble()) return false; // a zero-length segment carries no evidence.
    static const QRegularExpression groupPattern("^compatibility-v1:[0-9a-f]{64}$");
    const auto reference = segment.value("trackConfigurationReference");
    return reference.isString() && groupPattern.match(reference.toString()).hasMatch();
}

bool validTrackSegments(const QJsonValue &value)
{
    if (value.isUndefined() || value.isNull()) return true;
    if (!value.isArray()) return false;
    const QJsonArray segments = value.toArray();
    if (segments.size() > maximumTrackSegments) return false;
    QSet<QString> seenIds;
    double previousStart = -1.0;
    for (qsizetype index = 0; index < segments.size(); ++index) {
        const auto item = segments[index];
        if (!item.isObject() || !validTrackSegment(item.toObject())) return false;
        const auto segment = item.toObject();
        const QString id = segment.value("id").toString();
        if (seenIds.contains(id)) return false;
        seenIds.insert(id);
        const double start = segment.value("startProgressMeters").toDouble();
        const double end = segment.value("endProgressMeters").toDouble();
        if (start < previousStart) return false; // must be listed in non-decreasing start order.
        previousStart = start;
        // Only the final (highest-start) segment may wrap past the start/finish
        // line -- the one physically meaningful case, a segment covering the gate.
        if (end < start && index != segments.size() - 1) return false;
    }
    return true;
}

QString trackSegmentSetRevision(const QJsonArray &segments)
{
    const QJsonObject basis{{"version", trackSegmentAlgorithm}, {"segments", segments}};
    return "track-segments-v1:" + QString::fromLatin1(QCryptographicHash::hash(
        QJsonDocument(basis).toJson(QJsonDocument::Compact), QCryptographicHash::Sha256).toHex());
}

} // namespace FlappedEar
