#pragma once

#include <QJsonDocument>
#include <QJsonObject>
#include <QString>

namespace FlappedEar::ProjectLimits {

// These are deliberately sized for complex broadcast layouts while preventing an
// imported document from turning into an unbounded QML/model allocation.
inline constexpr qint64 projectBytes = 4 * 1024 * 1024;
inline constexpr qint64 templateBytes = 2 * 1024 * 1024;
inline constexpr qint64 templateStoreBytes = 8 * 1024 * 1024;
inline constexpr qint64 recoveryBytes = 4 * 1024 * 1024;
inline constexpr qint64 manifestBytes = 64 * 1024;
inline constexpr qsizetype maximumWidgets = 256;
inline constexpr qsizetype maximumCuesPerWidget = 256;
inline constexpr qsizetype maximumTotalCues = 4096;
inline constexpr qsizetype maximumSettingsEntries = 128;
inline constexpr qsizetype maximumTemplateCount = 128;
inline constexpr qsizetype maximumJsonDepth = 32;
inline constexpr qsizetype maximumStringCharacters = 4096;
inline constexpr qsizetype maximumIdCharacters = 128;
inline constexpr qsizetype maximumTemplateNameCharacters = 160;
inline constexpr qsizetype maximumTemplateDescriptionCharacters = 2048;

bool validateProject(const QJsonObject &project, QString *error = nullptr);
bool validateTemplate(const QJsonObject &templateObject, QString *error = nullptr);
bool validateTemplateStore(const QJsonObject &store, QString *error = nullptr);

} // namespace FlappedEar::ProjectLimits
