#pragma once

#include <QJsonDocument>
#include <QString>

#include <functional>

namespace FlappedEar {

class BoundedJsonLoader final {
public:
    struct Result {
        QJsonDocument document;
        qint64 bytesRead = 0;
        QString error;
        [[nodiscard]] bool success() const { return error.isEmpty(); }
    };
    using Validator = std::function<bool(const QJsonDocument &, QString *)>;

    static Result loadFile(const QString &path, qint64 maximumBytes, const QString &documentName,
                           Validator validator = {});
};

} // namespace FlappedEar
