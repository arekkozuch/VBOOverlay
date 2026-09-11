#pragma once
// Entirely synthetic fixtures: no private recording data in the repository.
#include <QByteArray>
#include <QMap>
#include <QtEndian>
#include <zlib.h>
#include <bit>
#include <stdexcept>

namespace RczFixture {
constexpr qint64 origin = 1'780'000'000'000;
template<class T> inline void append(QByteArray &bytes, T value)
{
    const auto at = bytes.size(); bytes.resize(at + sizeof(T));
    qToLittleEndian<T>(value, bytes.data() + at);
}
inline QByteArray ticks(std::initializer_list<qint64> offsets)
{
    QByteArray bytes;
    for (const auto offset : offsets) append(bytes, origin + offset);
    return bytes;
}
inline QByteArray ints(std::initializer_list<qint32> values)
{
    QByteArray bytes; for (const auto value : values) append(bytes, value); return bytes;
}
inline QByteArray doubles(std::initializer_list<double> values)
{
    QByteArray bytes; for (const auto value : values) append(bytes, std::bit_cast<quint64>(value)); return bytes;
}
inline QMap<QString, QByteArray> members()
{
    return {
        {"session.json", R"({"version":1,"firstTimestamp":1780000000000,"trackName":"Synthetic","laps":[]})"},
        {"sessionfragment.json", R"({"version":1,"firstTimestamp":1780000000000,"primaryGpsDeviceIndex":300})"},
        {"trackId.json", R"({"track":{"traps":[{"type":3,"uniDirectional":true,"centerLatitude":300000000,"centerLongitude":120000000,"width":20000,"bearing":90000}]}})"},
        {"channel_1_300_0_1_1", ticks({100, 200, 300, 400, 2000})},
        {"channel_1_300_0_3_1", ints({300000000,120000000,300000060,120000060,300000120,120000120,300000180,120000180,300000240,120000240})},
        {"channel_1_300_0_4_0", ints({10000,20000,30000,40000,50000})},
        {"channel_5_200_10024_1_1", ticks({250, 1250})},
        {"channel2_5_200_10024_10024_3", doubles({3000,5000})},
        {"channel_5_200_1002_1_1", ticks({250,1250})},
        {"channel2_5_200_1002_1002_3", doubles({0,80})},
        {"channel_6_400_0_1_1", ticks({150,1150})},
        {"channel_6_400_0_41_0", ints({120000,140000})},
        {"channel_2_301_0_1_1", ticks({150,1150})},
        {"channel_2_301_0_9_0", ints({10000,-5000})},
    };
}
inline QByteArray zip(const QMap<QString, QByteArray> &files, bool compressed = true)
{
    QByteArray result, directory;
    for (auto i = files.cbegin(); i != files.cend(); ++i) {
        const auto name = i.key().toUtf8();
        QByteArray payload = i.value();
        const auto size = static_cast<quint32>(payload.size());
        const auto crc = static_cast<quint32>(crc32(0, reinterpret_cast<const Bytef *>(payload.constData()), size));
        if (compressed) {
            z_stream stream{};
            if (deflateInit2(&stream, 6, Z_DEFLATED, -MAX_WBITS, 8, Z_DEFAULT_STRATEGY) != Z_OK)
                throw std::runtime_error("fixture deflate initialization failed");
            QByteArray encoded(static_cast<qsizetype>(deflateBound(&stream, size)), '\0');
            stream.next_in = reinterpret_cast<Bytef *>(payload.data()); stream.avail_in = size;
            stream.next_out = reinterpret_cast<Bytef *>(encoded.data()); stream.avail_out = static_cast<uInt>(encoded.size());
            const int status = deflate(&stream, Z_FINISH);
            encoded.resize(static_cast<qsizetype>(stream.total_out));
            deflateEnd(&stream);
            if (status != Z_STREAM_END) throw std::runtime_error("fixture deflate failed");
            payload = encoded;
        }
        const auto offset = static_cast<quint32>(result.size());
        append<quint32>(result, 0x04034b50); append<quint16>(result, 20); append<quint16>(result, 0);
        append<quint16>(result, compressed ? 8 : 0); append<quint32>(result, 0);
        append(result, crc); append<quint32>(result, static_cast<quint32>(payload.size())); append(result, size);
        append<quint16>(result, static_cast<quint16>(name.size())); append<quint16>(result, 0); result += name; result += payload;
        append<quint32>(directory, 0x02014b50); append<quint16>(directory, 20); append<quint16>(directory, 20);
        append<quint16>(directory, 0); append<quint16>(directory, compressed ? 8 : 0); append<quint32>(directory, 0);
        append(directory, crc); append<quint32>(directory, static_cast<quint32>(payload.size())); append(directory, size);
        append<quint16>(directory, static_cast<quint16>(name.size())); append<quint16>(directory, 0); append<quint16>(directory, 0);
        append<quint16>(directory, 0); append<quint16>(directory, 0); append<quint32>(directory, 0); append(directory, offset); directory += name;
    }
    const auto offset = static_cast<quint32>(result.size()); result += directory;
    append<quint32>(result, 0x06054b50); append<quint32>(result, 0);
    append<quint16>(result, static_cast<quint16>(files.size())); append<quint16>(result, static_cast<quint16>(files.size()));
    append<quint32>(result, static_cast<quint32>(directory.size())); append(result, offset); append<quint16>(result, 0);
    return result;
}
}
