#pragma once

#include <QString>
#include <QtLogging>

namespace FlappedEar::AppLog {

[[nodiscard]] bool initialize();
void shutdown();
[[nodiscard]] QString filePath();

void info(const QString &message);
void warn(const QString &message);
void error(const QString &message);
void debug(const QString &message);

void installQtMessageHandler();
void restoreQtMessageHandler();

} // namespace FlappedEar::AppLog
