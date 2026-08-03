#include <QtTest>
#include <QTemporaryDir>
#include "storage/Database.h"
#include "storage/EventRepository.h"
#include "storage/SettingsRepository.h"
#include "core/Event.h"

class TestDatabase : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        // Use temporary directory for test database
        m_tempDir = new QTemporaryDir();
        QString dbPath = m_tempDir->path() + "/test.db";
        QVERIFY(Database::instance().initialize(dbPath));
    }

    void cleanupTestCase()
    {
        Database::instance().closeConnection();
        delete m_tempDir;
    }

    void testSettingsReadWrite()
    {
        SettingsRepository settings;
        QVERIFY(settings.setValue("test_key", "test_value"));
        QCOMPARE(settings.getValue("test_key"), "test_value");
        QCOMPARE(settings.getValue("nonexistent", "default"), "default");
    }

    void testSettingsBool()
    {
        SettingsRepository settings;
        QVERIFY(settings.setBool("bool_key", true));
        QVERIFY(settings.getBool("bool_key"));
        QVERIFY(!settings.getBool("nonexistent"));
    }

    void testEventInsertAndQuery()
    {
        EventRepository repo;

        ActivityEvent e;
        e.timestamp = QDateTime::currentDateTimeUtc();
        e.type = EventType::FileModified;
        e.category = EventCategory::Coding;
        e.description = "Test event";
        e.application = "test.exe";
        e.filePath = "/test/file.cpp";
        e.fileExtension = ".cpp";
        e.durationSecs = 60;

        QList<ActivityEvent> events{e};
        QVERIFY(repo.insertBatch(events));

        Timeline timeline = repo.queryTimeline(QDate::currentDate());
        QVERIFY(timeline.count() > 0);
    }

    void testEventPruning()
    {
        EventRepository repo;
        int deleted = repo.pruneOlderThan(QDate::currentDate().addDays(365));
        QVERIFY(deleted >= 0); // Should not crash
    }

private:
    QTemporaryDir *m_tempDir = nullptr;
};

QTEST_MAIN(TestDatabase)
#include "test_database.moc"
