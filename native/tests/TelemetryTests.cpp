#include "gopro/GoProTelemetrySource.h"
#include "export/EncoderDetector.h"
#include "export/ExportEngine.h"
#include "export/ExportProgress.h"
#include "export/FfmpegTools.h"
#include "export/MediaProbe.h"
#include "sync/TelemetrySyncEngine.h"
#include "telemetry/TelemetrySession.h"
#include "telemetry/TelemetryRenderContext.h"
#include "telemetry/TrackGeometry.h"
#include "telemetry/VboParser.h"
#include "widgets/WidgetModel.h"

#include <QFile>
#include <QProcess>
#include <QScopeGuard>
#include <QTemporaryDir>
#include <QtEndian>
#include <QtTest>
#include <cmath>

using namespace FlappedEar;

class TelemetryTests final : public QObject {
    Q_OBJECT

private slots:
    void parsesRealisticFixture();
    void toleratesMalformedRows();
    void preservesRepeatedDataSections();
    void rejectsMissingSections();
    void interpolatesByTime();
    void samplesTelemetryRanges();
    void convertsArcMinuteCoordinates();
    void parsesOptionalRealVbo();
    void persistsWidgetScenes();
    void loadsVisualTemplates();
    void providesCustomizableArchetypes();
    void persistsAndSharesCustomTemplates();
    void persistsWidgetAnimationCues();
    void groupsAndMovesWidgets();
    void constrainsWidgetGeometry();
    void buildsTrackGeometry();
    void decodesGps9Gpmf();
    void rejectsMalformedGpmf();
    void synchronizesGpsSpeed();
    void reportsAmbiguousGpsSpeed();
    void gatesWeakSyncCandidates();
    void rendersTelemetryAtExplicitTime();
    void probesMediaInfoJson();
    void parsesMediaSummaryJson();
    void rejectsInvalidMediaProbeJson();
    void classifiesMediaProbeProcessFailures();
    void detectsHevcEncoders();
    void calculatesTimestampDrivenExportFrames();
    void preservesAbsoluteExportTimestamps();
    void composesNonZeroExportRangeWithZeroBasedOutput();
    void estimatesExportProgress();
    void parsesStructuredFfmpegProgress();
    void calculatesEncodedOutputProgress();
    void preservesFrameIdentityThroughCompletedOverlayComposition();
    void syncsOptionalRealRecording();
};

namespace {

QByteArray klvRecord(
    const QByteArray &key,
    const char type,
    const quint8 size,
    const quint16 repeat,
    QByteArray data)
{
    QByteArray result = key.leftJustified(4, ' ').left(4);
    result.append(type);
    result.append(static_cast<char>(size));
    const quint16 bigRepeat = qToBigEndian(repeat);
    result.append(reinterpret_cast<const char *>(&bigRepeat), sizeof(bigRepeat));
    result.append(data);
    while (result.size() % 4 != 0) {
        result.append('\0');
    }
    return result;
}

void append32(QByteArray &data, const qint32 value)
{
    const qint32 big = qToBigEndian(value);
    data.append(reinterpret_cast<const char *>(&big), sizeof(big));
}

void append16(QByteArray &data, const quint16 value)
{
    const quint16 big = qToBigEndian(value);
    data.append(reinterpret_cast<const char *>(&big), sizeof(big));
}

TelemetrySession speedSession(const double start, const double end, const double valueOffset)
{
    TelemetrySession session;
    TelemetryChannel speed;
    speed.name = "speed";
    speed.unit = "km/h";
    for (double time = start; time <= end; time += 0.2) {
        speed.timestamps.append(time);
        const double sourceTime = time - valueOffset;
        speed.values.append(static_cast<float>(
            50.0 + 18.0 * std::sin(sourceTime * 0.21)
            + 7.0 * std::sin(sourceTime * 0.73) + sourceTime * 0.08));
    }
    session.channels.insert("speed", speed);
    session.aliases.insert("speed", "speed");
    session.duration = end - start;
    session.sampleCount = speed.values.size();
    return session;
}

} // namespace

void TelemetryTests::parsesRealisticFixture()
{
    const TelemetrySession session = VboParser::parseFile(TEST_FIXTURE_PATH);
    QCOMPARE(session.sampleCount, 3);
    QCOMPARE(session.duration, 1.0);
    QCOMPARE(session.metadata.value("vehicle"), QString("Test Car"));
    QVERIFY(session.channels.contains("mystery"));
    QCOMPARE(session.aliases.value("speed"), QString("velocity"));
    QCOMPARE(session.aliases.value("rpm"), QString("rpm"));
    QCOMPARE(session.aliases.value("throttle"), QString("throttle"));
    QCOMPARE(session.aliases.value("brake"), QString("brake"));
    QCOMPARE(session.aliases.value("heartRate"), QString("heart_rate"));
    QCOMPARE(session.channels.value("velocity").values, QVector<float>({0.0F, 50.0F, 100.0F}));
}

void TelemetryTests::toleratesMalformedRows()
{
    const auto session = VboParser::parse(
        u"[column names]\ntime speed unknown\n[data]\n0   10  1\n1 bad\n2 30 3 extra");
    QCOMPARE(session.sampleCount, 3);
    QVERIFY(std::isnan(session.channels.value("speed").values[1]));
    QCOMPARE(session.warnings.size(), 2);
}

void TelemetryTests::preservesRepeatedDataSections()
{
    const TelemetrySession session = VboParser::parse(
        u"[column names]\ntime speed\n[data]\n0 10\n[data]\n1 20");
    QCOMPARE(session.sampleCount, 2);
    QCOMPARE(session.channels.value("speed").values, QVector<float>({10.0F, 20.0F}));
}

void TelemetryTests::rejectsMissingSections()
{
    QVERIFY_THROWS_EXCEPTION(VboParseError, (void) VboParser::parse(u"[header]\nfoo=bar"));
}

