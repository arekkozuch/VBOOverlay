#include "app/AppController.h"
#include "export/TelemetryFrameRenderer.h"
#include "export/ExportEngine.h"
#include "export/ExportProgress.h"
#include "telemetry/VboParser.h"
#include "telemetry/TrackGeometry.h"
#include "widgets/WidgetModel.h"

#include <QGuiApplication>
#include <QIcon>
#include <QFile>
#include <QElapsedTimer>
#include <QFileInfo>
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
    qInfo().noquote() << QStringLiteral(
        "HEVC export completed: %1 submitted / %10 encoded frames in %2 ms (%3 fps); render=%4 ms, polish=%5 ms, "
        "syncRender=%6 ms, readback=%7 ms, cpuCopy=%8 ms, ffmpegWrite=%9 ms, maxQueued=%11 MiB.")
                             .arg(result.renderedFrames).arg(result.elapsedMilliseconds)
                             .arg(result.renderedFrames * 1000.0 / qMax<qint64>(1, result.elapsedMilliseconds), 0, 'f', 2)
                             .arg(result.renderMilliseconds)
                             .arg(result.polishNanoseconds / 1'000'000)
                             .arg(result.syncRenderNanoseconds / 1'000'000)
                             .arg(result.readbackNanoseconds / 1'000'000)
                             .arg(result.cpuCopyNanoseconds / 1'000'000)
                             .arg(result.ffmpegWriteNanoseconds / 1'000'000)
                             .arg(result.encodedFrames)
                             .arg(result.maximumQueuedBytes / (1024.0 * 1024.0), 0, 'f', 1);
    return EXIT_SUCCESS;
}

