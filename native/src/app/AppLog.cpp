#include "app/AppLog.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QMutex>
#include <QMutexLocker>
#include <QStandardPaths>

#include <cstdio>

namespace FlappedEar::AppLog {
namespace {

QMutex logMutex;
QFile logFile;
QString currentLogPath;
QtMessageHandler previousMessageHandler = nullptr;
thread_local bool handlingQtMessage = false;

void writeConsole(const QtMsgType type, const QMessageLogContext &context, const QString &message)
{
    if (previousMessageHandler) {
        previousMessageHandler(type, context, message);
        return;
    }
    const QByteArray formatted = qFormatLogMessage(type, context, message).toLocal8Bit();
    std::fwrite(formatted.constData(), 1, static_cast<size_t>(formatted.size()), stderr);
    std::fputc('\n', stderr);
    std::fflush(stderr);
}

void writeLine(const char *level, QString message)
{
    message.replace(QStringLiteral("\r\n"), QStringLiteral(" | "));
    message.replace(u'\r', u' ');
    message.replace(u'\n', QStringLiteral(" | "));
    const QByteArray line = QStringLiteral("%1 [%2] %3\n")
                                .arg(QDateTime::currentDateTime().toString(
                                         QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz")),
                                     QString::fromLatin1(level), message)
                                .toUtf8();
    QMutexLocker locker(&logMutex);
    if (!logFile.isOpen()) {
        return;
    }
    static_cast<void>(logFile.write(line));
    logFile.flush();
}

const char *levelFor(const QtMsgType type)
{
    switch (type) {
    case QtDebugMsg:
        return "DEBUG";
    case QtInfoMsg:
        return "INFO";
    case QtWarningMsg:
        return "WARN";
    case QtCriticalMsg:
    case QtFatalMsg:
        return "ERROR";
    }
    return "INFO";
}

void qtMessageHandler(const QtMsgType type, const QMessageLogContext &context, const QString &message)
{
    if (handlingQtMessage) {
        writeConsole(type, context, message);
        return;
    }
    handlingQtMessage = true;
    QString loggedMessage = message;
    if (context.category && context.category[0] != '\0') {
        loggedMessage = QStringLiteral("Qt[%1]: %2")
                            .arg(QString::fromUtf8(context.category), message);
    }
    writeLine(levelFor(type), loggedMessage);
    writeConsole(type, context, message);
    handlingQtMessage = false;
}

} // namespace

bool initialize()
{
    const QString directoryPath = QStandardPaths::writableLocation(
        QStandardPaths::AppLocalDataLocation);
    QMutexLocker locker(&logMutex);
    QDir directory(directoryPath);
    if (!directory.mkpath(QStringLiteral("."))) {
        std::fprintf(stderr, "Could not create application log directory: %s\n",
                     directoryPath.toLocal8Bit().constData());
        return false;
    }
    const QString previousPath = directory.filePath(QStringLiteral("flappedear.previous.log"));
    currentLogPath = directory.filePath(QStringLiteral("flappedear.log"));
    if (QFile::exists(previousPath) && !QFile::remove(previousPath)) {
        std::fprintf(stderr, "Could not remove previous application log: %s\n",
                     previousPath.toLocal8Bit().constData());
    }
    if (QFile::exists(currentLogPath) && !QFile::rename(currentLogPath, previousPath)) {
        std::fprintf(stderr, "Could not rotate application log: %s\n",
                     currentLogPath.toLocal8Bit().constData());
    }
    logFile.setFileName(currentLogPath);
    if (!logFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        std::fprintf(stderr, "Could not open application log: %s\n",
                     logFile.errorString().toLocal8Bit().constData());
        currentLogPath.clear();
        return false;
    }
    return true;
}

void shutdown()
{
    QMutexLocker locker(&logMutex);
    if (logFile.isOpen()) {
        logFile.flush();
        logFile.close();
    }
}

QString filePath()
{
    QMutexLocker locker(&logMutex);
    return currentLogPath;
}

void info(const QString &message) { writeLine("INFO", message); }
void warn(const QString &message) { writeLine("WARN", message); }
void error(const QString &message) { writeLine("ERROR", message); }
void debug(const QString &message) { writeLine("DEBUG", message); }

void installQtMessageHandler()
{
    previousMessageHandler = qInstallMessageHandler(qtMessageHandler);
}

void restoreQtMessageHandler()
{
    qInstallMessageHandler(previousMessageHandler);
    previousMessageHandler = nullptr;
}

} // namespace FlappedEar::AppLog
