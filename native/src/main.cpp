#include "app/AppController.h"
#include "export/TelemetryFrameRenderer.h"
#include "export/ExportEngine.h"
#include "telemetry/VboParser.h"
#include "telemetry/TrackGeometry.h"
#include "widgets/WidgetModel.h"

#include <QGuiApplication>
#include <QIcon>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QTextStream>

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

void writeExportEvent(const QJsonObject &event)
{
    QTextStream stream(stdout);
    stream << QJsonDocument(event).toJson(QJsonDocument::Compact) << '\n' << Qt::flush;
}

int exportWorker(const QString &configPath)
{
    QFile configFile(configPath);
    if (!configFile.open(QIODevice::ReadOnly)) {
        writeExportEvent({{"state", "failed"}, {"error", "Could not open export configuration."}});
        return EXIT_FAILURE;
    }
    const QJsonObject config = QJsonDocument::fromJson(configFile.readAll()).object();
    try {
        const FlappedEar::TelemetrySession session = FlappedEar::VboParser::parseFile(
            config.value("vboPath").toString());
        FlappedEar::WidgetModel widgets;
        if (!widgets.fromJson(config.value("widgets").toArray())) {
            writeExportEvent({{"state", "failed"}, {"error", "Widget scene is invalid."}});
            return EXIT_FAILURE;
        }
        const FlappedEar::TrackGeometry geometry = FlappedEar::buildTrackGeometry(session);
        const QJsonObject syncJson = config.value("sync").toObject();
        const FlappedEar::SyncTransform sync{
            syncJson.value("offset").toDouble(), syncJson.value("timeScale").toDouble(1.0)};
        const FlappedEar::MediaInfo input = FlappedEar::MediaProbe::probe(
            config.value("inputPath").toString());
        FlappedEar::TelemetryFrameRenderer renderer;
        if (!renderer.initialize(&widgets, &session, &geometry, sync, input.videoSize)) {
            writeExportEvent({{"state", "failed"}, {"error", renderer.errorString()}});
            return EXIT_FAILURE;
        }
        FlappedEar::ExportSettings settings;
        settings.inputPath = config.value("inputPath").toString();
        settings.outputPath = config.value("outputPath").toString();
        settings.outputSize = input.videoSize;
        settings.frameRate = input.averageFrameRate;
        settings.endTime = input.duration;
        settings.quality = config.value("quality").toString("high");
        settings.audioEnabled = config.value("audioEnabled").toBool(true);
        settings.cancellationFilePath = config.value("cancelPath").toString();
        writeExportEvent({{"state", "rendering"}, {"current", 0},
                          {"total", static_cast<qint64>(FlappedEar::ExportEngine::frameCount(
                                        settings.startTime, settings.endTime, settings.frameRate))}});
        settings.progressCallback = [](const qsizetype current, const qsizetype total) {
            writeExportEvent(
                {{"state", "rendering"}, {"current", static_cast<qint64>(current)},
                 {"total", static_cast<qint64>(total)}});
            return true;
        };
        settings.stateCallback = [](const QString &state) {
            writeExportEvent({{"state", state}});
        };
        const FlappedEar::ExportResult result = FlappedEar::ExportEngine::exportVideo(settings, renderer);
        if (result.cancelled) {
            writeExportEvent({{"state", "cancelled"}});
            return EXIT_SUCCESS;
        }
        if (!result.success) {
            writeExportEvent({{"state", "failed"}, {"error", result.error}});
            return EXIT_FAILURE;
        }
        writeExportEvent({{"state", "finished"}, {"current", static_cast<qint64>(result.renderedFrames)},
                          {"total", static_cast<qint64>(result.renderedFrames)}});
        return EXIT_SUCCESS;
    } catch (const std::exception &error) {
        writeExportEvent(
            {{"state", "failed"}, {"error", QString::fromUtf8(error.what())}});
        return EXIT_FAILURE;
    }
}

} // namespace

int main(int argc, char *argv[])
{
    const bool renderStillMode = argc == 3 && QString::fromLocal8Bit(argv[1]) == "--render-still";
    const bool exportTestMode = argc == 4 && QString::fromLocal8Bit(argv[1]) == "--export-test";
    const bool exportWorkerMode = argc == 3 && QString::fromLocal8Bit(argv[1]) == "--export-worker";
    if (renderStillMode || exportTestMode || exportWorkerMode) {
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
    if (exportWorkerMode) {
        return exportWorker(QString::fromLocal8Bit(argv[2]));
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
