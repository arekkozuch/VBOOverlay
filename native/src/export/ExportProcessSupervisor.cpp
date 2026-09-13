#include "export/ExportProcessSupervisor.h"

#include <QElapsedTimer>
#include <QScopedValueRollback>
#include <QThread>
#include <algorithm>

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
    static_cast<void>(stopAndWait());
#ifdef Q_OS_WIN
    if (m_job) CloseHandle(static_cast<HANDLE>(m_job));
#endif
}
void ExportProcessSupervisor::start(const QString &program, const QStringList &arguments)
{
    // Never replace ownership while an earlier group/job can still write.
    if (isRunning() && !stopAndWait()) return;
    m_pid = 0;
    m_supervisionActive = false;
    m_supervisionError.clear();
#ifdef Q_OS_WIN
    if (m_job) CloseHandle(static_cast<HANDLE>(m_job));
    m_job = nullptr;
#endif
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
bool ExportProcessSupervisor::isRunning() const
{
    if (m_process.state() != QProcess::NotRunning) return true;
#ifdef Q_OS_UNIX
    if (m_supervisionActive && m_pid > 1) {
        // ESRCH is the only proof that the owned group is gone. A leader's
        // QProcess can be finished while descendants still own export files.
        return ::kill(-static_cast<pid_t>(m_pid), 0) == 0 || errno != ESRCH;
    }
#elif defined(Q_OS_WIN)
    if (m_job) {
        JOBOBJECT_BASIC_ACCOUNTING_INFORMATION accounting{};
        if (!QueryInformationJobObject(static_cast<HANDLE>(m_job), JobObjectBasicAccountingInformation,
                                       &accounting, sizeof(accounting), nullptr)) return true;
        return accounting.ActiveProcesses != 0;
    }
#endif
    return false;
}
bool ExportProcessSupervisor::supervisionActive() const { return m_supervisionActive; }
QString ExportProcessSupervisor::supervisionError() const { return m_supervisionError; }
bool ExportProcessSupervisor::stopAndWait(const int gracefulMilliseconds, const int forceMilliseconds)
{
    // waitForFinished emits signals. A reentrant request must not publish a
    // successful stop while the outer call is still supervising descendants.
    if (m_stopping) return false;
    const QScopedValueRollback<bool> stopping(m_stopping, true);
    if (!isRunning()) {
        m_pid = 0;
        return true;
    }
#ifdef Q_OS_UNIX
    if (m_supervisionActive && m_pid > 1) ::kill(-static_cast<pid_t>(m_pid), SIGTERM);
    else if (m_process.state() != QProcess::NotRunning) m_process.terminate();
#else
    m_process.terminate();
#endif
    if (waitForStopped(gracefulMilliseconds)) return true;
#ifdef Q_OS_UNIX
    if (m_supervisionActive && m_pid > 1) ::kill(-static_cast<pid_t>(m_pid), SIGKILL);
    else if (m_process.state() != QProcess::NotRunning) m_process.kill();
#elif defined(Q_OS_WIN)
    if (m_job) TerminateJobObject(static_cast<HANDLE>(m_job), 1); else m_process.kill();
#else
    m_process.kill();
#endif
    if (waitForStopped(forceMilliseconds)) return true;
    m_supervisionError = QStringLiteral("Export process tree did not stop within the shutdown deadline.");
    return false;
}
bool ExportProcessSupervisor::waitForStopped(const int milliseconds)
{
    QElapsedTimer timer;
    timer.start();
    const int budget = std::max(0, milliseconds);
    while (isRunning()) {
        const qint64 remaining = static_cast<qint64>(budget) - timer.elapsed();
        if (remaining <= 0) return false;
        const int interval = static_cast<int>(std::min<qint64>(10, remaining));
        if (m_process.state() != QProcess::NotRunning) {
            static_cast<void>(m_process.waitForFinished(interval));
        } else {
            QThread::msleep(static_cast<unsigned long>(interval));
        }
    }
    // Retire the group identity before a later idempotent stop or destruction.
    m_pid = 0;
    return true;
}
} // namespace FlappedEar
