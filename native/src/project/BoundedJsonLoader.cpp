#include "project/BoundedJsonLoader.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonParseError>

namespace FlappedEar {

BoundedJsonLoader::Result BoundedJsonLoader::loadFile(
    const QString &path, const qint64 maximumBytes, const QString &documentName, Validator validator)
{
    Result result;
    const QFileInfo info(path);
    if (!info.isFile()) { result.error = QStringLiteral("%1 is not a readable file.").arg(documentName); return result; }
    if (info.size() > maximumBytes) {
        result.error = QStringLiteral("%1 is %2 bytes; the limit is %3 bytes.").arg(documentName).arg(info.size()).arg(maximumBytes);
        return result;
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) { result.error = QStringLiteral("Could not read %1: %2").arg(documentName, file.errorString()); return result; }
    QByteArray bytes;
    bytes.reserve(static_cast<qsizetype>(info.size()));
    while (!file.atEnd()) {
        const QByteArray chunk = file.read(qMin<qint64>(64 * 1024, maximumBytes - bytes.size() + 1));
        if (chunk.isEmpty() && file.error() != QFileDevice::NoError) { result.error = file.errorString(); return result; }
        bytes += chunk;
        if (bytes.size() > maximumBytes) { result.error = QStringLiteral("%1 exceeds its %2-byte limit while reading.").arg(documentName).arg(maximumBytes); return result; }
    }
    result.bytesRead = bytes.size();
    QJsonParseError parseError;
    result.document = QJsonDocument::fromJson(bytes, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        result.error = QStringLiteral("%1 has invalid JSON at byte %2: %3").arg(documentName).arg(parseError.offset).arg(parseError.errorString());
        return result;
    }
    if (validator && !validator(result.document, &result.error)) return result;
    return result;
}

} // namespace FlappedEar
