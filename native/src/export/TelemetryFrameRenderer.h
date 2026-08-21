#pragma once

#include "telemetry/TelemetryRenderContext.h"

#include <QImage>
#include <QSize>
#include <QString>
#include <memory>

class QQmlEngine;
class QQuickItem;
class QQuickWindow;

namespace FlappedEar {

class WidgetModel;

// Owns a private Qt Quick scene. All methods must be called on its owning GUI
// thread; export workers may consume the returned QImage, but never move the
// QML/scene-graph objects themselves.
class TelemetryFrameRenderer final {
public:
    TelemetryFrameRenderer();
    ~TelemetryFrameRenderer();

    TelemetryFrameRenderer(const TelemetryFrameRenderer &) = delete;
    TelemetryFrameRenderer &operator=(const TelemetryFrameRenderer &) = delete;

    [[nodiscard]] bool initialize(
        WidgetModel *widgetModel,
        const TelemetrySession *session,
        const TrackGeometry *geometry,
        SyncTransform sync,
        QSize outputSize);
    [[nodiscard]] QImage renderFrame(double sourceVideoTime);
    [[nodiscard]] QString errorString() const;

private:
    TelemetryRenderContext m_context;
    QSize m_outputSize;
    QString m_error;
    std::unique_ptr<QQmlEngine> m_engine;
    std::unique_ptr<QQuickWindow> m_window;
    QQuickItem *m_rootItem = nullptr;
};

} // namespace FlappedEar
