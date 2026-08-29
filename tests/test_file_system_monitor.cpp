#include <QtTest>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>

#include "monitor/FileSystemMonitor.h"

class TestFileSystemMonitor : public QObject {
    Q_OBJECT

private slots:
    void capturesCreateModifyAndDelete()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());

        FileSystemMonitor monitor;
        monitor.setPollInterval(100);
        monitor.addWatchPath(directory.path());
        QSignalSpy eventSpy(&monitor, &IMonitor::rawEventCaptured);
        QVERIFY(monitor.start());

        const QString path = directory.filePath("main.cpp");
        {
            QFile file(path);
            QVERIFY(file.open(QIODevice::WriteOnly));
            QCOMPARE(file.write("int main() {}\n"), qint64(14));
        }

        QTRY_VERIFY_WITH_TIMEOUT(eventSpy.count() >= 1, 2000);
        RawEvent created = qvariant_cast<RawEvent>(eventSpy.takeFirst().at(0));
        QCOMPARE(created.type, EventType::FileCreated);
        QCOMPARE(created.filePath, path);

        QTest::qWait(150);
        {
            QFile file(path);
            QVERIFY(file.open(QIODevice::Append));
            QCOMPARE(file.write("// changed\n"), qint64(11));
        }
        QTRY_VERIFY_WITH_TIMEOUT(eventSpy.count() >= 1, 2000);
        RawEvent modified = qvariant_cast<RawEvent>(eventSpy.takeFirst().at(0));
        QCOMPARE(modified.type, EventType::FileModified);

        QTest::qWait(150);
        QVERIFY(QFile::remove(path));
        QTRY_VERIFY_WITH_TIMEOUT(eventSpy.count() >= 1, 2000);
        RawEvent deleted = qvariant_cast<RawEvent>(eventSpy.takeFirst().at(0));
        QCOMPARE(deleted.type, EventType::FileDeleted);

        monitor.stop();
    }
};

QTEST_GUILESS_MAIN(TestFileSystemMonitor)
#include "test_file_system_monitor.moc"
