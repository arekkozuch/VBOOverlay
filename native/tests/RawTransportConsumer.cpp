#include <QCoreApplication>
#include <QFile>
#include <QThread>

#include <cstdio>

int main(int argc, char *argv[])
{
    QCoreApplication application(argc, argv);
    const QString mode = application.arguments().value(1);
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
