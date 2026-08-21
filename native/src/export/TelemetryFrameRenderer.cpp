#include "export/TelemetryFrameRenderer.h"

#include "widgets/WidgetModel.h"

#include <QCoreApplication>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QScreen>
#include <QThread>
#include <QVariantMap>

namespace FlappedEar {

TelemetryFrameRenderer::TelemetryFrameRenderer() = default;
TelemetryFrameRenderer::~TelemetryFrameRenderer() = default;

bool TelemetryFrameRenderer::initialize(
    WidgetModel *widgetModel,
    const TelemetrySession *session,
    const TrackGeometry *geometry,
    const SyncTransform sync,
    const QSize outputSize)
{
    m_error.clear();
    if (QThread::currentThread() != qApp->thread()) {
        m_error = QStringLiteral("TelemetryFrameRenderer must be initialized on the GUI thread.");
        return false;
    }
    if (!widgetModel || !session || !outputSize.isValid()) {
        m_error = QStringLiteral("A widget model, telemetry session, and valid output size are required.");
        return false;
    }

    m_context.setSession(session);
    m_context.setTrackGeometry(geometry);
    m_context.setSyncTransform(sync);
    m_outputSize = outputSize;
    m_engine = std::make_unique<QQmlEngine>();
    m_window = std::make_unique<QQuickWindow>();
    m_window->setColor(Qt::transparent);
    const qreal devicePixelRatio = QGuiApplication::primaryScreen()->devicePixelRatio();
    const QSize logicalSize(
        qMax(1, qRound(outputSize.width() / devicePixelRatio)),
        qMax(1, qRound(outputSize.height() / devicePixelRatio)));
    m_window->setGeometry(0, 0, logicalSize.width(), logicalSize.height());
    m_window->setPosition(-10'000, -10'000);

    QQmlComponent component(
        m_engine.get(), QUrl(QStringLiteral("qrc:/qt/qml/FlappedEar/qml/TelemetryScene.qml")));
    QObject *object = component.createWithInitialProperties(
        {{"renderContext", QVariant::fromValue(static_cast<QObject *>(&m_context))},
         {"widgetModel", QVariant::fromValue(static_cast<QObject *>(widgetModel))}});
    if (!object) {
        m_error = component.errorString();
        return false;
    }
    m_rootItem = qobject_cast<QQuickItem *>(object);
    if (!m_rootItem) {
        delete object;
        m_error = QStringLiteral("TelemetryScene root is not a QQuickItem.");
        return false;
    }
    m_rootItem->setParentItem(m_window->contentItem());
    m_rootItem->setSize(logicalSize);
    // QQuickWindow::grabWindow() is the documented image path for Qt Quick's
    // software adaptation. The export helper owns this hidden window, so it
    // never competes with or changes the editor's visible scene graph.
    m_window->show();
    QCoreApplication::processEvents(QEventLoop::AllEvents);
    return true;
}

QImage TelemetryFrameRenderer::renderFrame(const double sourceVideoTime)
{
    if (!m_rootItem || !m_window) {
        m_error = QStringLiteral("TelemetryFrameRenderer has not been initialized.");
        return {};
    }
    if (QThread::currentThread() != qApp->thread()) {
        m_error = QStringLiteral("Telemetry frames must be rendered on the GUI thread.");
        return {};
    }
    m_context.setTime(sourceVideoTime);
    QCoreApplication::processEvents(QEventLoop::AllEvents);
    const QImage frame = m_window->grabWindow();
    return frame.size() == m_outputSize
        ? frame
        : frame.scaled(m_outputSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
}

QString TelemetryFrameRenderer::errorString() const { return m_error; }

} // namespace FlappedEar
