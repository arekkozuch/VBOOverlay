#pragma once

#include <QHash>
#include <QString>
#include <QStringList>

namespace FlappedEar {

class ExportStageTimer final {
public:
    void start(qint64 nowMilliseconds, const QString &stage);
    void transition(qint64 nowMilliseconds, const QString &stage);
    [[nodiscard]] qint64 totalElapsedMilliseconds(qint64 nowMilliseconds) const;
    [[nodiscard]] qint64 stageElapsedMilliseconds(qint64 nowMilliseconds) const;
    [[nodiscard]] QHash<QString, qint64> completedStageDurations() const;
    [[nodiscard]] QString stage() const;

private:
    qint64 m_totalStartedMilliseconds = 0;
    qint64 m_stageStartedMilliseconds = 0;
    QString m_stage;
    QHash<QString, qint64> m_completedStageDurations;
};

class DiagnosticHeartbeat final {
public:
    explicit DiagnosticHeartbeat(qint64 intervalMilliseconds = 500);
    [[nodiscard]] bool shouldEmit(qint64 elapsedMilliseconds);

private:
    qint64 m_intervalMilliseconds = 500;
    qint64 m_lastEmissionMilliseconds = 0;
};

class BoundedDiagnosticLog final {
public:
    explicit BoundedDiagnosticLog(qsizetype maximumEntries = 1500);
    void clear();
    void append(const QString &entry);
    [[nodiscard]] QString text() const;
    [[nodiscard]] qsizetype size() const;
    [[nodiscard]] qsizetype maximumEntries() const;

private:
    qsizetype m_maximumEntries = 1500;
    bool m_omissionMarkerNeeded = false;
    QStringList m_entries;
};

} // namespace FlappedEar