void TelemetryTests::interpolatesByTime()
{
    const auto session = VboParser::parse(u"[column names]\ntime speed\n[data]\n0 0\n1 10\n2 30");
    QCOMPARE(session.valueAt("speed", 1).value(), 10.0);
    QCOMPARE(session.valueAt("speed", 1.5).value(), 20.0);
    QCOMPARE(session.valueAt("speed", 1.6, InterpolationMode::Previous).value(), 10.0);
    QCOMPARE(session.valueAt("speed", 1.6, InterpolationMode::Nearest).value(), 30.0);
    QCOMPARE(session.valueAt("speed", -1).value(), 0.0);
    QCOMPARE(session.valueAt("speed", 9).value(), 30.0);
    QVERIFY(!session.valueAt("rpm", 1));
    QCOMPARE(videoToTelemetryTime(10, {2.5, 1.01}), 12.6);
}

void TelemetryTests::samplesTelemetryRanges()
{
    const auto session = VboParser::parse(
        u"[column names]\ntime speed\n[data]\n0 0\n1 10\n2 20\n3 30\n4 40");
    const QVector<QPointF> points = session.sampledRange("speed", 1.0, 3.0, 5);
    QCOMPARE(points.size(), 5);
    QCOMPARE(points.front(), QPointF(1.0, 10.0));
    QCOMPARE(points[2], QPointF(2.0, 20.0));
    QCOMPARE(points.back(), QPointF(3.0, 30.0));
    QVERIFY(session.sampledRange("missing", 0.0, 1.0, 10).isEmpty());
    QVERIFY(session.sampledRange("speed", 0.0, 1.0, 1).isEmpty());
}

void TelemetryTests::convertsArcMinuteCoordinates()
{
    const auto session = VboParser::parse(
        u"[column names]\ntime latitude longitude\n[data]\n0 3120 -1260\n1 3126 -1266");
    QCOMPARE(session.channels.value("latitude").values[0], 52.0F);
    QCOMPARE(session.channels.value("longitude").values[0], -21.0F);
}

void TelemetryTests::parsesOptionalRealVbo()
{
    const QString path = qEnvironmentVariable("FLAPPEDEAR_REAL_VBO");
    if (path.isEmpty()) {
        QSKIP("FLAPPEDEAR_REAL_VBO is not set");
    }
    const auto session = VboParser::parseFile(path);
    QVERIFY(session.sampleCount > 0);
    QVERIFY(session.duration > 0.0);
    QVERIFY(!session.channels.isEmpty());
    QVERIFY(session.aliases.contains("speed"));
    QVERIFY(session.aliases.contains("rpm"));
}

void TelemetryTests::persistsWidgetScenes()
{
    WidgetModel source;
    source.resetDefaults();
    const int custom = source.addWidget("customValue");
    source.setSetting(custom, "source", "oiltemp");
    source.setSetting(custom, "label", "Oil temperature");
    source.setWidgetProperty(custom, "rotation", 12.0);

    WidgetModel restored;
    QVERIFY(restored.fromJson(source.toJson()));
    QCOMPARE(restored.count(), source.count());
    const QVariantMap widget = restored.widget(custom);
    QCOMPARE(widget.value("type").toString(), QString("customValue"));
    QCOMPARE(widget.value("rotation").toDouble(), 12.0);
    QCOMPARE(widget.value("settings").toMap().value("source").toString(), QString("oiltemp"));
}

void TelemetryTests::loadsVisualTemplates()
{
    WidgetModel model;
    QVERIFY(model.templates().size() >= 3);
    QVERIFY(model.applyTemplate("minimal"));
    QCOMPARE(model.count(), 2);
    QCOMPARE(model.widget(0).value("type").toString(), QString("speed"));
    QCOMPARE(
        model.widget(0).value("settings").toMap().value("showBackground").toBool(),
        false);
    QVERIFY(model.applyTemplate("performance"));
    QCOMPARE(model.count(), 3);
    QCOMPARE(model.widget(0).value("type").toString(), QString("arcGauge"));
    QCOMPARE(model.widget(1).value("type").toString(), QString("dialGauge"));
    QCOMPARE(model.widget(2).value("type").toString(), QString("telemetryOverlay"));
    QVERIFY(model.applyTemplate("2000s-grand-prix"));
    QCOMPARE(model.count(), 6);
    QCOMPARE(model.widget(0).value("type").toString(), QString("retroTachometer"));
    QCOMPARE(model.widget(1).value("type").toString(), QString("retroGear"));
    QCOMPARE(model.widget(2).value("type").toString(), QString("retroPedal"));
    QCOMPARE(model.widget(3).value("settings").toMap().value("source").toString(), QString("brake_pos-obd"));
    QVERIFY(!model.applyTemplate("missing-template"));
}

