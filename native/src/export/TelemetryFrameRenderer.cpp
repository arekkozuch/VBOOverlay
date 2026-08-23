#include "export/TelemetryFrameRenderer.h"

#include "widgets/WidgetModel.h"
#include "export/ExportFormat.h"

#include <QElapsedTimer>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickRenderControl>
#include <QQuickRenderTarget>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QThread>
#include <QVariantMap>
#include <rhi/qrhi.h>

namespace FlappedEar {

class TelemetryFrameRenderer::Impl final {
public:
    ~Impl()
    {
        // QRhi resources must go away while the render control's RHI remains valid.
        renderTarget.reset();
        renderPassDescriptor.reset();
        depthStencil.reset();
        colorTexture.reset();
        if (renderControl) {
            renderControl->invalidate();
        }
        window.reset();
        renderControl.reset();
    }

    std::unique_ptr<QQuickRenderControl> renderControl;
    std::unique_ptr<QQuickWindow> window;
    std::unique_ptr<QRhiTexture> colorTexture;
    std::unique_ptr<QRhiRenderBuffer> depthStencil;
    std::unique_ptr<QRhiRenderPassDescriptor> renderPassDescriptor;
    std::unique_ptr<QRhiTextureRenderTarget> renderTarget;
    QString graphicsApi;
    RendererCapabilityResult capability;
    TimingMetrics timings;
};

namespace {

QString friendlyGraphicsApiName(const QSGRendererInterface::GraphicsApi api)
{
    switch (api) {
    case QSGRendererInterface::Metal: return QStringLiteral("Metal");
    case QSGRendererInterface::Direct3D11: return QStringLiteral("Direct3D 11");
    case QSGRendererInterface::Direct3D12: return QStringLiteral("Direct3D 12");
    case QSGRendererInterface::Vulkan: return QStringLiteral("Vulkan");
    case QSGRendererInterface::OpenGL: return QStringLiteral("OpenGL");
    case QSGRendererInterface::OpenVG: return QStringLiteral("OpenVG");
    case QSGRendererInterface::Software: return QStringLiteral("Software");
    case QSGRendererInterface::Null: return QStringLiteral("Null");
    case QSGRendererInterface::Unknown: return QStringLiteral("Unknown");
    }
    return QStringLiteral("Unknown");
}

} // namespace

TelemetryFrameRenderer::TelemetryFrameRenderer() = default;
TelemetryFrameRenderer::~TelemetryFrameRenderer()
{
    m_rootItem = nullptr;
    m_impl.reset();
}

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
    m_rootItem = nullptr;
    m_impl.reset();
    m_engine = std::make_unique<QQmlEngine>();
    m_impl = std::make_unique<Impl>();
    m_impl->renderControl = std::make_unique<QQuickRenderControl>();
    m_impl->window = std::make_unique<QQuickWindow>(m_impl->renderControl.get());
    m_impl->window->setColor(Qt::transparent);
    // The offscreen target uses physical export pixels. A DPR of one makes
    // the QML scene's logical geometry identical to those pixels.
    m_impl->window->setGeometry(0, 0, outputSize.width(), outputSize.height());

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
    m_rootItem->setParentItem(m_impl->window->contentItem());
    m_rootItem->setSize(outputSize);

    if (!m_impl->renderControl->initialize()) {
        m_error = QStringLiteral("Could not initialize Qt Quick's offscreen RHI renderer.");
        m_rootItem = nullptr;
        m_impl.reset();
        return false;
    }
    QRhi *rhi = m_impl->renderControl->rhi();
    if (!rhi) {
        m_error = QStringLiteral("Qt Quick offscreen renderer did not provide a QRhi instance.");
        m_rootItem = nullptr;
        m_impl.reset();
        return false;
    }
    m_impl->graphicsApi = friendlyGraphicsApiName(m_impl->window->rendererInterface()->graphicsApi());
    m_impl->capability = evaluateCapability(
        outputSize, rhi->resourceLimit(QRhi::TextureSizeMax), m_impl->graphicsApi);
    if (!m_impl->capability.supported) {
        m_error = m_impl->capability.error;
        m_rootItem = nullptr;
        m_impl.reset();
        return false;
    }
    m_impl->colorTexture.reset(rhi->newTexture(
        QRhiTexture::RGBA8, outputSize, 1,
        QRhiTexture::RenderTarget | QRhiTexture::UsedAsTransferSource));
    m_impl->depthStencil.reset(rhi->newRenderBuffer(QRhiRenderBuffer::DepthStencil, outputSize));
    if (!m_impl->colorTexture->create() || !m_impl->depthStencil->create()) {
        m_error = QStringLiteral("Could not create Qt Quick offscreen render target resources.");
        m_rootItem = nullptr;
        m_impl.reset();
        return false;
    }
    QRhiTextureRenderTargetDescription description(
        QRhiColorAttachment(m_impl->colorTexture.get()), m_impl->depthStencil.get());
    m_impl->renderTarget.reset(rhi->newTextureRenderTarget(description));
    m_impl->renderPassDescriptor.reset(m_impl->renderTarget->newCompatibleRenderPassDescriptor());
    m_impl->renderTarget->setRenderPassDescriptor(m_impl->renderPassDescriptor.get());
    if (!m_impl->renderTarget->create()) {
        m_error = QStringLiteral("Could not create Qt Quick offscreen texture render target.");
        m_rootItem = nullptr;
        m_impl.reset();
        return false;
    }
    QQuickRenderTarget target = QQuickRenderTarget::fromRhiRenderTarget(m_impl->renderTarget.get());
    target.setDevicePixelRatio(1.0);
    m_impl->window->setRenderTarget(target);
    return true;
}