int benchmarkRender(const QSize size, const int frames)
{
    const FlappedEar::TelemetrySession session = FlappedEar::VboParser::parse(
        u"[column names]\ntime speed\n[data]\n0 0\n10 100");
    FlappedEar::WidgetModel widgets;
    widgets.resetDefaults();
    FlappedEar::TelemetryFrameRenderer renderer;
    if (!renderer.initialize(&widgets, &session, nullptr, FlappedEar::SyncTransform{}, size)) {
        qCritical().noquote() << renderer.errorString();
        return EXIT_FAILURE;
    }
    QElapsedTimer elapsed;
    elapsed.start();
    for (int frame = 0; frame < frames; ++frame) {
        if (renderer.renderFrame(static_cast<double>(frame) / 60.0).isNull()) {
            qCritical().noquote() << renderer.errorString();
            return EXIT_FAILURE;
        }
    }
    const auto metrics = renderer.timingMetrics();
    const double milliseconds = elapsed.nsecsElapsed() / 1'000'000.0;
    const auto perFrame = [frames](const qint64 nanoseconds) {
        return nanoseconds / 1'000'000.0 / qMax(1, frames);
    };
    qInfo().noquote() << QStringLiteral(
        "Renderer benchmark: backend=Qt Quick RHI graphicsApi=%1 resolution=%2x%3 frames=%4 "
        "elapsedMs=%5 fps=%6 totalMsPerFrame=%7 polishMsPerFrame=%8 syncRenderMsPerFrame=%9 readbackMsPerFrame=%10")
                             .arg(renderer.graphicsApiName()).arg(size.width()).arg(size.height())
                             .arg(frames).arg(milliseconds, 0, 'f', 1)
                             .arg(frames * 1000.0 / milliseconds, 0, 'f', 2)
                             .arg(milliseconds / frames, 0, 'f', 2)
                             .arg(perFrame(metrics.polishNanoseconds), 0, 'f', 2)
                             .arg(perFrame(metrics.syncRenderNanoseconds), 0, 'f', 2)
                             .arg(perFrame(metrics.readbackNanoseconds), 0, 'f', 2);
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
        settings.startTime = config.value("startTime").toDouble();
        settings.endTime = config.value("endTime").toDouble(input.duration);
        settings.quality = config.value("quality").toString("high");
        settings.audioEnabled = config.value("audioEnabled").toBool(true);
        settings.cancellationFilePath = config.value("cancelPath").toString();
        const qsizetype totalFrames = FlappedEar::ExportEngine::frameCount(
            settings.startTime, settings.endTime, settings.frameRate);
        FlappedEar::ExportProgressEstimator rendererProgress;
        QElapsedTimer elapsed;
        elapsed.start();
        qint64 lastUpdate = -125;
        writeExportEvent({{"state", "preparing"}, {"sourceTime", settings.startTime},
                          {"endTime", settings.endTime}, {"totalFrames", static_cast<qint64>(totalFrames)},
                          {"width", input.videoSize.width()}, {"height", input.videoSize.height()},
                          {"frameRate", settings.frameRate.value()}, {"audioEnabled", settings.audioEnabled}});
        settings.progressCallback = [&](const FlappedEar::ExportPipelineProgress &pipeline) {
            const qint64 elapsedMilliseconds = elapsed.elapsed();
            if (pipeline.submittedFrames != pipeline.totalFrames
                && elapsedMilliseconds - lastUpdate < 125) return;
            lastUpdate = elapsedMilliseconds;
            const auto rendererSnapshot = rendererProgress.update(
                pipeline.submittedFrames, pipeline.totalFrames, elapsedMilliseconds, settings.frameRate);
            const double encodedPercent = FlappedEar::FfmpegProgressParser::overallPercent(
                pipeline.encodedSeconds, pipeline.outputDurationSeconds);
            const double visiblePercent = pipeline.stage == QStringLiteral("finalizing")
                ? 97.0 : encodedPercent;
            QJsonObject event{{"state", pipeline.stage},
                              {"renderedFrames", static_cast<qint64>(pipeline.submittedFrames)},
                              {"totalFrames", static_cast<qint64>(pipeline.totalFrames)},
                              {"sourceTime", pipeline.submittedSourceTime},
                              {"endTime", settings.endTime},
                              {"telemetryTime", FlappedEar::videoToTelemetryTime(pipeline.submittedSourceTime, sync)},
                              {"encodedFrames", static_cast<qint64>(pipeline.encodedFrames)},
                              {"encodedSeconds", pipeline.encodedSeconds},
                              {"encodedProgress", encodedPercent},
                              {"encoderFps", pipeline.encoderFps},
                              {"encoderRealtimeFactor", pipeline.encoderRealtimeFactor},
                              {"queuedBytes", pipeline.queuedBytes},
                              {"maximumQueuedBytes", pipeline.maximumQueuedBytes},
                              {"elapsedMilliseconds", elapsedMilliseconds},
                              {"visibleProgress", visiblePercent},
                              {"outputBytes", QFileInfo(settings.outputPath).size()}};
            if (rendererSnapshot.etaAvailable) {
                event.insert("rendererFps", rendererSnapshot.throughputFps);
            }
            writeExportEvent(event);
        };
        settings.stateCallback = [](const QString &state) {
            const QString progressState = state == "encoding" ? QStringLiteral("finalizing")
                : state == "validating" ? QStringLiteral("validating")
                                      : QStringLiteral("preparing");
            writeExportEvent({{"state", progressState}});
        };
        settings.encoderCallback = [](const QString &id, const QString &name) {
            writeExportEvent({{"state", "preparing"}, {"encoderId", id}, {"encoderName", name}});
        };
        const FlappedEar::ExportResult result = FlappedEar::ExportEngine::exportVideo(settings, renderer);
        if (result.cancelled) {
            writeExportEvent({{"state", "cancelled"}, {"elapsedMilliseconds", elapsed.elapsed()}});
            return EXIT_SUCCESS;
        }
        if (!result.success) {
            writeExportEvent({{"state", "failed"}, {"error", result.error}, {"diagnostics", result.diagnostics}});
            return EXIT_FAILURE;
        }
        writeExportEvent({{"state", "complete"}, {"renderedFrames", static_cast<qint64>(result.renderedFrames)},
                          {"totalFrames", static_cast<qint64>(result.renderedFrames)},
                          {"elapsedMilliseconds", result.elapsedMilliseconds},
                          {"renderMilliseconds", result.renderMilliseconds},
                          {"renderNanoseconds", result.renderNanoseconds},
                          {"polishNanoseconds", result.polishNanoseconds},
                          {"syncRenderNanoseconds", result.syncRenderNanoseconds},
                          {"readbackNanoseconds", result.readbackNanoseconds},
                          {"cpuCopyNanoseconds", result.cpuCopyNanoseconds},
                          {"ffmpegWriteNanoseconds", result.ffmpegWriteNanoseconds},
                          {"maximumQueuedBytes", result.maximumQueuedBytes},
                          {"encodedFrames", static_cast<qint64>(result.encodedFrames)},
                          {"encodedSeconds", result.encodedSeconds},
                          {"renderedFrames", static_cast<qint64>(result.renderedFrames)}});
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
    const bool benchmarkRenderMode = argc == 5 && QString::fromLocal8Bit(argv[1]) == "--benchmark-render";
    if (qEnvironmentVariableIntValue("FLAPPEDEAR_EXPORT_SOFTWARE") == 1) {
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
    if (benchmarkRenderMode) {
        bool widthOk = false;
        bool heightOk = false;
        bool framesOk = false;
        const int width = QString::fromLocal8Bit(argv[2]).toInt(&widthOk);
        const int height = QString::fromLocal8Bit(argv[3]).toInt(&heightOk);
        const int frames = QString::fromLocal8Bit(argv[4]).toInt(&framesOk);
        if (!widthOk || !heightOk || !framesOk || width <= 0 || height <= 0 || frames <= 0) {
            qCritical() << "Usage: --benchmark-render <width> <height> <frames>";
            return EXIT_FAILURE;
        }
        return benchmarkRender(QSize(width, height), frames);
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