void TelemetryTests::providesCustomizableArchetypes()
{
    WidgetModel model;
    const QHash<QString, QStringList> specialized = {
        {"speed", {"showGauge", "unit", "maxValue"}},
        {"rpm", {"showBar", "warningValue", "maxValue"}},
        {"heartRate", {"showIcon", "unit", "accentColor"}},
        {"pedals", {"acceleratorSource", "brakeSource", "acceleratorColor", "brakeColor"}},
        {"gForce", {"lateralSource", "longitudinalSource", "gRange", "gridColor"}},
        {"track", {"lineColor", "lineWidth", "markerColor", "mirrorX", "mirrorY"}},
        {"customValue", {"label", "decimals", "multiplier"}},
        {"arcGauge", {"source", "startAngle", "endAngle", "arcWidth", "trackColor"}},
        {"dialGauge", {"source", "startAngle", "endAngle", "majorTicks", "needleColor"}},
        {"telemetryOverlay", {"source1", "source2", "source3", "source4", "columns"}},
        {"retroGrandPrix", {"rpmSource", "speedSource", "gearSource", "throttleSource", "brakeSource", "driverName"}},
        {"retroTachometer", {"source", "minValue", "maxValue", "needleColor"}},
        {"retroGear", {"source", "label", "fallbackText", "panelColor"}},
        {"retroPedal", {"source", "minValue", "maxValue", "fillColor", "emptyColor"}},
        {"retroSpeedArc", {"source", "minValue", "maxValue", "segments", "lowColor"}},
        {"retroNameplate", {"topSource", "bottomSource", "topText", "bottomText"}},
        {"brandLogo", {"logoOpacity", "logoScale"}},
    };
    for (auto iterator = specialized.cbegin(); iterator != specialized.cend(); ++iterator) {
        const int index = model.addWidget(iterator.key());
        QVERIFY(index >= 0);
        const QVariantMap settings = model.widget(index).value("settings").toMap();
        for (const QString &common : {
                 "backgroundColor", "backgroundOpacity", "borderColor", "cornerRadius",
                 "textColor", "secondaryTextColor", "accentColor", "fontFamily",
                 "fontWeight", "valueFontScale", "labelFontScale", "padding"}) {
            QVERIFY2(settings.contains(common), qPrintable(iterator.key() + ": " + common));
        }
        for (const QString &key : iterator.value()) {
            QVERIFY2(settings.contains(key), qPrintable(iterator.key() + ": " + key));
        }
    }
}

void TelemetryTests::persistsAndSharesCustomTemplates()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const bool hadOverride = qEnvironmentVariableIsSet("FLAPPEDEAR_TEMPLATE_STORE");
    const QByteArray previousOverride = qgetenv("FLAPPEDEAR_TEMPLATE_STORE");
    qputenv(
        "FLAPPEDEAR_TEMPLATE_STORE",
        directory.filePath("layout-templates.json").toUtf8());
    const auto restoreEnvironment = qScopeGuard([hadOverride, previousOverride] {
        if (hadOverride) {
            qputenv("FLAPPEDEAR_TEMPLATE_STORE", previousOverride);
        } else {
            qunsetenv("FLAPPEDEAR_TEMPLATE_STORE");
        }
    });

    WidgetModel source;
    QVERIFY(source.applyTemplate("2000s-grand-prix"));
    const QString templateId =
        source.saveCurrentAsTemplate("My broadcast", "Reusable race insert");
    QVERIFY(!templateId.isEmpty());

    WidgetModel restored;
    QVERIFY(restored.applyTemplate(templateId));
    QCOMPARE(restored.count(), source.count());
    const QUrl exported = QUrl::fromLocalFile(directory.filePath("shared.fettemplate"));
    QVERIFY(restored.exportTemplate(templateId, exported));
    QVERIFY(QFileInfo::exists(exported.toLocalFile()));
    QVERIFY(restored.deleteTemplate(templateId));
    QVERIFY(!restored.applyTemplate(templateId));

    const QString importedId = restored.importTemplate(exported);
    QVERIFY(!importedId.isEmpty());
    QVERIFY(restored.applyTemplate(importedId));
    QCOMPARE(restored.widget(0).value("type").toString(), QString("retroTachometer"));
}

void TelemetryTests::persistsWidgetAnimationCues()
{
    WidgetModel source;
    const int widget = source.addWidget("telemetryOverlay");
    QCOMPARE(source.addCue(widget, 12.5, 4.0, "slideUp"), 0);
    source.setCueProperty(widget, 0, "fadeIn", 0.6);
    source.setCueProperty(widget, 0, "fadeOut", 0.8);

    WidgetModel restored;
    QVERIFY(restored.fromJson(source.toJson()));
    const QVariantList cues = restored.widget(0).value("cues").toList();
    QCOMPARE(cues.size(), 1);
    const QVariantMap cue = cues.front().toMap();
    QCOMPARE(cue.value("start").toDouble(), 12.5);
    QCOMPARE(cue.value("duration").toDouble(), 4.0);
    QCOMPARE(cue.value("fadeIn").toDouble(), 0.6);
    QCOMPARE(cue.value("fadeOut").toDouble(), 0.8);
    QCOMPARE(cue.value("effect").toString(), QString("slideUp"));
    restored.removeCue(0, 0);
    QVERIFY(restored.widget(0).value("cues").toList().isEmpty());
}

void TelemetryTests::groupsAndMovesWidgets()
{
    WidgetModel model;
    const int first = model.addWidget("retroGear");
    const int second = model.addWidget("retroPedal");
    const double firstX = model.widget(first).value("x").toDouble();
    const double secondX = model.widget(second).value("x").toDouble();
    const QString groupId = model.groupWidgets({first, second});
    QVERIFY(!groupId.isEmpty());
    QCOMPARE(model.groupMembers(first), QVariantList({first, second}));

    model.moveWidget(first, firstX + 0.1, model.widget(first).value("y").toDouble());
    QCOMPARE(model.widget(first).value("x").toDouble(), firstX + 0.1);
    QCOMPARE(model.widget(second).value("x").toDouble(), secondX + 0.1);

    WidgetModel restored;
    QVERIFY(restored.fromJson(model.toJson()));
    QCOMPARE(restored.groupMembers(second), QVariantList({first, second}));
    restored.ungroupWidget(first);
    QCOMPARE(restored.groupMembers(first), QVariantList{QVariant(first)});
    restored.removeWidgets({first, second});
    QCOMPARE(restored.count(), 0);
}

void TelemetryTests::constrainsWidgetGeometry()
{
    WidgetModel model;
    const int index = model.addWidget("speed");
    model.resizeWidget(index, 0.4, 0.3);
    model.moveWidget(index, 0.9, -1.0);
    const QVariantMap widget = model.widget(index);
    QCOMPARE(widget.value("x").toDouble(), 0.6);
    QCOMPARE(widget.value("y").toDouble(), 0.0);
    model.setWidgetProperty(index, "opacity", 4.0);
    QCOMPARE(model.widget(index).value("opacity").toDouble(), 1.0);
}

