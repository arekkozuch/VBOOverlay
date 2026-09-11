#pragma once

#include <QLockFile>
#include <QString>

namespace FlappedEar {

// Held for the complete GUI lifetime, before any shared settings/recovery access.
// Export workers do not own GUI state and must not acquire this lock.
class GuiSessionLock final {
public:
    explicit GuiSessionLock(QString dataDirectory = {});
    [[nodiscard]] bool tryAcquire(QString *error = nullptr);

private:
    QString m_directory;
    QLockFile m_lock;
};

} // namespace FlappedEar
