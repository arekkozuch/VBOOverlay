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
    Q_PROPERTY(QVariantList trackPoints READ trackPoints NOTIFY sourceChanged)
    Q_PROPERTY(QVariantMap currentTrackPoint READ currentTrackPoint NOTIFY timeChanged)

public:
    explicit TelemetryRenderContext(QObject *parent = nullptr);

    [[nodiscard]] double time() const;
    [[nodiscard]] QVariantList trackPoints() const;
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
    void syncTransformChanged();

private:
    const TelemetrySession *m_session = nullptr;
    const TrackGeometry *m_geometry = nullptr;
    SyncTransform m_sync;
    double m_time = 0.0;
};

} // namespace FlappedEar