void TelemetryTests::buildsTrackGeometry()
{
    const TelemetrySession session = VboParser::parseFile(TEST_FIXTURE_PATH);
    const TrackGeometry geometry = buildTrackGeometry(session);
    QVERIFY(geometry.valid);
    QCOMPARE(geometry.points.size(), 3);
    QVERIFY(geometry.points.front().x() > 0.0);
    QCOMPARE(geometry.points.front().y(), 1.0);
    QVERIFY(geometry.points.back().x() < 1.0);
    QCOMPARE(geometry.points.back().y(), 0.0);
    const auto current = currentTrackPoint(session, 0.5, geometry);
    QVERIFY(current.has_value());
    QVERIFY2(
        qAbs(current->x() - 0.5) < 0.01,
        qPrintable(QStringLiteral("x=%1").arg(current->x(), 0, 'g', 12)));
    QVERIFY2(
        qAbs(current->y() - 0.5) < 0.01,
        qPrintable(QStringLiteral("y=%1").arg(current->y(), 0, 'g', 12)));
}

void TelemetryTests::gatesWeakSyncCandidates()
{
    SyncCandidate strong;
    strong.confidence = 0.75;
    QVERIFY(shouldAutoApplySyncCandidate(strong));
    QVERIFY(syncConfidenceLevel(strong.confidence) == SyncConfidenceLevel::High);

    SyncCandidate weak;
    weak.confidence = 0.43;
    QVERIFY(!shouldAutoApplySyncCandidate(weak));
    QVERIFY(syncConfidenceLevel(weak.confidence) == SyncConfidenceLevel::Low);

    TelemetrySession rectangle;
    TelemetryChannel latitude;
    latitude.name = "latitude";
    latitude.timestamps = {0.0, 1.0, 2.0, 3.0};
    latitude.values = {0.0F, 0.0F, 0.001F, 0.001F};
    TelemetryChannel longitude;
    longitude.name = "longitude";
    longitude.timestamps = latitude.timestamps;
    longitude.values = {0.0F, 0.004F, 0.004F, 0.0F};
    rectangle.channels.insert(latitude.name, latitude);
    rectangle.channels.insert(longitude.name, longitude);
    rectangle.aliases.insert("latitude", latitude.name);
    rectangle.aliases.insert("longitude", longitude.name);
    const TrackGeometry geometry = buildTrackGeometry(rectangle);
    QVERIFY(geometry.valid);
    const double width = geometry.points[1].x() - geometry.points[0].x();
    const double height = geometry.points[0].y() - geometry.points[2].y();
    QVERIFY2(qAbs(width / height - 4.0) < 0.05,
             qPrintable(QStringLiteral("aspect=%1").arg(width / height, 0, 'f', 3)));
    const auto current = currentTrackPoint(rectangle, 1.0, geometry);
    QVERIFY(current.has_value());
    QVERIFY(qAbs(current->x() - geometry.points[1].x()) < 0.001);
    QVERIFY(qAbs(current->y() - geometry.points[1].y()) < 0.001);
}

void TelemetryTests::rendersTelemetryAtExplicitTime()
{
    const TelemetrySession session = VboParser::parse(
        u"[column names]\ntime speed\n[data]\n0 0\n10 100");
    TelemetryRenderContext context;
    context.setSession(&session);
    context.setSyncTransform({2.0, 1.5});
    context.setTime(4.0);
    QCOMPARE(context.telemetryTime(), 8.0);
    QCOMPARE(context.telemetryValue("speed").toDouble(), 80.0);
    context.setTime(2.0);
    QCOMPARE(context.telemetryValue("speed").toDouble(), 50.0);
}

void TelemetryTests::probesMediaInfoJson()
{
    const QByteArray json = R"({"format":{"duration":"3.000000","start_time":"0.500000"},"streams":[{"codec_type":"video","codec_name":"h264","width":320,"height":180,"r_frame_rate":"30000/1001","avg_frame_rate":"30000/1001","time_base":"1/90000","pix_fmt":"yuv420p","nb_read_frames":"90"},{"codec_type":"audio","codec_name":"aac"}]})";
    const MediaInfo info = MediaProbe::parseJson(json, "/fixture.mp4");
    QCOMPARE(info.path, QString("/fixture.mp4"));
    QCOMPARE(info.videoSize, QSize(320, 180));
    QCOMPARE(info.videoCodec, QString("h264"));
    QCOMPARE(info.videoFrameCount, qsizetype(90));
    QCOMPARE(info.audioCodecs, QStringList({"aac"}));
    QVERIFY(qAbs(info.frameRate.value() - 29.97002997) < 0.00001);
    QVERIFY(!info.likelyVariableFrameRate);
}

void TelemetryTests::parsesMediaSummaryJson()
{
    const QByteArray json = R"({"format":{"duration":"120.003000"},"streams":[{"index":0,"codec_type":"video","codec_name":"hevc","width":3840,"height":2160,"r_frame_rate":"60000/1001","avg_frame_rate":"60000/1001"},{"index":1,"codec_type":"audio","codec_name":"aac"}]})";
    const MediaInfo info = MediaProbe::parseJson(json, "/summary.mp4");
    QCOMPARE(info.videoCodec, QStringLiteral("hevc"));
    QCOMPARE(info.videoSize, QSize(3840, 2160));
    QVERIFY(qAbs(info.duration - 120.003) < 0.0001);
    QVERIFY(qAbs(info.averageFrameRate.value() - 59.94005994) < 0.00001);
    QCOMPARE(info.audioCodecs, QStringList({QStringLiteral("aac")}));

    const QByteArray silentJson = R"({"format":{"duration":"12.000000"},"streams":[{"index":0,"codec_type":"video","codec_name":"hevc","width":1920,"height":1080,"r_frame_rate":"30/1","avg_frame_rate":"30/1"}]})";
    const MediaInfo silentInfo = MediaProbe::parseJson(silentJson, "/silent.mp4");
    QVERIFY(silentInfo.audioCodecs.isEmpty());
}

