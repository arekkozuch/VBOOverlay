#include "export/ExportDiagnostics.h"

#include <QtGlobal>

namespace FlappedEar {

void ExportStageTimer::start(const qint64 nowMilliseconds, const QString &stage)
{
    m_totalStartedMilliseconds = nowMilliseconds;
    m_stageStartedMilliseconds = nowMilliseconds;
    m_stage = stage;
    m_completedStageDurations.clear();
}

void ExportStageTimer::transition(const qint64 nowMilliseconds, const QString &stage)
{
    if (m_stage == stage) {
        return;
    }
    if (!m_stage.isEmpty()) {
        m_completedStageDurations.insert(
            m_stage, qMax<qint64>(0, nowMilliseconds - m_stageStartedMilliseconds));
    }
    m_stage = stage;
    m_stageStartedMilliseconds = nowMilliseconds;
}

qint64 ExportStageTimer::totalElapsedMilliseconds(const qint64 nowMilliseconds) const
{
    return qMax<qint64>(0, nowMilliseconds - m_totalStartedMilliseconds);
}

qint64 ExportStageTimer::stageElapsedMilliseconds(const qint64 nowMilliseconds) const
{
    return qMax<qint64>(0, nowMilliseconds - m_stageStartedMilliseconds);
}

QHash<QString, qint64> ExportStageTimer::completedStageDurations() const
{
    return m_completedStageDurations;
}

QString ExportStageTimer::stage() const { return m_stage; }

DiagnosticHeartbeat::DiagnosticHeartbeat(const qint64 intervalMilliseconds)
    : m_intervalMilliseconds(qMax<qint64>(1, intervalMilliseconds))
{
}

bool DiagnosticHeartbeat::shouldEmit(const qint64 elapsedMilliseconds)
{
    if (m_lastEmissionMilliseconds < 0
        || elapsedMilliseconds - m_lastEmissionMilliseconds >= m_intervalMilliseconds) {
        m_lastEmissionMilliseconds = elapsedMilliseconds;
        return true;
    }
    return false;
}

BoundedDiagnosticLog::BoundedDiagnosticLog(const qsizetype maximumEntries)
    : m_maximumEntries(qMax<qsizetype>(2, maximumEntries))
{
}

void BoundedDiagnosticLog::clear()
{
    m_entries.clear();
    m_omissionMarkerNeeded = false;
}

void BoundedDiagnosticLog::append(const QString &entry)
{
    m_entries.append(entry);
    const qsizetype contentLimit = m_maximumEntries - 1;
    while (m_entries.size() > contentLimit) {
        m_entries.removeFirst();
        m_omissionMarkerNeeded = true;
    }
}

QString BoundedDiagnosticLog::text() const
{
    QStringList visible = m_entries;
    if (m_omissionMarkerNeeded) {
        visible.prepend(QStringLiteral("[older diagnostic entries omitted]"));
    }
    return visible.join(QLatin1Char('\n'));
}

qsizetype BoundedDiagnosticLog::size() const
{
    return m_entries.size() + (m_omissionMarkerNeeded ? 1 : 0);
}

qsizetype BoundedDiagnosticLog::maximumEntries() const { return m_maximumEntries; }

} // namespace FlappedEar
