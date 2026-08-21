#pragma once

#include <QString>
#include <QStringList>
#include <QSet>

namespace FlappedEar {

class ExportOutputTransaction final {
public:
    enum class PreparationStatus {
        Ready,
        OverwriteConfirmationRequired,
        Error,
    };

    struct PreparationResult {
        PreparationStatus status = PreparationStatus::Error;
        QString error;
    };

    ExportOutputTransaction() = default;
    ~ExportOutputTransaction();
    ExportOutputTransaction(const ExportOutputTransaction &) = delete;
    ExportOutputTransaction &operator=(const ExportOutputTransaction &) = delete;

    [[nodiscard]] PreparationResult prepare(
        const QString &userTargetPath,
        const QString &inputPath,
        const QStringList &collisionPaths,
        bool overwriteAllowed);
    [[nodiscard]] bool commit(QString *error = nullptr);
    void cleanup();

    [[nodiscard]] QString userTargetPath() const;
    [[nodiscard]] QString stagingPath() const;
    [[nodiscard]] QString transactionId() const;
    [[nodiscard]] bool targetExistedBeforeExport() const;
    [[nodiscard]] bool overwriteAllowed() const;
    [[nodiscard]] bool ownsPath(const QString &path) const;
    [[nodiscard]] static QString normalizedComparisonPath(const QString &path);

private:
    [[nodiscard]] bool replaceExisting(QString *error);
    [[nodiscard]] bool fallbackReplaceExisting(QString *error);
    void reset();

    QString m_userTargetPath;
    QString m_stagingPath;
    QString m_transactionId;
    QSet<QString> m_ownedPaths;
    bool m_targetExistedBeforeExport = false;
    bool m_overwriteAllowed = false;
    bool m_committed = false;
};

} // namespace FlappedEar
