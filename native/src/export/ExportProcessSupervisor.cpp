#include "export/ExportProcessSupervisor.h"

#ifdef Q_OS_UNIX
#include <cerrno>
#include <signal.h>
#include <unistd.h>
#endif
#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

namespace FlappedEar {
ExportProcessSupervisor::ExportProcessSupervisor(QProcess &process, const bool isolateProcessGroup)
    : m_process(process), m_isolateProcessGroup(isolateProcessGroup) {}
ExportProcessSupervisor::~ExportProcessSupervisor()
{
    if (isRunning()) static_cast<void>(stopAndWait());
#ifdef Q_OS_WIN
    if (m_job) CloseHandle(static_cast<HANDLE>(m_job));
#endif
}
void ExportProcessSupervisor::start(const QString &program, const QStringList &arguments)
{
#ifdef Q_OS_UNIX
    if (m_isolateProcessGroup) {
        // Runs in the forked child before exec, avoiding the parent-side
        // setpgid race where a fast executable has already changed state.
        m_process.setChildProcessModifier([this] {
            if (::setpgid(0, 0) != 0) {
                m_process.failChildProcessModifier("Could not create export process group", errno);
            }
        });
    }
#endif
    m_process.start(program, arguments);
}
bool ExportProcessSupervisor::waitForStarted(const int milliseconds)
{
    if (!m_process.waitForStarted(milliseconds)) return false;
    m_pid = m_process.processId();
#ifdef Q_OS_UNIX
    // The group was created in the child modifier before exec.
    m_supervisionActive = m_isolateProcessGroup;
#elif defined(Q_OS_WIN)
    if (!m_isolateProcessGroup) return true; // Inherits the worker's job.
    HANDLE job = CreateJobObjectW(nullptr, nullptr);
    if (!job) {
        m_supervisionError = QStringLiteral("CreateJobObject failed (error %1).").arg(GetLastError());
        return false;
    }
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (!SetInformationJobObject(job, JobObjectExtendedLimitInformation, &limits, sizeof(limits))) {
        m_supervisionError = QStringLiteral("SetInformationJobObject failed (error %1).").arg(GetLastError());
        CloseHandle(job);
        return false;
    }
    HANDLE process = OpenProcess(PROCESS_SET_QUOTA | PROCESS_TERMINATE, FALSE, static_cast<DWORD>(m_pid));
    if (!process) {
        m_supervisionError = QStringLiteral("OpenProcess for export worker failed (error %1).").arg(GetLastError());
        CloseHandle(job);
        return false;
    }
    if (!AssignProcessToJobObject(job, process)) {
        m_supervisionError = QStringLiteral("AssignProcessToJobObject failed (error %1).").arg(GetLastError());
        CloseHandle(process);
        CloseHandle(job);
        return false;
    }
    CloseHandle(process);
    m_job = job;
    m_supervisionActive = true;
#endif
    return true;
}
bool ExportProcessSupervisor::isRunning() const { return m_process.state() != QProcess::NotRunning; }
bool ExportProcessSupervisor::supervisionActive() const { return m_supervisionActive; }
QString ExportProcessSupervisor::supervisionError() const { return m_supervisionError; }
bool ExportProcessSupervisor::stopAndWait(const int gracefulMilliseconds, const int forceMilliseconds)
{
    if (!isRunning()) return true;
#ifdef Q_OS_UNIX
    if (m_isolateProcessGroup && m_pid > 0) ::kill(-static_cast<pid_t>(m_pid), SIGTERM); else m_process.terminate();
#else
    m_process.terminate();
#endif
    if (m_process.waitForFinished(gracefulMilliseconds)) return true;
#ifdef Q_OS_UNIX
    if (m_isolateProcessGroup && m_pid > 0) ::kill(-static_cast<pid_t>(m_pid), SIGKILL); else m_process.kill();
#elif defined(Q_OS_WIN)
    if (m_job) TerminateJobObject(static_cast<HANDLE>(m_job), 1); else m_process.kill();
#else
    m_process.kill();
#endif
    return m_process.waitForFinished(forceMilliseconds);
}
} // namespace FlappedEar
