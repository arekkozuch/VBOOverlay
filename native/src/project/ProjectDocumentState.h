#pragma once

#include <QString>

namespace FlappedEar {

class ProjectDocumentState final {
public:
    enum class DestructiveAction {
        None,
        NewProject,
        OpenProject,
        Quit,
    };

    enum class RequestResult {
        ContinueImmediately,
        DecisionRequired,
    };

    void markChanged();
    void markSaved(const QString &projectPath);
    void reset(const QString &projectPath = {}, quint64 savedRevision = 0);
    void restoreUnsaved(const QString &projectPath, quint64 revision, quint64 lastSavedRevision);

    [[nodiscard]] QString projectPath() const;
    [[nodiscard]] bool dirty() const;
    [[nodiscard]] quint64 revision() const;
    [[nodiscard]] quint64 lastSavedRevision() const;

    [[nodiscard]] RequestResult request(DestructiveAction action);
    [[nodiscard]] DestructiveAction pendingAction() const;
    [[nodiscard]] DestructiveAction takePendingAction();
    void cancelPendingAction();
    [[nodiscard]] static QString actionName(DestructiveAction action);

private:
    QString m_projectPath;
    quint64 m_revision = 0;
    quint64 m_lastSavedRevision = 0;
    DestructiveAction m_pendingAction = DestructiveAction::None;
};

} // namespace FlappedEar