QImage TelemetryFrameRenderer::renderFrame(const double sourceVideoTime)
{
    if (!m_rootItem || !m_impl || !m_impl->window) {
        m_error = QStringLiteral("TelemetryFrameRenderer has not been initialized.");
        return {};
    }
    if (QThread::currentThread() != qApp->thread()) {
        m_error = QStringLiteral("Telemetry frames must be rendered on the GUI thread.");
        return {};
    }
    m_context.setTime(sourceVideoTime);
    QElapsedTimer timer;
    timer.start();
    m_impl->renderControl->polishItems();
    m_impl->timings.polishNanoseconds += timer.nsecsElapsed();

    timer.restart();
    m_impl->renderControl->beginFrame();
    if (!m_impl->renderControl->sync()) {
        m_impl->renderControl->endFrame();
        m_error = QStringLiteral("Qt Quick could not synchronize the offscreen scene.");
        return {};
    }
    m_impl->renderControl->render();
    m_impl->timings.syncRenderNanoseconds += timer.nsecsElapsed();

    timer.restart();
    QRhiReadbackResult readback;
    QRhiResourceUpdateBatch *updates = m_impl->renderControl->rhi()->nextResourceUpdateBatch();
    updates->readBackTexture(QRhiReadbackDescription(m_impl->colorTexture.get()), &readback);
    m_impl->renderControl->commandBuffer()->resourceUpdate(updates);
    m_impl->renderControl->endFrame();
    m_impl->timings.readbackNanoseconds += timer.nsecsElapsed();
    ++m_impl->timings.frames;
    const auto expectedBytes = ExportFormat::rgbaFrameBytes(m_outputSize);
    if (!expectedBytes || readback.pixelSize != m_outputSize
        || readback.data.size() < *expectedBytes) {
        m_error = QStringLiteral("Qt Quick offscreen texture readback returned an invalid frame.");
        return {};
    }
    // Transfer the QRhi readback allocation to the image. This avoids a second
    // full-frame CPU copy while keeping the pixels alive independently of the
    // next render call.
    auto *pixels = new QByteArray(std::move(readback.data));
    return QImage(reinterpret_cast<const uchar *>(pixels->constData()), m_outputSize.width(),
                  m_outputSize.height(), m_outputSize.width() * 4, QImage::Format_RGBA8888,
                  [](void *data) { delete static_cast<QByteArray *>(data); }, pixels);
}

QString TelemetryFrameRenderer::errorString() const { return m_error; }
QString TelemetryFrameRenderer::graphicsApiName() const
{
    return m_impl ? m_impl->graphicsApi : QString{};
}
RendererCapabilityResult TelemetryFrameRenderer::capability() const
{
    return m_impl ? m_impl->capability : RendererCapabilityResult{};
}

RendererCapabilityResult TelemetryFrameRenderer::evaluateCapability(
    const QSize &size, const int maximumTextureSize, const QString &backend)
{
    RendererCapabilityResult result;
    result.requestedSize = size;
    result.maximumTextureSize = maximumTextureSize;
    result.backend = backend;
    const auto frameBytes = ExportFormat::rgbaFrameBytes(size);
    if (!frameBytes) {
        result.error = QStringLiteral("Requested render raster %1×%2 has an invalid or overflowing RGBA frame size.")
                           .arg(size.width()).arg(size.height());
        return result;
    }
    result.frameBytes = *frameBytes;
    result.pixelCount = *frameBytes / 4;
    if (maximumTextureSize <= 0) {
        result.error = QStringLiteral("The %1 renderer did not report a usable maximum texture size.")
                           .arg(backend.isEmpty() ? QStringLiteral("active") : backend);
        return result;
    }
    if (size.width() > maximumTextureSize || size.height() > maximumTextureSize) {
        result.error = QStringLiteral(
            "Requested render raster %1×%2 exceeds the %3 backend texture limit of %4 pixels per dimension.")
                           .arg(size.width()).arg(size.height())
                           .arg(backend.isEmpty() ? QStringLiteral("active") : backend)
                           .arg(maximumTextureSize);
        return result;
    }
    result.supported = true;
    return result;
}
TelemetryFrameRenderer::TimingMetrics TelemetryFrameRenderer::timingMetrics() const
{
    return m_impl ? m_impl->timings : TimingMetrics{};
}

} // namespace FlappedEar
