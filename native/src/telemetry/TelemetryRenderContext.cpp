#include "telemetry/TelemetryRenderContext.h"

#include <QtGlobal>
#include <algorithm>
#include <cmath>

namespace FlappedEar {

namespace {

struct PresentationPolicy {
    double smoothingSeconds;
    double staleSeconds;
    InterpolationMode interpolation;
};

PresentationPolicy presentationPolicy(const QString &channelName)
{
    const QString name = channelName.toLower();
    if (name.contains(QStringLiteral("throttle")) || name.contains(QStringLiteral("brake")))
        return {0.10, 0.75, InterpolationMode::Linear};
    if (name.contains(QStringLiteral("lateral")) || name.contains(QStringLiteral("longitudinal"))
        || name.contains(QStringLiteral("accel")) || name.contains(QStringLiteral("gforce")))
        return {0.20, 0.75, InterpolationMode::Linear};
    if (name.contains(QStringLiteral("heart")) || name.contains(QStringLiteral("hr"))
        || name.contains(QStringLiteral("bpm")))
        return {0.25, 2.0, InterpolationMode::Linear};
    if (name.contains(QStringLiteral("gear")))
        return {0.0, 0.75, InterpolationMode::Previous};
    if (name.contains(QStringLiteral("speed")) || name.contains(QStringLiteral("rpm")))
        return {0.15, 0.75, InterpolationMode::Linear};
    return {0.15, 0.75, InterpolationMode::Linear};
}

std::optional<double> presentationValueAt(
    const TelemetrySession &session, const QString &channelName, const double time)
{
    const QString resolved = session.aliases.value(channelName, channelName);
    const auto channelIterator = session.channels.constFind(resolved);
    if (channelIterator == session.channels.cend()) return std::nullopt;
    const TelemetryChannel &channel = channelIterator.value();
    if (channel.timestamps.size() != channel.values.size() || channel.timestamps.isEmpty())
        return std::nullopt;

    const PresentationPolicy policy = presentationPolicy(channelName + QLatin1Char(' ') + resolved);
    std::optional<double> presented = session.valueAt(channelName, time, policy.interpolation);
    const auto next = std::lower_bound(channel.timestamps.cbegin(), channel.timestamps.cend(), time);
    if (presented && next != channel.timestamps.cbegin() && next != channel.timestamps.cend()
        && *next != time) {
        const double gap = *next - *(next - 1);
        if (gap > telemetryGapThreshold(channel, policy.staleSeconds)) presented.reset();
    }
    if (!presented) {
        const auto after = std::upper_bound(channel.timestamps.cbegin(), channel.timestamps.cend(), time);
        for (auto iterator = after; iterator != channel.timestamps.cbegin();) {
            --iterator;
            const qsizetype index = std::distance(channel.timestamps.cbegin(), iterator);
            if (time - *iterator > policy.staleSeconds) break;
            if (std::isfinite(channel.values[index])) {
                presented = channel.values[index];
                break;
            }
        }
    }
    if (!presented || policy.smoothingSeconds <= 0.0) return presented;

    double weightedTotal = *presented;
    double totalWeight = 1.0;
    const double windowStart = time - policy.smoothingSeconds;
    auto iterator = std::lower_bound(channel.timestamps.cbegin(), channel.timestamps.cend(), windowStart);
    for (; iterator != channel.timestamps.cend() && *iterator <= time; ++iterator) {
        const qsizetype index = std::distance(channel.timestamps.cbegin(), iterator);
        if (qFuzzyCompare(*iterator, time) || !std::isfinite(channel.values[index])) continue;
        const double weight = std::max(0.0, 1.0 - (time - *iterator) / policy.smoothingSeconds);
        weightedTotal += channel.values[index] * weight;
        totalWeight += weight;
    }
    const double smoothed = weightedTotal / totalWeight;
    return std::isfinite(smoothed) ? std::optional<double>(smoothed) : std::nullopt;
}

} // namespace

TelemetryRenderContext::TelemetryRenderContext(QObject *parent)
    : QObject(parent)
{
}

double TelemetryRenderContext::time() const { return m_time; }

QVariantList TelemetryRenderContext::trackPoints() const
{
    return m_trackPoints;
}

quint64 TelemetryRenderContext::trackRevision() const { return m_trackRevision; }
quint64 TelemetryRenderContext::trackConversionCount() const { return m_trackConversionCount; }

QVariantMap TelemetryRenderContext::currentTrackPoint() const
{
    if (!m_session || !m_geometry) {
        return {};
    }
    const auto point = FlappedEar::currentTrackPoint(*m_session, telemetryTime(), *m_geometry);
    return point ? QVariantMap{{"x", point->x()}, {"y", point->y()}} : QVariantMap();
}

const TelemetrySession *TelemetryRenderContext::session() const { return m_session; }
SyncTransform TelemetryRenderContext::syncTransform() const { return m_sync; }

void TelemetryRenderContext::setSession(const TelemetrySession *session)
{
    if (m_session == session) {
        return;
    }
    m_session = session;
    emit sourceChanged();
    emit timeChanged();
}

void TelemetryRenderContext::setTrackGeometry(const TrackGeometry *geometry)
{
    m_geometry = geometry;
    m_trackPoints.clear();
    if (m_geometry) {
        m_trackPoints.reserve(m_geometry->points.size());
        for (const QPointF &point : m_geometry->points) {
            m_trackPoints.append(point);
        }
        ++m_trackConversionCount;
    }
    ++m_trackRevision;
    emit trackGeometryChanged();
    emit sourceChanged();
    emit timeChanged();
}

void TelemetryRenderContext::setSyncTransform(const SyncTransform transform)
{
    if (!std::isfinite(transform.offset) || !std::isfinite(transform.timeScale)
        || transform.timeScale <= 0.0
        || (qFuzzyCompare(m_sync.offset, transform.offset)
            && qFuzzyCompare(m_sync.timeScale, transform.timeScale))) {
        return;
    }
    m_sync = transform;
    emit syncTransformChanged();
    emit timeChanged();
}

QVariant TelemetryRenderContext::telemetryValue(const QString &channelName) const
{
    if (!m_session || channelName.isEmpty()) {
        return {};
    }
    const auto value = presentationValueAt(*m_session, channelName, telemetryTime());
    return value ? QVariant(*value) : QVariant();
}

QString TelemetryRenderContext::valueText(const QString &channelName, const int decimals) const
{
    const QVariant value = telemetryValue(channelName);
    return value.isValid() ? QString::number(value.toDouble(), 'f', qBound(0, decimals, 6))
                           : QStringLiteral("—");
}

double TelemetryRenderContext::telemetryTime() const
{
    return videoToTelemetryTime(m_time, m_sync);
}

void TelemetryRenderContext::setTime(const double time)
{
    if (!std::isfinite(time) || qFuzzyCompare(m_time, time)) {
        return;
    }
    m_time = time;
    emit timeChanged();
}

} // namespace FlappedEar
