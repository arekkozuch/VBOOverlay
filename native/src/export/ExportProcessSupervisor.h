#pragma once

#include <QProcess>

namespace FlappedEar {

// Owns the complete process tree started by one QProcess. On Unix the direct
// child gets a dedicated process group; on Windows it is assigned to a Job
// Object configured to kill descendants when the job closes.
class ExportProcessSupervisor final {
public:
    explicit ExportProcessSupervisor(QProcess &process, bool isolateProcessGroup = true);
    ~ExportProcessSupervisor();
    ExportProcessSupervisor(const ExportProcessSupervisor &) = delete;
    void start(const QString &program, const QStringList &arguments);
    [[nodiscard]] bool waitForStarted(int milliseconds = 30'000);
    [[nodiscard]] bool stopAndWait(int gracefulMilliseconds = 5'000, int forceMilliseconds = 5'000);
    [[nodiscard]] bool isRunning() const;
private:
    QProcess &m_process;
    bool m_isolateProcessGroup = true;
    qint64 m_pid = 0;
#ifdef Q_OS_WIN
    void *m_job = nullptr;
#endif
};

} // namespace FlappedEar
