#include "app/GuiSessionLock.h"

#include <QCoreApplication>
#include <QFile>
#include <QTextStream>

int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);
    if (application.arguments().size() != 2) return 2;
    FlappedEar::GuiSessionLock lock(application.arguments().at(1));
    if (!lock.tryAcquire()) return 3;
    QFile output;
    if (!output.open(stdout, QIODevice::WriteOnly)) return 4;
    if (output.write("locked\n") != 7 || !output.flush()) return 5;
    QTextStream input(stdin);
    input.readLine(); // Test sends a newline for clean exit, or kills us for crash recovery.
    return 0;
}
