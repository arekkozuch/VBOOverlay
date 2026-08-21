#include "telemetry/TelemetryRenderContext.h"

#include <QtGlobal>
#include <cmath>

namespace FlappedEar {

TelemetryRenderContext::TelemetryRenderContext(QObject *parent)
    : QObject(parent)
{
}

double TelemetryRenderContext::time() const { return m_time; }

QVariantList TelemetryRenderContext::trackPoints() const
{
    QVariantList points;
    if (!m_geometry) {
        return points;
    }
    points.reserve(m_geometry->points.size());
    for (const QPointF &point : m_geometry->points) {
        points.append(point);
    }
    return points;
}

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
    if (m_geometry == geometry) {
        return;
    }
    m_geometry = geometry;
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
    const auto value = m_session->valueAt(channelName, telemetryTime());
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
