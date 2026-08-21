#include "app/AppController.h"
#include "export/TelemetryFrameRenderer.h"
#include "export/ExportEngine.h"
#include "telemetry/VboParser.h"
#include "telemetry/TrackGeometry.h"
#include "widgets/WidgetModel.h"

#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>

namespace {

int renderStill(const QString &path)
{
    const FlappedEar::TelemetrySession session = FlappedEar::VboParser::parse(
        u"[column names]\ntime speed\n[data]\n0 0\n10 100");
    FlappedEar::WidgetModel widgets;
    widgets.resetDefaults();
    FlappedEar::TelemetryFrameRenderer renderer;
    if (!renderer.initialize(
            &widgets, &session, nullptr, FlappedEar::SyncTransform{}, QSize(320, 180))) {
        qCritical().noquote() << renderer.errorString();
        return EXIT_FAILURE;
    }
    const QImage image = renderer.renderFrame(10.0);
    if (image.isNull() || !image.save(path)) {
        qCritical() << "Could not render telemetry still:" << renderer.errorString();
        return EXIT_FAILURE;
    }
    qInfo().noquote() << QStringLiteral("Telemetry still rendered to %1 (%2×%3).")
                              .arg(path)
                              .arg(image.width())
                              .arg(image.height());
    return EXIT_SUCCESS;
}

int exportTest(const QString &inputPath, const QString &outputPath)
{
    const FlappedEar::TelemetrySession session = FlappedEar::VboParser::parse(
        u"[column names]\ntime speed\n[data]\n0 0\n10 100");
    FlappedEar::WidgetModel widgets;
    widgets.resetDefaults();
    const FlappedEar::MediaInfo input = FlappedEar::MediaProbe::probe(inputPath);
    FlappedEar::TelemetryFrameRenderer renderer;
    if (!renderer.initialize(
            &widgets, &session, nullptr, FlappedEar::SyncTransform{}, input.videoSize)) {
        qCritical().noquote() << renderer.errorString();
        return EXIT_FAILURE;
    }
    FlappedEar::ExportSettings settings;
    settings.inputPath = inputPath;
    settings.outputPath = outputPath;
    settings.outputSize = input.videoSize;
    settings.frameRate = input.averageFrameRate;
    settings.endTime = input.duration;
    const FlappedEar::ExportResult result = FlappedEar::ExportEngine::exportVideo(settings, renderer);
    if (!result.success) {
        qCritical().noquote() << result.error;
        return EXIT_FAILURE;
    }
    qInfo().noquote() << QStringLiteral("HEVC export completed: %1 frames.").arg(result.renderedFrames);
    return EXIT_SUCCESS;
}

} // namespace

int main(int argc, char *argv[])
{
    const bool renderStillMode = argc == 3 && QString::fromLocal8Bit(argv[1]) == "--render-still";
    const bool exportTestMode = argc == 4 && QString::fromLocal8Bit(argv[1]) == "--export-test";
    if (renderStillMode || exportTestMode) {
        qputenv("QT_QUICK_BACKEND", "software");
    }
    QQuickStyle::setStyle(QStringLiteral("Basic"));
    QGuiApplication app(argc, argv);
    QCoreApplication::setOrganizationName("FlappedEar");
    QCoreApplication::setOrganizationDomain("flappedear.com");
    QCoreApplication::setApplicationName("FlappedEar Telemetry");
    app.setWindowIcon(QIcon(QStringLiteral(":/flappedear/resources/branding/app-logo.png")));
    if (renderStillMode) {
        return renderStill(QString::fromLocal8Bit(argv[2]));
    }
    if (exportTestMode) {
        return exportTest(QString::fromLocal8Bit(argv[2]), QString::fromLocal8Bit(argv[3]));
    }
    FlappedEar::AppController controller;
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("appController", &controller);
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        [] { QCoreApplication::exit(EXIT_FAILURE); },
        Qt::QueuedConnection);
    engine.loadFromModule("FlappedEar", "Main");
    return app.exec();
}
