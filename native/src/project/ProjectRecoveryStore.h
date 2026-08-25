#pragma once

#include <QJsonObject>
#include <QString>

#include <functional>

namespace FlappedEar {

struct ProjectRecoverySnapshot final {
    QString originalProjectPath;
    QString documentId;
    quint64 revision = 0;
    quint64 lastSavedRevision = 0;
    QString timestamp;
    QJsonObject project;
    bool hasLogicalMetadata = false;
};

struct ProjectRecoveryDiscardTombstone final {
    QString documentId;
    quint64 discardedThroughRevision = 0;
};

class ProjectRecoveryStore final {
public:
    struct Operations final {
        // Deterministic failure injection for recovery persistence tests.
        std::function<bool(QString *error)> clearSnapshot;
        std::function<bool(QString *error)> writeDiscardTombstone;
    };

    explicit ProjectRecoveryStore(QString path = {}, Operations operations = {});

    [[nodiscard]] QString path() const;
    [[nodiscard]] QString discardTombstonePath() const;
    [[nodiscard]] bool exists() const;
    [[nodiscard]] bool load(ProjectRecoverySnapshot *snapshot, QString *error = nullptr) const;
    [[nodiscard]] bool write(const ProjectRecoverySnapshot &snapshot, QString *error = nullptr) const;
    [[nodiscard]] bool clear(QString *error = nullptr) const;
    [[nodiscard]] bool loadDiscardTombstone(
        ProjectRecoveryDiscardTombstone *tombstone, QString *error = nullptr) const;
    [[nodiscard]] bool writeDiscardTombstone(
        const ProjectRecoveryDiscardTombstone &tombstone, QString *error = nullptr) const;
    [[nodiscard]] bool clearDiscardTombstone(QString *error = nullptr) const;

private:
    QString m_path;
    Operations m_operations;
};

} // namespace FlappedEar
