#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QThread>

#include <cstdio>
#ifdef Q_OS_UNIX
#include <signal.h>
#include <unistd.h>
#endif

int main(int argc, char *argv[])
{
    QCoreApplication application(argc, argv);
    const QString mode = application.arguments().value(1);
#ifdef Q_OS_UNIX
    if (mode == QStringLiteral("tree-leader-exits") || mode == QStringLiteral("tree-leader-waits")) {
        const QString artifactPath = application.arguments().value(2);
        const QString readyPath = application.arguments().value(3);
        const pid_t child = ::fork();
        if (child < 0) return 10;
        if (child == 0) {
            ::signal(SIGTERM, SIG_IGN);
            ::close(STDIN_FILENO);
            ::close(STDOUT_FILENO);
            ::close(STDERR_FILENO);
            // Reopen on every write: premature cleanup must be detected even
            // when a removed transaction artifact can be recreated by a writer.
            for (int i = 0; i < 1'500; ++i) {
                QFile artifact(artifactPath);
                if (!artifact.open(QIODevice::WriteOnly | QIODevice::Append)
                    || artifact.write("x", 1) != 1) ::_exit(11);
                artifact.close();
                if (i == 0) {
                    QFile ready(readyPath);
                    if (!ready.open(QIODevice::WriteOnly)
                        || ready.write(QByteArray::number(::getpid())) <= 0) ::_exit(12);
                    ready.close();
                }
                QThread::msleep(20);
            }
            ::_exit(0);
        }
        // The test releases the leader only after observing the child's first
        // write and installed SIGTERM handler. No startup timing assumption.
        if (std::getchar() != 'R') return 13;
        if (mode == QStringLiteral("tree-leader-waits")) QThread::sleep(30);
        return 0;
    }
#endif
    if (mode == QStringLiteral("stall")) {
        QThread::sleep(30);
        return 0;
    }

    QFile input;
    if (!input.open(stdin, QIODevice::ReadOnly)) return 2;
    qint64 receivedBytes = 0;
    while (true) {
        const QByteArray bytes = input.read(64 * 1024);
        if (bytes.isEmpty()) {
            if (input.atEnd()) break;
            return 3;
        }
        receivedBytes += bytes.size();
        if (mode == QStringLiteral("early-exit") && receivedBytes >= 2LL * 1024LL * 1024LL) {
            return 7;
        }
        if (mode == QStringLiteral("slow")) QThread::msleep(2);
    }

    QFile output;
    if (!output.open(stdout, QIODevice::WriteOnly)) return 4;
    if (output.write(QByteArray::number(receivedBytes)) < 0) return 5;
    return 0;
}
