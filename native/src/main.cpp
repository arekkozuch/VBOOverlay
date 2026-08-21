#include "app/AppController.h"
#include "export/TelemetryFrameRenderer.h"
#include "export/ExportEngine.h"
#include "export/ExportDiagnostics.h"
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
#include <QJsonArray>
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
        if (!result.diagnostics.isEmpty()) qCritical().noquote() << result.diagnostics;
        return EXIT_FAILURE;
    }
    if (!result.validationWarning.isEmpty()) {
        qWarning().noquote() << QStringLiteral("Video export completed, but automatic validation failed.");
        qWarning().noquote() << result.validationWarning;
        if (!result.diagnostics.isEmpty()) qWarning().noquote() << result.diagnostics;
    }
    qInfo().noquote() << QStringLiteral(
        "HEVC export completed: %1 submitted / %10 encoded frames in %2 ms (%3 fps); render=%4 ms, polish=%5 ms, "
        "syncRender=%6 ms, readback=%7 ms, cpuCopy=%8 ms, ffmpegWrite=%9 ms, maxQueued=%11 MiB, temporaryOverlay=%12 MiB.")
                             .arg(result.renderedFrames).arg(result.elapsedMilliseconds)
                             .arg(result.renderedFrames * 1000.0 / qMax<qint64>(1, result.elapsedMilliseconds), 0, 'f', 2)
                             .arg(result.renderMilliseconds)
                             .arg(result.polishNanoseconds / 1'000'000)
                             .arg(result.syncRenderNanoseconds / 1'000'000)
                             .arg(result.readbackNanoseconds / 1'000'000)
                             .arg(result.cpuCopyNanoseconds / 1'000'000)
                             .arg(result.ffmpegWriteNanoseconds / 1'000'000)
                             .arg(result.encodedFrames)
                             .arg(result.maximumQueuedBytes / (1024.0 * 1024.0), 0, 'f', 1)
                             .arg(result.temporaryOverlayBytes / (1024.0 * 1024.0), 0, 'f', 1);
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
    QElapsedTimer elapsed;
    elapsed.start();
    FlappedEar::ExportStageTimer stageTimer;
    stageTimer.start(0, QStringLiteral("preparing"));
    QString currentOperation = QStringLiteral("prepareTelemetryScene");
    QString currentMessage = QStringLiteral("Preparing telemetry scene");
    const auto stageDurationsJson = [&] {
        QJsonObject durations;
        const auto values = stageTimer.completedStageDurations();
        for (auto it = values.cbegin(); it != values.cend(); ++it) {
            durations.insert(it.key(), it.value());
        }
        return durations;
    };
    const auto emitEvent = [&](QJsonObject event) {
        const QString state = event.value("state").toString();
        if (!state.isEmpty() && state != stageTimer.stage()) {
            stageTimer.transition(elapsed.elapsed(), state);
        }
        if (event.contains("operation")) currentOperation = event.value("operation").toString();
        if (event.value("type").toString() != QStringLiteral("log") && event.contains("message")) {
            currentMessage = event.value("message").toString();
        }
        event.insert("state", stageTimer.stage());
        event.insert("operation", currentOperation);
        event.insert("currentOperation", currentMessage);
        event.insert("stageElapsedMilliseconds", stageTimer.stageElapsedMilliseconds(elapsed.elapsed()));
        event.insert("totalElapsedMilliseconds", stageTimer.totalElapsedMilliseconds(elapsed.elapsed()));
        event.insert("elapsedMilliseconds", stageTimer.totalElapsedMilliseconds(elapsed.elapsed()));
        event.insert("timestampMilliseconds", stageTimer.totalElapsedMilliseconds(elapsed.elapsed()));
        event.insert("stageDurations", stageDurationsJson());
        writeExportEvent(event);
    };
    const auto emitProbeEvent = [&](const FlappedEar::MediaProbeEvent &probe) {
        QJsonObject details{{"mode", probe.mode}, {"target", probe.targetPath},
                            {"probeElapsedMilliseconds", probe.elapsedMilliseconds}};
        QString message;
        if (probe.phase == FlappedEar::MediaProbeEvent::Phase::Started) {
            details.insert("executable", probe.executable);
            details.insert("arguments", QJsonArray::fromStringList(probe.arguments));
            message = QStringLiteral("ffprobe started");
        } else if (probe.phase == FlappedEar::MediaProbeEvent::Phase::Heartbeat) {
            message = QStringLiteral("ffprobe running · %1 s")
                          .arg(probe.elapsedMilliseconds / 1000.0, 0, 'f', 1);
        } else {
            details.insert("exitCode", probe.exitCode);
            message = QStringLiteral("ffprobe exited · code %1").arg(probe.exitCode);
        }
        emitEvent({{"type", "log"}, {"level", "debug"}, {"component", "ffprobe"},
                   {"message", message}, {"details", details}});
    };
    try {
        emitEvent({{"type", "log"}, {"level", "info"}, {"component", "export"},
                   {"message", "Export worker started"}});
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
        currentOperation = QStringLiteral("probeInput");
        currentMessage = QStringLiteral("Reading input metadata with ffprobe");
        emitEvent({{"type", "status"}, {"operation", currentOperation}, {"message", currentMessage}});
        const FlappedEar::MediaInfo input = FlappedEar::MediaProbe::probe(
            config.value("inputPath").toString(), {}, false, -1, emitProbeEvent);
        emitEvent({{"type", "log"}, {"level", "info"}, {"component", "ffprobe"},
                   {"message", "Input probed"},
                   {"details", QJsonObject{{"codec", input.videoCodec},
                                            {"width", input.videoSize.width()},
                                            {"height", input.videoSize.height()},
                                            {"duration", input.duration}}}});
        currentOperation = QStringLiteral("initializeRenderer");
        currentMessage = QStringLiteral("Preparing telemetry scene");
        emitEvent({{"type", "status"}, {"operation", currentOperation}, {"message", currentMessage}});
        FlappedEar::TelemetryFrameRenderer renderer;
        if (!renderer.initialize(&widgets, &session, &geometry, sync, input.videoSize)) {
            writeExportEvent({{"state", "failed"}, {"error", renderer.errorString()}});
            return EXIT_FAILURE;
        }
        emitEvent({{"type", "log"}, {"level", "info"}, {"component", "renderer"},
                   {"message", "Renderer initialized"},
                   {"details", QJsonObject{{"width", input.videoSize.width()},
                                            {"height", input.videoSize.height()}}}});
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
        const double sourceRangeStart = settings.startTime;
        const double sourceRangeEnd = settings.endTime;
        const double exportDuration = sourceRangeEnd - sourceRangeStart;
        const qsizetype expectedFrames = FlappedEar::ExportEngine::frameCount(
            settings.startTime, settings.endTime, settings.frameRate);
        FlappedEar::ExportProgressEstimator rendererProgress;
        qint64 lastUpdate = -125;
        emitEvent({{"type", "status"}, {"state", "preparing"},
                          {"operation", "prepareTelemetryScene"},
                          {"message", "Preparing telemetry scene"},
                          {"sourceRangeStart", sourceRangeStart},
                          {"sourceRangeEnd", sourceRangeEnd}, {"exportDuration", exportDuration},
                          {"sourceVideoTime", sourceRangeStart},
                          {"exportRelativeTime", 0.0},
                          {"expectedFrames", static_cast<qint64>(expectedFrames)},
                          {"width", input.videoSize.width()}, {"height", input.videoSize.height()},
                          {"frameRate", settings.frameRate.value()}, {"audioEnabled", settings.audioEnabled}});
        settings.progressCallback = [&](const FlappedEar::ExportPipelineProgress &pipeline) {
            const qint64 elapsedMilliseconds = elapsed.elapsed();
            if (pipeline.submittedFrames != pipeline.expectedFrames
                && elapsedMilliseconds - lastUpdate < 125) return;
            lastUpdate = elapsedMilliseconds;
            const auto rendererSnapshot = rendererProgress.update(
                pipeline.submittedFrames, pipeline.expectedFrames, elapsedMilliseconds, settings.frameRate);
            const double encodedPercent = FlappedEar::FfmpegProgressParser::overallPercent(
                pipeline.encodedSeconds, pipeline.outputDurationSeconds);
            const double overlayPercent = pipeline.expectedFrames > 0
                ? 60.0 * double(pipeline.submittedFrames) / double(pipeline.expectedFrames) : 0.0;
            const double visiblePercent = pipeline.stage == QStringLiteral("renderingOverlay") ? overlayPercent
                : pipeline.stage == QStringLiteral("encodingVideo") ? 60.0 + 0.35 * encodedPercent
                : pipeline.stage == QStringLiteral("finalizing") ? 97.0 : encodedPercent;
            const QString eventStage = pipeline.stage == QStringLiteral("renderingOverlay")
                ? QStringLiteral("renderingOverlay") : QStringLiteral("encodingVideo");
            const QString operation = pipeline.stage == QStringLiteral("renderingOverlay")
                ? QStringLiteral("renderTelemetryOverlay")
                : pipeline.stage == QStringLiteral("finalizing")
                    ? QStringLiteral("flushOutputContainer") : QStringLiteral("encodeFinalVideo");
            const QString operationMessage = pipeline.stage == QStringLiteral("renderingOverlay")
                ? QStringLiteral("Rendering telemetry overlay")
                : pipeline.stage == QStringLiteral("finalizing")
                    ? QStringLiteral("Flushing MP4 container")
                    : QStringLiteral("Encoding final HEVC video");
            QJsonObject event{{"type", "progress"}, {"state", eventStage},
                              {"operation", operation}, {"message", operationMessage},
                              {"renderedFrames", static_cast<qint64>(pipeline.submittedFrames)},
                              {"generatedFrames", static_cast<qint64>(pipeline.generatedFrames)},
                              {"expectedFrames", static_cast<qint64>(pipeline.expectedFrames)},
                              {"sourceRangeStart", pipeline.sourceRangeStart},
                              {"sourceRangeEnd", pipeline.sourceRangeEnd},
                              {"exportDuration", pipeline.exportDuration},
                              {"exportRelativeTime", pipeline.exportRelativeTime},
                              {"sourceVideoTime", pipeline.sourceVideoTime},
                              {"telemetryTime", FlappedEar::videoToTelemetryTime(pipeline.sourceVideoTime, sync)},
                              {"encodedFrames", static_cast<qint64>(pipeline.encodedFrames)},
                              {"encodedSeconds", pipeline.encodedSeconds},
                              {"encodedProgress", encodedPercent},
                              {"encoderFps", pipeline.encoderFps},
                              {"encoderRealtimeFactor", pipeline.encoderRealtimeFactor},
                              {"queuedBytes", pipeline.queuedBytes},
                              {"maximumQueuedBytes", pipeline.maximumQueuedBytes},
                              {"temporaryOverlayBytes", pipeline.temporaryOverlayBytes},
                              {"visibleProgress", visiblePercent},
                              {"outputBytes", QFileInfo(settings.outputPath).size()}};
            if (rendererSnapshot.etaAvailable) {
                event.insert("rendererFps", rendererSnapshot.throughputFps);
            }
            emitEvent(event);
        };
        settings.stateCallback = [](const QString &) {};
        settings.encoderCallback = [&](const QString &id, const QString &name) {
            emitEvent({{"type", "log"}, {"state", "preparing"}, {"level", "info"},
                       {"component", "encoder"}, {"message", "Final encoder selected"},
                       {"encoderId", id}, {"encoderName", name},
                       {"details", QJsonObject{{"id", id}, {"name", name}}}});
        };
        settings.observationCallback = [&](const FlappedEar::ExportObservation &observation) {
            QJsonObject event{{"type", observation.type}, {"state", observation.state},
                              {"operation", observation.operation},
                              {"message", observation.message},
                              {"component", observation.component}};
            if (!observation.details.isEmpty()) {
                event.insert("details", QJsonObject::fromVariantMap(observation.details));
            }
            emitEvent(event);
        };
        const FlappedEar::ExportResult result = FlappedEar::ExportEngine::exportVideo(settings, renderer);
        if (result.cancelled) {
            emitEvent({{"type", "log"}, {"state", "cancelled"}, {"level", "warning"},
                       {"operation", "cancelled"}, {"message", "Export cancelled"}});
            return EXIT_SUCCESS;
        }
        if (!result.success) {
            emitEvent({{"type", "log"}, {"state", "failed"}, {"level", "error"},
                       {"operation", "failed"}, {"message", "Export failed"},
                       {"error", result.error}, {"diagnostics", result.diagnostics}});
            return EXIT_FAILURE;
        }
        const QString completionState = result.validationWarning.isEmpty()
            ? QStringLiteral("complete") : QStringLiteral("validationWarning");
        emitEvent({{"type", "log"}, {"state", completionState},
                   {"level", result.validationWarning.isEmpty() ? "info" : "warning"},
                   {"operation", "complete"},
                   {"component", "export"},
                   {"message", result.validationWarning.isEmpty()
                       ? QStringLiteral("Export complete")
                       : QStringLiteral("Export completed with validation warning")}});
        emitEvent({{"type", "status"}, {"state", completionState},
                          {"operation", "complete"}, {"message", "Export complete"},
                          {"warning", result.validationWarning},
                          {"diagnostics", result.diagnostics},
                          {"renderedFrames", static_cast<qint64>(result.renderedFrames)},
                          {"generatedFrames", static_cast<qint64>(result.generatedFrames)},
                          {"expectedFrames", static_cast<qint64>(result.renderedFrames)},
                          {"elapsedMilliseconds", result.elapsedMilliseconds},
                          {"renderMilliseconds", result.renderMilliseconds},
                          {"renderNanoseconds", result.renderNanoseconds},
                          {"polishNanoseconds", result.polishNanoseconds},
                          {"syncRenderNanoseconds", result.syncRenderNanoseconds},
                          {"readbackNanoseconds", result.readbackNanoseconds},
                          {"cpuCopyNanoseconds", result.cpuCopyNanoseconds},
                          {"ffmpegWriteNanoseconds", result.ffmpegWriteNanoseconds},
                          {"maximumQueuedBytes", result.maximumQueuedBytes},
                          {"temporaryOverlayBytes", result.temporaryOverlayBytes},
                          {"outputBytes", result.outputBytes},
                          {"encodedFrames", static_cast<qint64>(result.encodedFrames)},
                          {"encodedSeconds", result.encodedSeconds},
                          {"outputVideoCodec", result.mediaInfo.videoCodec},
                          {"outputWidth", result.mediaInfo.videoSize.width()},
                          {"outputHeight", result.mediaInfo.videoSize.height()},
                          {"outputDuration", result.mediaInfo.duration},
                          {"outputAudioCodecs", result.mediaInfo.audioCodecs.join(", ")}});
        return EXIT_SUCCESS;
    } catch (const std::exception &error) {
        emitEvent({{"type", "log"}, {"state", "failed"}, {"level", "error"},
                   {"operation", "failed"}, {"message", "Export failed"},
                   {"error", QString::fromUtf8(error.what())}});
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
