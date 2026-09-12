#include "export/PersistentExportLog.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTimeZone>
#include <QUuid>
#include <QtTest>

using namespace FlappedEar;

class PersistentExportLogTests final : public QObject {
    Q_OBJECT

private slots:
    void retainsProductionAndLegacyLogsWithoutRemovingUnrelatedFiles();
    void preservesSymbolicLinks();
};

void PersistentExportLogTests::retainsProductionAndLegacyLogsWithoutRemovingUnrelatedFiles()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QDateTime started(QDate(2026, 9, 12), QTime(10, 0), QTimeZone::UTC);
    QString error;
    auto active = PersistentExportLog::create(
        directory.path(), QUuid::createUuid().toString(QUuid::WithoutBraces),
        QStringLiteral("Active export"), &error, started);
    QVERIFY2(active, qPrintable(error));

    QStringList ownedPaths;
    for (int index = 0; index < 12; ++index) {
        const QString id = index % 2 == 0
            ? QUuid::createUuid().toString(QUuid::WithoutBraces)
            : QStringLiteral("%1abcdef012345678").arg(index, 8, 16, QLatin1Char('0'));
        auto log = PersistentExportLog::create(
            directory.path(), id, QStringLiteral("Finished export"), &error, started.addSecs(index + 1));
        QVERIFY2(log, qPrintable(error));
        ownedPaths.append(log->path());
    }

    const QStringList unrelatedNames{
        QStringLiteral("notes.log"),
        QStringLiteral("export-20260912-090000-not-a-uuid.log"),
        QStringLiteral("export-20260912-090000-12345678-1234-1234-1234-12345678901.log"),
        QStringLiteral("export-20260912-090000-12345678-1234-1234-1234-123456789012-extra.log"),
        QStringLiteral("export-20260912-090000-12345678.log.backup"),
    };
    for (const QString &name : unrelatedNames) {
        QFile file(directory.filePath(name));
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write("Keep me"), qint64(7));
    }

    PersistentExportLog::retainNewest(directory.path(), active->path(), 3);
    QVERIFY(QFileInfo::exists(active->path()));
    QVERIFY(active->append(QStringLiteral("Still active")));
    for (qsizetype index = 0; index < ownedPaths.size(); ++index)
        QCOMPARE(QFileInfo::exists(ownedPaths.at(index)), index >= ownedPaths.size() - 3);
    for (const QString &name : unrelatedNames) {
        QFile file(directory.filePath(name));
        QVERIFY2(file.open(QIODevice::ReadOnly), qPrintable(name));
        QCOMPARE(file.readAll(), QByteArray("Keep me"));
    }
}

void PersistentExportLogTests::preservesSymbolicLinks()
{
#ifdef Q_OS_UNIX
    QTemporaryDir directory;
    QTemporaryDir external;
    QVERIFY(directory.isValid());
    QVERIFY(external.isValid());
    const QString targetPath = external.filePath(QStringLiteral("external.log"));
    {
        QFile target(targetPath);
        QVERIFY(target.open(QIODevice::WriteOnly));
        QCOMPARE(target.write("External contents"), qint64(17));
    }
    const QString linkPath = directory.filePath(QStringLiteral(
        "export-20260912-100000-12345678-1234-1234-1234-123456789012.log"));
    QVERIFY(QFile::link(targetPath, linkPath));
    QVERIFY(QFileInfo(linkPath).isSymbolicLink());

    PersistentExportLog::retainNewest(directory.path(), {}, 0);
    QVERIFY(QFileInfo(linkPath).isSymbolicLink());
    QFile target(targetPath);
    QVERIFY(target.open(QIODevice::ReadOnly));
    QCOMPARE(target.readAll(), QByteArray("External contents"));
#else
    QSKIP("Symbolic-link creation is exercised on Unix; Windows QFile::link creates shortcuts.");
#endif
}

QTEST_GUILESS_MAIN(PersistentExportLogTests)
#include "PersistentExportLogTests.moc"
