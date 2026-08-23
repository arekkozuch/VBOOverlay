#pragma once

#include "telemetry/TelemetrySession.h"
#include "telemetry/TrackGeometry.h"

#include <QObject>
#include <QVariant>
#include <QVariantList>

namespace FlappedEar {

// A render context is deliberately independent from QMediaPlayer. It borrows
// the session and geometry from its owner, and resolves all values at an
// explicit video timestamp through the central SyncTransform.
class TelemetryRenderContext final : public QObject {
    Q_OBJECT
    Q_PROPERTY(double time READ time WRITE setTime NOTIFY timeChanged)
    Q_PROPERTY(QVariantList trackPoints READ trackPoints NOTIFY trackGeometryChanged)
    Q_PROPERTY(quint64 trackRevision READ trackRevision NOTIFY trackGeometryChanged)
    Q_PROPERTY(QVariantMap currentTrackPoint READ currentTrackPoint NOTIFY timeChanged)

public:
    explicit TelemetryRenderContext(QObject *parent = nullptr);

    [[nodiscard]] double time() const;
    [[nodiscard]] QVariantList trackPoints() const;
    [[nodiscard]] quint64 trackRevision() const;
    [[nodiscard]] quint64 trackConversionCount() const;
    [[nodiscard]] QVariantMap currentTrackPoint() const;
    [[nodiscard]] const TelemetrySession *session() const;
    [[nodiscard]] SyncTransform syncTransform() const;

    void setSession(const TelemetrySession *session);
    void setTrackGeometry(const TrackGeometry *geometry);
    void setSyncTransform(SyncTransform transform);

    Q_INVOKABLE QVariant telemetryValue(const QString &channelName) const;
    Q_INVOKABLE QString valueText(const QString &channelName, int decimals = 2) const;
    Q_INVOKABLE double telemetryTime() const;

public slots:
    void setTime(double time);

signals:
    void timeChanged();
    void sourceChanged();
    void trackGeometryChanged();
    void syncTransformChanged();

private:
    const TelemetrySession *m_session = nullptr;
    const TrackGeometry *m_geometry = nullptr;
    QVariantList m_trackPoints;
    SyncTransform m_sync;
    double m_time = 0.0;
    quint64 m_trackRevision = 0;
    quint64 m_trackConversionCount = 0;
};

} // namespace FlappedEar
