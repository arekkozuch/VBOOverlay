#pragma once

#include <QJsonObject>
#include <QString>

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

class ProjectRecoveryStore final {
public:
    explicit ProjectRecoveryStore(QString path = {});

    [[nodiscard]] QString path() const;
    [[nodiscard]] bool exists() const;
    [[nodiscard]] bool load(ProjectRecoverySnapshot *snapshot, QString *error = nullptr) const;
    [[nodiscard]] bool write(const ProjectRecoverySnapshot &snapshot, QString *error = nullptr) const;
    [[nodiscard]] bool clear(QString *error = nullptr) const;

private:
    QString m_path;
};

} // namespace FlappedEar
