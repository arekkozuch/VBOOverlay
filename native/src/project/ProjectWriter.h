#pragma once

#include <QByteArray>
#include <QString>

#include <functional>
#include <memory>

namespace FlappedEar {

class ProjectWriteDevice {
public:
    virtual ~ProjectWriteDevice() = default;
    [[nodiscard]] virtual bool open() = 0;
    [[nodiscard]] virtual qint64 write(const QByteArray &payload) = 0;
    [[nodiscard]] virtual bool commit() = 0;
    [[nodiscard]] virtual QString errorString() const = 0;
};

class ProjectWriter final {
public:
    struct Result {
        bool success = false;
        QString error;
    };

    using DeviceFactory = std::function<std::unique_ptr<ProjectWriteDevice>(const QString &path)>;

    explicit ProjectWriter(DeviceFactory deviceFactory = {});
    [[nodiscard]] Result write(const QString &path, const QByteArray &payload) const;

private:
    DeviceFactory m_deviceFactory;
};

} // namespace FlappedEar