void TelemetryTests::rejectsInvalidMediaProbeJson()
{
    try {
        static_cast<void>(MediaProbe::parseJson("not-json", "/invalid.mp4"));
        QFAIL("Invalid ffprobe JSON should throw.");
    } catch (const std::runtime_error &error) {
        QCOMPARE(QString::fromUtf8(error.what()), QStringLiteral("ffprobe returned invalid JSON."));
    }
}

void TelemetryTests::classifiesMediaProbeProcessFailures()
{
    const auto errorFrom = [](const std::function<void()> &operation) {
        try {
            operation();
        } catch (const std::runtime_error &error) {
            return QString::fromUtf8(error.what());
        }
        return QString();
    };

    const QString startError = errorFrom([] {
        static_cast<void>(MediaProbe::probe(
            "/fixture.mp4", "/definitely/missing/flappedear-ffprobe", false, 100));
    });
    QVERIFY2(startError.startsWith("Could not start ffprobe while probing: /fixture.mp4"),
             qPrintable(startError));

#ifdef Q_OS_UNIX
    const QString exitError = errorFrom([] {
        static_cast<void>(MediaProbe::probe("/fixture.mp4", "/usr/bin/false", false, 1'000));
    });
    QVERIFY2(exitError.contains("ffprobe exited with code 1 while probing: /fixture.mp4"),
             qPrintable(exitError));
    QVERIFY2(exitError.contains("stderr: <no stderr output>"), qPrintable(exitError));

    const QString jsonError = errorFrom([] {
        static_cast<void>(MediaProbe::probe("/fixture.mp4", "/usr/bin/true", false, 1'000));
    });
    QCOMPARE(jsonError, QStringLiteral("ffprobe returned invalid JSON."));

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString slowProbePath = directory.filePath("slow-ffprobe");
    QFile slowProbe(slowProbePath);
    QVERIFY(slowProbe.open(QIODevice::WriteOnly));
    QVERIFY(slowProbe.write("#!/bin/sh\nwhile :; do :; done\n") > 0);
    slowProbe.close();
    QVERIFY(slowProbe.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner
                                     | QFileDevice::ExeOwner));
    const QString timeoutError = errorFrom([&slowProbePath] {
        static_cast<void>(MediaProbe::probe("/fixture.mp4", slowProbePath, false, 10));
    });
    QVERIFY2(timeoutError.startsWith("ffprobe timed out after 0.010 seconds while probing: /fixture.mp4"),
             qPrintable(timeoutError));
    QVERIFY2(timeoutError.contains("stderr: <no stderr output>"), qPrintable(timeoutError));

    const QString crashingProbePath = directory.filePath("crashing-ffprobe");
    QFile crashingProbe(crashingProbePath);
    QVERIFY(crashingProbe.open(QIODevice::WriteOnly));
    QVERIFY(crashingProbe.write("#!/bin/sh\nkill -SEGV $$\n") > 0);
    crashingProbe.close();
    QVERIFY(crashingProbe.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner
                                         | QFileDevice::ExeOwner));
    const QString crashError = errorFrom([&crashingProbePath] {
        static_cast<void>(MediaProbe::probe("/fixture.mp4", crashingProbePath, false, 1'000));
    });
    QVERIFY2(crashError.startsWith("ffprobe crashed while probing: /fixture.mp4"),
             qPrintable(crashError));
#endif
}

void TelemetryTests::detectsHevcEncoders()
{
    const QString output = " V....D hevc_videotoolbox Apple VideoToolbox\n V....D libx265 x265\n";
    const QList<EncoderCapability> encoders = EncoderDetector::parseEncoders(output);
    QCOMPARE(encoders.size(), 2);
    QCOMPARE(encoders[0].id, QString("hevc_videotoolbox"));
    QVERIFY(encoders[0].hardware);
    QCOMPARE(EncoderDetector::preferredHevcEncoder(encoders), QString("hevc_videotoolbox"));
}

void TelemetryTests::calculatesTimestampDrivenExportFrames()
{
    QCOMPARE(ExportEngine::frameCount(120.0, 140.0, {30'000, 1001}), qsizetype(600));
    QCOMPARE(ExportEngine::frameCount(0.0, 1.0, {60'000, 1001}), qsizetype(60));
    QCOMPARE(ExportEngine::frameCount(1.0, 1.0, {30, 1}), qsizetype(0));
    const MediaRational ntscRate{60'000, 1001};
    QCOMPARE(ExportEngine::frameCount(30.0, 150.0, ntscRate), qsizetype(7'193));
    QCOMPARE(ExportEngine::sourceVideoTime(30.0, 0, ntscRate), 30.0);
    QVERIFY(qAbs(ExportEngine::sourceVideoTime(30.0, 1, ntscRate) - (30.0 + 1001.0 / 60'000.0)) < 0.000001);
    QVERIFY(ExportEngine::sourceVideoTime(30.0, 7'192, ntscRate) < 150.0);
    QVERIFY(ExportEngine::sourceVideoTime(30.0, 7'192, ntscRate) > 149.9);
    QCOMPARE(ExportEngine::exportRelativeTime(0, ntscRate), 0.0);
}

void TelemetryTests::preservesAbsoluteExportTimestamps()
{
    const MediaRational rate{30'000, 1001};
    QCOMPARE(ExportEngine::framePresentationTime(120.0, 0, rate), 120.0);
    QVERIFY(qAbs(ExportEngine::framePresentationTime(120.0, 300, rate) - 130.01) < 0.000001);
    const SyncTransform sync{90.203, 1.0};
    TelemetrySession session;
    session.channels.insert(
        "speed", TelemetryChannel{"speed", {}, {210.203, 215.203}, {73.4F, 101.2F}});
    session.channels.insert(
        "rpm", TelemetryChannel{"rpm", {}, {210.203, 215.203}, {3842.0F, 5270.0F}});
    TelemetryRenderContext context;
    context.setSession(&session);
    context.setSyncTransform(sync);
    context.setTime(ExportEngine::framePresentationTime(120.0, 0, rate));
    QVERIFY(qAbs(context.telemetryTime() - 210.203) < 0.000001);
    QVERIFY(qAbs(context.telemetryValue("speed").toDouble() - 73.4) < 0.001);
    context.setTime(125.0);
    QVERIFY(qAbs(context.telemetryTime() - 215.203) < 0.000001);
    QVERIFY(qAbs(context.telemetryValue("rpm").toDouble() - 5270.0) < 0.001);
}

void TelemetryTests::composesNonZeroExportRangeWithZeroBasedOutput()
{
    const QString ffmpeg = FfmpegTools::ffmpegPath();
    if (ffmpeg.isEmpty()) QSKIP("FFmpeg is unavailable for the non-zero-range integration test.");
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    constexpr int width = 64;
    constexpr int height = 16;
    constexpr int sourceFrames = 300;
    constexpr int rangeStartFrame = 90;
    constexpr int exportFrames = 150;
    const QString primaryRaw = directory.filePath("source.rgba");
    const QString overlayRaw = directory.filePath("telemetry.rgba");
    const QString primary = directory.filePath("source.mkv");
    const QString overlay = directory.filePath("overlay.mkv");
    const QString composed = directory.filePath("composed.mkv");
    const QString decoded = directory.filePath("decoded.rgba");
    const auto writeIdentityFrames = [](const QString &path, const int count, const int pixelOffset) {
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly)) return false;
        for (int frame = 0; frame < count; ++frame) {
            QByteArray pixels(width * height * 4, '\0');
            for (int bit = 0; bit < 8; ++bit) {
                const int offset = (pixelOffset + bit) * 4;
                const char value = (frame & (1 << bit)) ? static_cast<char>(255) : 0;
                pixels[offset] = value;
                pixels[offset + 1] = value;
                pixels[offset + 2] = value;
                pixels[offset + 3] = static_cast<char>(255);
            }
            if (file.write(pixels) != pixels.size()) return false;
        }
        return true;
    };
    QVERIFY(writeIdentityFrames(primaryRaw, sourceFrames, 0));
    QVERIFY(writeIdentityFrames(overlayRaw, exportFrames, 8));
    const auto runFfmpeg = [&ffmpeg](const QStringList &arguments) {
        QProcess process;
        process.start(ffmpeg, arguments);
        QVERIFY2(process.waitForStarted(), qPrintable(process.errorString()));
        QVERIFY2(process.waitForFinished(30'000), qPrintable(process.errorString()));
        QCOMPARE(process.exitStatus(), QProcess::NormalExit);
        QVERIFY2(process.exitCode() == 0, process.readAllStandardError().constData());
    };
    const QString size = QStringLiteral("%1x%2").arg(width).arg(height);
    runFfmpeg({"-hide_banner", "-loglevel", "error", "-y", "-f", "rawvideo", "-pixel_format", "rgba",
               "-video_size", size, "-framerate", "30", "-i", primaryRaw, "-f", "lavfi", "-i",
               "sine=frequency=440:sample_rate=48000:duration=10", "-frames:v", QString::number(sourceFrames),
               "-c:v", "ffv1", "-pix_fmt", "bgra", "-c:a", "pcm_s16le", primary});
    runFfmpeg({"-hide_banner", "-loglevel", "error", "-y", "-f", "rawvideo", "-pixel_format", "rgba",
               "-video_size", size, "-framerate", "30", "-i", overlayRaw, "-an", "-c:v", "ffv1",
               "-pix_fmt", "bgra", overlay});
    runFfmpeg({"-hide_banner", "-loglevel", "error", "-y", "-i", primary, "-i", overlay,
               "-filter_complex", "[0:v]trim=start=3:end=8,setpts=PTS-STARTPTS[source];[1:v]setpts=PTS-STARTPTS[telemetry];[source][telemetry]overlay=0:0:shortest=1:repeatlast=0:eof_action=endall:format=rgb[video];[0:a]atrim=start=3:end=8,asetpts=PTS-STARTPTS[audio]",
               "-map", "[video]", "-map", "[audio]", "-c:v", "ffv1", "-pix_fmt", "bgra", "-c:a",
               "pcm_s16le", composed});
    const MediaInfo outputInfo = MediaProbe::probe(composed, {}, true);
    QCOMPARE(outputInfo.videoFrameCount, qsizetype(exportFrames));
    QVERIFY(qAbs(outputInfo.duration - 5.0) < 0.05);
    QVERIFY(!outputInfo.audioCodecs.isEmpty());
    const MediaInfo summaryInfo = MediaProbe::probeSummary(composed);
    QCOMPARE(summaryInfo.videoCodec, QStringLiteral("ffv1"));
    QCOMPARE(summaryInfo.videoSize, QSize(width, height));
    QVERIFY(qAbs(summaryInfo.duration - 5.0) < 0.05);
    QVERIFY(!summaryInfo.audioCodecs.isEmpty());
    runFfmpeg({"-hide_banner", "-loglevel", "error", "-y", "-i", composed, "-f", "rawvideo",
               "-pixel_format", "rgba", decoded});
    QFile decodedFile(decoded);
    QVERIFY(decodedFile.open(QIODevice::ReadOnly));
    const QByteArray output = decodedFile.readAll();
    const qsizetype bytesPerFrame = width * height * 4;
    QCOMPARE(output.size(), exportFrames * bytesPerFrame);
    const auto decodeIdentity = [](const char *pixels, const int pixelOffset) {
        int identity = 0;
        for (int bit = 0; bit < 8; ++bit) {
            if (static_cast<uchar>(pixels[(pixelOffset + bit) * 4]) > 127) identity |= 1 << bit;
        }
        return identity;
    };
    for (int frame = 0; frame < exportFrames; ++frame) {
        const char *pixels = output.constData() + frame * bytesPerFrame;
        QCOMPARE(decodeIdentity(pixels, 0), rangeStartFrame + frame);
        QCOMPARE(decodeIdentity(pixels, 8), frame);
    }
}

void TelemetryTests::estimatesExportProgress()
{
    ExportProgressEstimator estimator;
    const MediaRational rate{60, 1};
    const auto early = estimator.update(1, 600, 100, rate);
    QVERIFY(!early.etaAvailable);
    const auto steady = estimator.update(100, 600, 1'100, rate);
    QVERIFY(steady.etaAvailable);
    QVERIFY(qAbs(steady.throughputFps - 99.0) < 0.1);
    QVERIFY(qAbs(steady.realtimeFactor - 1.65) < 0.01);
    QVERIFY(steady.etaSeconds > 5.0 && steady.etaSeconds < 6.0);
    QCOMPARE(ExportProgressEstimator::stageProgress("rendering", 1.0), 95.0);
    QCOMPARE(ExportProgressEstimator::stageProgress("finalizing", 0.0), 97.0);
    QCOMPARE(ExportProgressEstimator::stageProgress("validating", 1.0), 99.0);
    QCOMPARE(ExportProgressEstimator::stageProgress("complete", 0.0), 100.0);
    QCOMPARE(ExportProgressEstimator::stageProgress("cancelled", 1.0), 0.0);
}

void TelemetryTests::parsesStructuredFfmpegProgress()
{
    FfmpegProgressParser parser;
    const QList<FfmpegProgress> first = parser.append(
        "frame=42\nfps=27.5\nout_time_us=700700\nspeed=0.46x\nprogress=continue\n");
    QCOMPARE(first.size(), 1);
    QCOMPARE(first.front().encodedFrames, qsizetype(42));
    QCOMPARE(first.front().outputMicroseconds, qint64(700700));
    QVERIFY(qAbs(first.front().encoderFps - 27.5) < 0.001);
    QVERIFY(qAbs(first.front().realtimeFactor - 0.46) < 0.001);
    QVERIFY(!first.front().complete);

    const QList<FfmpegProgress> split = parser.append("frame=60\nout_time_ms=1001000\nprogress=");
    QVERIFY(split.isEmpty());
    const QList<FfmpegProgress> last = parser.append("end\n");
    QCOMPARE(last.size(), 1);
    QCOMPARE(last.front().encodedFrames, qsizetype(60));
    QCOMPARE(last.front().outputMicroseconds, qint64(1001000));
    QVERIFY(last.front().complete);
}

void TelemetryTests::calculatesEncodedOutputProgress()
{
    QCOMPARE(FfmpegProgressParser::overallPercent(0.0, 10.0), 0.0);
    QVERIFY(FfmpegProgressParser::overallPercent(2.5, 10.0)
            < FfmpegProgressParser::overallPercent(5.0, 10.0));
    QCOMPARE(FfmpegProgressParser::overallPercent(5.0, 10.0), 47.5);
    QCOMPARE(FfmpegProgressParser::overallPercent(12.0, 10.0), 95.0);
    QCOMPARE(FfmpegProgressParser::overallPercent(-1.0, 10.0), 0.0);
}

void TelemetryTests::preservesFrameIdentityThroughCompletedOverlayComposition()
{
    const QString ffmpeg = FfmpegTools::ffmpegPath();
    if (ffmpeg.isEmpty()) QSKIP("FFmpeg is unavailable for the frame-identity integration test.");
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    constexpr int width = 64;
    constexpr int height = 16;
    constexpr int frameCount = 150;
    const QString primary = directory.filePath("primary.mkv");
    const QString rawOverlay = directory.filePath("identity.rgba");
    const QString overlay = directory.filePath("overlay.mkv");
    const QString composed = directory.filePath("composed.mkv");
    const QString decoded = directory.filePath("decoded.rgba");
    QFile rawFile(rawOverlay);
    QVERIFY(rawFile.open(QIODevice::WriteOnly));
    for (int frame = 0; frame < frameCount; ++frame) {
        QByteArray pixels(width * height * 4, '\0');
        for (int bit = 0; bit < 8; ++bit) {
            const int offset = bit * 4;
            const char value = (frame & (1 << bit)) ? static_cast<char>(255) : 0;
            pixels[offset] = value;
            pixels[offset + 1] = value;
            pixels[offset + 2] = value;
            pixels[offset + 3] = static_cast<char>(255);
        }
        QCOMPARE(rawFile.write(pixels), qint64(pixels.size()));
    }
    rawFile.close();
    const auto runFfmpeg = [&ffmpeg](const QStringList &arguments) {
        QProcess process;
        process.start(ffmpeg, arguments);
        QVERIFY2(process.waitForStarted(), qPrintable(process.errorString()));
        QVERIFY2(process.waitForFinished(30'000), qPrintable(process.errorString()));
        QCOMPARE(process.exitStatus(), QProcess::NormalExit);
        QVERIFY2(process.exitCode() == 0, process.readAllStandardError().constData());
    };
    runFfmpeg({"-hide_banner", "-loglevel", "error", "-y", "-f", "lavfi", "-i",
               QStringLiteral("color=c=black:s=%1x%2:r=30").arg(width).arg(height),
               "-frames:v", QString::number(frameCount), "-c:v", "ffv1", "-pix_fmt", "bgra", primary});
    runFfmpeg({"-hide_banner", "-loglevel", "error", "-y", "-f", "rawvideo", "-pixel_format",
               "rgba", "-video_size", QStringLiteral("%1x%2").arg(width).arg(height), "-framerate", "30",
               "-i", rawOverlay, "-frames:v", QString::number(frameCount), "-c:v", "ffv1", "-pix_fmt",
               "bgra", overlay});
    runFfmpeg({"-hide_banner", "-loglevel", "error", "-y", "-i", primary, "-i", overlay,
               "-filter_complex", "[0:v][1:v]overlay=0:0:shortest=1:repeatlast=0:eof_action=endall:format=rgb[v]",
               "-map", "[v]", "-c:v", "ffv1", "-pix_fmt", "bgra", composed});
    runFfmpeg({"-hide_banner", "-loglevel", "error", "-y", "-i", composed, "-f", "rawvideo",
               "-pixel_format", "rgba", decoded});
    QFile decodedFile(decoded);
    QVERIFY(decodedFile.open(QIODevice::ReadOnly));
    const QByteArray output = decodedFile.readAll();
    const qsizetype bytesPerFrame = width * height * 4;
    QCOMPARE(output.size(), frameCount * bytesPerFrame);
    for (int frame = 0; frame < frameCount; ++frame) {
        const char *pixels = output.constData() + frame * bytesPerFrame;
        int identity = 0;
        for (int bit = 0; bit < 8; ++bit) {
            if (static_cast<uchar>(pixels[bit * 4]) > 127) identity |= 1 << bit;
        }
        QCOMPARE(identity, frame);
    }
}

void TelemetryTests::decodesGps9Gpmf()
{
    QByteArray scale;
    for (const qint32 value : {10000000, 10000000, 1000, 1000, 100, 1, 1000, 100, 1}) {
        append32(scale, value);
    }
    QByteArray gps;
    for (int index = 0; index < 2; ++index) {
        append32(gps, 500000000 + index * 100);
        append32(gps, 190000000 + index * 100);
        append32(gps, 250000);
        append32(gps, 1250 + index * 250);
        append32(gps, 130);
        append32(gps, 10000);
        append32(gps, 200000 + index * 100);
        append16(gps, 150);
        append16(gps, 3);
    }
    QByteArray stream;
    stream += klvRecord("SCAL", 'l', 4, 9, scale);
    stream += klvRecord("GPS9", '?', 32, 2, gps);
    const QByteArray streamRecord = klvRecord("STRM", 0, 1, stream.size(), stream);
    const QByteArray packet = klvRecord("DEVC", 0, 1, streamRecord.size(), streamRecord);

    const GoProTelemetryResult result =
        GoProTelemetrySource::decodeGpsPackets({{packet, 10.0, 1.0}}, 20.0);
    QCOMPARE(result.gpsStream, QString("GPS9"));
    QCOMPARE(result.session.sampleCount, 2);
    const TelemetryChannel speed = result.session.channels.value("GoPro GPS speed");
    QCOMPARE(speed.timestamps, QVector<double>({10.0, 10.5}));
    QVERIFY(qAbs(speed.values[0] - 4.5F) < 0.001F);
    QVERIFY(qAbs(speed.values[1] - 5.4F) < 0.001F);
}

void TelemetryTests::rejectsMalformedGpmf()
{
    QVERIFY_THROWS_EXCEPTION(
        std::runtime_error,
        (void) GoProTelemetrySource::decodeGpsPackets({{{"broken"}, 0.0, 1.0}}, 1.0));
}

void TelemetryTests::synchronizesGpsSpeed()
{
    const TelemetrySession video = speedSession(0.0, 60.0, 0.0);
    const TelemetrySession telemetry = speedSession(0.0, 70.0, 3.2);
    const SyncCandidate candidate = TelemetrySyncEngine::synchronize(video, telemetry);
    QVERIFY2(qAbs(candidate.offset - 3.2) <= 0.11, qPrintable(QString::number(candidate.offset)));
    QVERIFY(candidate.diagnostics.correlation > 0.99);
    QVERIFY(candidate.confidence > 0.7);
}

void TelemetryTests::reportsAmbiguousGpsSpeed()
{
    TelemetrySession video = speedSession(0.0, 30.0, 0.0);
    TelemetrySession telemetry = speedSession(0.0, 35.0, 0.0);
    std::fill(video.channels["speed"].values.begin(), video.channels["speed"].values.end(), 42.0F);
    std::fill(
        telemetry.channels["speed"].values.begin(),
        telemetry.channels["speed"].values.end(),
        42.0F);
    const SyncCandidate candidate = TelemetrySyncEngine::synchronize(video, telemetry);
    QCOMPARE(candidate.diagnostics.correlation, -1.0);
    QCOMPARE(candidate.confidence, 0.0);
}

void TelemetryTests::syncsOptionalRealRecording()
{
    const QString videoPath = qEnvironmentVariable("FLAPPEDEAR_REAL_GOPRO");
    const QString vboPath = qEnvironmentVariable("FLAPPEDEAR_REAL_VBO");
    if (videoPath.isEmpty() || vboPath.isEmpty()) {
        QSKIP("FLAPPEDEAR_REAL_GOPRO and FLAPPEDEAR_REAL_VBO are not set");
    }
    const GoProTelemetryResult video = GoProTelemetrySource::load(videoPath);
    const TelemetrySession telemetry = VboParser::parseFile(vboPath);
    const SyncCandidate candidate = TelemetrySyncEngine::synchronize(video.session, telemetry);
    qInfo().noquote()
        << QStringLiteral("real GoPro: %1 packets, %2 %3 samples; offset=%4 correlation=%5 confidence=%6")
               .arg(video.packetCount)
               .arg(video.session.sampleCount)
               .arg(video.gpsStream)
               .arg(candidate.offset, 0, 'f', 3)
               .arg(candidate.diagnostics.correlation, 0, 'f', 6)
               .arg(candidate.confidence, 0, 'f', 3);
    QVERIFY(video.packetCount > 0);
    QVERIFY(video.session.sampleCount > 100);
    QVERIFY(candidate.diagnostics.correlation > 0.8);
    QVERIFY(candidate.confidence > 0.5);
}

QTEST_MAIN(TelemetryTests)
#include "TelemetryTests.moc"
