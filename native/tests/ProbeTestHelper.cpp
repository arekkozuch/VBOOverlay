#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QThread>

int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);
    const QStringList arguments = application.arguments();
    const bool packets = arguments.contains(QStringLiteral("-show_packets"));
    const QString inputName = QFileInfo(arguments.constLast()).fileName();
    if (inputName.contains(QStringLiteral("slow"))) {
        QThread::msleep(30'000);
        return 0;
    }
    QFile output;
    if (!output.open(stdout, QIODevice::WriteOnly)) return 3;
    if (!packets) {
        output.write("{\"streams\":[{\"codec_type\":\"data\",\"codec_tag_string\":\"gpmd\",\"index\":0}],"
                     "\"format\":{\"duration\":\"1.0\"}}");
        return 0;
    }
    if (inputName.contains(QStringLiteral("large-output"))) {
        const QByteArray block(64 * 1024, 'x');
        for (int index = 0; index < 1025; ++index) {
            if (output.write(block) != block.size()) return 2;
            output.flush();
        }
        return 0;
    }
    if (inputName.contains(QStringLiteral("outside"))) {
        output.write("{\"packets\":[{\"pos\":\"99\",\"size\":\"10\",\"pts_time\":\"0\",\"duration_time\":\"1\"}]}");
        return 0;
    }
    if (inputName.contains(QStringLiteral("overflow"))) {
        output.write("{\"packets\":[{\"pos\":\"9223372036854775800\",\"size\":\"100\",\"pts_time\":\"0\",\"duration_time\":\"1\"}]}");
        return 0;
    }
    output.write("{\"packets\":[]}");
    return 0;
}
