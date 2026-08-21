#include "project/ProjectWriter.h"

#include <QSaveFile>

#include <utility>

namespace FlappedEar {
namespace {

class QSaveFileDevice final : public ProjectWriteDevice {
public:
    explicit QSaveFileDevice(const QString &path)
        : m_file(path)
    {
        m_file.setDirectWriteFallback(false);
    }

    bool open() override { return m_file.open(QIODevice::WriteOnly); }
    qint64 write(const QByteArray &payload) override { return m_file.write(payload); }
    bool commit() override { return m_file.commit(); }
    QString errorString() const override { return m_file.errorString(); }

private:
    QSaveFile m_file;
};

} // namespace

ProjectWriter::ProjectWriter(DeviceFactory deviceFactory)
    : m_deviceFactory(std::move(deviceFactory))
{
    if (!m_deviceFactory) {
        m_deviceFactory = [](const QString &path) {
            return std::make_unique<QSaveFileDevice>(path);
        };
    }
}

ProjectWriter::Result ProjectWriter::write(const QString &path, const QByteArray &payload) const
{
    if (path.isEmpty()) {
        return {false, QStringLiteral("Project path is empty.")};
    }
    std::unique_ptr<ProjectWriteDevice> device = m_deviceFactory(path);
    if (!device) {
        return {false, QStringLiteral("Could not create the atomic project writer.")};
    }
    if (!device->open()) {
        return {false, QStringLiteral("Could not open the project for atomic saving: %1")
                           .arg(device->errorString())};
    }
    const qint64 written = device->write(payload);
    if (written != payload.size()) {
        return {false, QStringLiteral("Project save was incomplete: wrote %1 of %2 bytes (%3)")
                           .arg(written)
                           .arg(payload.size())
                           .arg(device->errorString())};
    }
    if (!device->commit()) {
        return {false, QStringLiteral("Could not commit the project atomically: %1")
                           .arg(device->errorString())};
    }
    return {true, {}};
}

} // namespace FlappedEar
