#pragma once

#include "project/ProjectSourceReference.h"

#include <QJsonObject>
#include <QStringList>

namespace FlappedEar {

// v3 owns sources and synchronization per run. The single-run editor projection
// is ephemeral: it must never be serialized alongside the authoritative event.
class EventProjectCodec final {
public:
    static constexpr qsizetype maximumRuns = 64;
    static constexpr qsizetype maximumSourcesPerRun = 8;
    static constexpr qsizetype maximumTelemetrySources = 128;

    [[nodiscard]] static bool isEvent(const QJsonObject &project);
    // Called by ProjectLimits after the common JSON resource checks.
    [[nodiscard]] static bool validate(const QJsonObject &project, QString *error);
    // These adapters accept validated documents; they never open telemetry/media.
    [[nodiscard]] static QJsonObject editorProjection(const QJsonObject &project);
    [[nodiscard]] static QJsonObject withEditorState(
        const QJsonObject &eventProject, QJsonObject editorProject,
        const QString &previousProjectPath, const QString &targetProjectPath);
    [[nodiscard]] static QJsonObject referenceForSave(
        const ProjectSourceReference &reference, const QString &previousProjectPath,
        const QString &targetProjectPath);
    // All referenced path candidates, including inactive sources, for export protection.
    [[nodiscard]] static QStringList referencedPaths(const QJsonObject &project, const QString &projectPath);
};

} // namespace FlappedEar
