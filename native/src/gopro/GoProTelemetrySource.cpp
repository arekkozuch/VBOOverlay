#include "gopro/GoProTelemetrySource.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QStandardPaths>
#include <QtEndian>
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace FlappedEar {
namespace {

struct Record {
    QByteArray key;
    char type = 0;
    int size = 0;
    int repeat = 0;
    QByteArray data;
};

struct GpsSample {
    double time = 0.0;
    float latitude = 0.0F;
    float longitude = 0.0F;
    float speedKmh = 0.0F;
};

struct ProbePacket {
    qint64 position = 0;
    qint64 size = 0;
    double pts = 0.0;
    double duration = 0.0;
};

[[noreturn]] void fail(const QString &message)
{
    throw std::runtime_error(message.toStdString());
}

QString ffprobePath()
{
    const QString found = QStandardPaths::findExecutable(QStringLiteral("ffprobe"));
    if (!found.isEmpty()) {
        return found;
    }
    for (const QString &candidate : {
             QStringLiteral("/opt/homebrew/bin/ffprobe"),
             QStringLiteral("/usr/local/bin/ffprobe")}) {
        if (QFileInfo::exists(candidate)) {
            return candidate;
        }
    }
    fail("ffprobe was not found. Install FFmpeg to read GoPro telemetry.");
}

QJsonObject runProbe(const QStringList &arguments)
{
    QProcess process;
    process.start(ffprobePath(), arguments);
    if (!process.waitForStarted(5000)) {
        fail(QStringLiteral("Could not start ffprobe: %1").arg(process.errorString()));
    }
    if (!process.waitForFinished(120000)) {
        process.kill();
        process.waitForFinished();
        fail("ffprobe timed out while indexing the recording.");
    }
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        fail(QStringLiteral("ffprobe failed: %1")
                 .arg(QString::fromUtf8(process.readAllStandardError()).trimmed()));
    }
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(process.readAllStandardOutput(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        fail(QStringLiteral("Invalid ffprobe response: %1").arg(error.errorString()));
    }
    return document.object();
}

QVector<Record> records(const QByteArray &bytes)
{
    QVector<Record> result;
    qsizetype offset = 0;
    while (offset + 8 <= bytes.size()) {
        const char *header = bytes.constData() + offset;
        const int size = static_cast<unsigned char>(header[5]);
        const int repeat = qFromBigEndian<quint16>(
            reinterpret_cast<const uchar *>(header + 6));
        const qint64 dataSize = static_cast<qint64>(size) * repeat;
        if (size <= 0 || dataSize < 0 || offset + 8 + dataSize > bytes.size()) {
            break;
        }
        result.append({
            QByteArray(header, 4),
            header[4],
            size,
            repeat,
            bytes.mid(offset + 8, dataSize),
        });
        offset += 8 + ((dataSize + 3) & ~qint64(3));
    }
    return result;
}

qint32 signed32(const char *data)
{
    return qFromBigEndian<qint32>(reinterpret_cast<const uchar *>(data));
}

quint16 unsigned16(const char *data)
{
    return qFromBigEndian<quint16>(reinterpret_cast<const uchar *>(data));
}

quint64 unsigned64(const char *data)
{
    return qFromBigEndian<quint64>(reinterpret_cast<const uchar *>(data));
}

QVector<double> scalers(const Record &record)
{
    QVector<double> result;
    if (record.size != 4) {
        return result;
    }
    result.reserve(record.repeat);
    for (int index = 0; index < record.repeat; ++index) {
        const qint32 value = signed32(record.data.constData() + index * 4);
        result.append(value == 0 ? 1.0 : static_cast<double>(value));
    }
    return result;
}

void appendGpsStream(
    const QByteArray &streamData,
    const double packetPts,
    const double packetDuration,
    QVector<GpsSample> &gps5,
    QVector<GpsSample> &gps9)
{
    const QVector<Record> streamRecords = records(streamData);
    double streamPts = packetPts;
    const auto timestampIt =
        std::find_if(streamRecords.cbegin(), streamRecords.cend(), [](const Record &item) {
            return item.key == "STMP";
        });
    if (timestampIt != streamRecords.cend() && timestampIt->data.size() >= 8) {
        const double microsecondTime = unsigned64(timestampIt->data.constData()) / 1'000'000.0;
        if (std::isfinite(microsecondTime) && std::abs(microsecondTime - packetPts) <= 2.0) {
            streamPts = microsecondTime;
        }
    }
    const auto scaleIt = std::find_if(streamRecords.cbegin(), streamRecords.cend(), [](const Record &item) {
        return item.key == "SCAL";
    });
    const QVector<double> scale = scaleIt == streamRecords.cend() ? QVector<double>() : scalers(*scaleIt);
    const auto gpsIt = std::find_if(streamRecords.cbegin(), streamRecords.cend(), [](const Record &item) {
        return item.key == "GPS9" || item.key == "GPS5";
    });
    if (gpsIt == streamRecords.cend()) {
        return;
    }
    const bool isGps9 = gpsIt->key == "GPS9";
    const int expectedSize = isGps9 ? 32 : 20;
    const int dimensions = isGps9 ? 9 : 5;
    if (gpsIt->size != expectedSize || gpsIt->repeat <= 0 || scale.size() < dimensions) {
        return;
    }
    int gps5Fix = 3;
    if (!isGps9) {
        const auto fixIt = std::find_if(streamRecords.cbegin(), streamRecords.cend(), [](const Record &item) {
            return item.key == "GPSF";
        });
        if (fixIt != streamRecords.cend() && fixIt->data.size() >= 4) {
            gps5Fix = signed32(fixIt->data.constData());
        }
    }
    QVector<GpsSample> &destination = isGps9 ? gps9 : gps5;
    for (int sampleIndex = 0; sampleIndex < gpsIt->repeat; ++sampleIndex) {
        const char *sample = gpsIt->data.constData() + sampleIndex * expectedSize;
        const double latitude = signed32(sample) / scale[0];
        const double longitude = signed32(sample + 4) / scale[1];
        const double speed = signed32(sample + 12) / scale[3];
        const int fix = isGps9 ? unsigned16(sample + 30) : gps5Fix;
        if (!std::isfinite(latitude) || !std::isfinite(longitude) || !std::isfinite(speed)
            || std::abs(latitude) > 90.0 || std::abs(longitude) > 180.0
            || (latitude == 0.0 && longitude == 0.0) || fix < 2) {
            continue;
        }
        destination.append({
            streamPts + packetDuration * sampleIndex / gpsIt->repeat,
            static_cast<float>(latitude),
            static_cast<float>(longitude),
            static_cast<float>(speed * 3.6),
        });
    }
}

void visitContainers(
    const QByteArray &bytes,
    const double pts,
    const double duration,
    QVector<GpsSample> &gps5,
    QVector<GpsSample> &gps9)
{
    for (const Record &record : records(bytes)) {
        if (record.key == "STRM") {
            appendGpsStream(record.data, pts, duration, gps5, gps9);
        } else if (record.type == 0) {
            visitContainers(record.data, pts, duration, gps5, gps9);
        }
    }
}

TelemetryChannel channel(
    const QString &name,
    const QString &unit,
    const QVector<GpsSample> &samples,
    const auto value)
{
    TelemetryChannel result;
    result.name = name;
    result.unit = unit;
    result.timestamps.reserve(samples.size());
    result.values.reserve(samples.size());
    for (const GpsSample &sample : samples) {
        result.timestamps.append(sample.time);
        result.values.append(value(sample));
    }
    return result;
}

} // namespace

GoProTelemetryResult GoProTelemetrySource::load(const QString &videoPath)
{
    const QFileInfo info(videoPath);
    if (!info.isFile()) {
        fail("The selected video does not exist.");
    }
    const QJsonObject probe = runProbe({
        QStringLiteral("-v"), QStringLiteral("error"), QStringLiteral("-show_format"),
        QStringLiteral("-show_streams"), QStringLiteral("-of"), QStringLiteral("json"),
        info.absoluteFilePath(),
    });
    int streamIndex = -1;
    for (const QJsonValue &value : probe.value("streams").toArray()) {
        const QJsonObject stream = value.toObject();
        if (stream.value("codec_type").toString() == "data"
            && stream.value("codec_tag_string").toString() == "gpmd") {
            streamIndex = stream.value("index").toInt(-1);
            break;
        }
    }
    if (streamIndex < 0) {
        fail("No GoPro GPMF telemetry track was found.");
    }
    const double videoDuration = probe.value("format").toObject().value("duration").toString().toDouble();
    const QJsonObject packetProbe = runProbe({
        QStringLiteral("-v"), QStringLiteral("error"), QStringLiteral("-select_streams"),
        QString::number(streamIndex), QStringLiteral("-show_packets"), QStringLiteral("-show_entries"),
        QStringLiteral("packet=pts_time,duration_time,size,pos"), QStringLiteral("-of"),
        QStringLiteral("json"), info.absoluteFilePath(),
    });
    QVector<ProbePacket> index;
    qint64 totalSize = 0;
    for (const QJsonValue &value : packetProbe.value("packets").toArray()) {
        const QJsonObject item = value.toObject();
        ProbePacket packet{
            item.value("pos").toString().toLongLong(),
            item.value("size").toString().toLongLong(),
            item.value("pts_time").toString().toDouble(),
            item.value("duration_time").toString().toDouble(),
        };
        if (packet.position < 0 || packet.size <= 0 || !std::isfinite(packet.pts)
            || !std::isfinite(packet.duration)) {
            fail("The GPMF packet index is missing or invalid.");
        }
        totalSize += packet.size;
        index.append(packet);
    }
    if (index.isEmpty()) {
        fail("The GoPro telemetry track is empty.");
    }
    if (totalSize > 512LL * 1024 * 1024) {
        fail("The GPMF metadata track is unexpectedly large.");
    }
    QFile file(info.absoluteFilePath());
    if (!file.open(QIODevice::ReadOnly)) {
        fail(QStringLiteral("Could not read video: %1").arg(file.errorString()));
    }
    QVector<GpmfPacket> packets;
    packets.reserve(index.size());
    for (const ProbePacket &entry : index) {
        if (!file.seek(entry.position)) {
            fail("Could not seek to a GPMF packet.");
        }
        QByteArray data = file.read(entry.size);
        if (data.size() != entry.size) {
            fail("Unexpected end of file in a GPMF packet.");
        }
        packets.append({std::move(data), entry.pts, entry.duration});
    }
    return decodeGpsPackets(packets, videoDuration);
}

GoProTelemetryResult GoProTelemetrySource::decodeGpsPackets(
    const QVector<GpmfPacket> &packets,
    const double videoDuration)
{
    QVector<GpsSample> gps5;
    QVector<GpsSample> gps9;
    for (const GpmfPacket &packet : packets) {
        visitContainers(packet.data, packet.pts, packet.duration, gps5, gps9);
    }
    const QVector<GpsSample> &samples = gps9.isEmpty() ? gps5 : gps9;
    const QString streamName = gps9.isEmpty() ? QStringLiteral("GPS5") : QStringLiteral("GPS9");
    if (samples.isEmpty()) {
        fail("The GPMF track contains no usable GPS speed samples.");
    }
    TelemetrySession session;
    session.duration = videoDuration > 0.0 ? videoDuration : samples.constLast().time;
    session.startTime = 0.0;
    session.metadata.insert("source", "GoPro GPMF");
    session.metadata.insert("gpsStream", streamName);
    session.sampleCount = samples.size();
    session.channels.insert(
        "GoPro latitude", channel("GoPro latitude", "deg", samples, [](const GpsSample &item) {
            return item.latitude;
        }));
    session.channels.insert(
        "GoPro longitude", channel("GoPro longitude", "deg", samples, [](const GpsSample &item) {
            return item.longitude;
        }));
    session.channels.insert(
        "GoPro GPS speed", channel("GoPro GPS speed", "km/h", samples, [](const GpsSample &item) {
            return item.speedKmh;
        }));
    session.aliases.insert("latitude", "GoPro latitude");
    session.aliases.insert("longitude", "GoPro longitude");
    session.aliases.insert("speed", "GoPro GPS speed");
    return {std::move(session), packets.size(), streamName};
}

} // namespace FlappedEar
