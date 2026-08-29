#include <QtTest>
#include <QTemporaryDir>
#include "storage/Database.h"
#include "storage/EventRepository.h"
#include "storage/SettingsRepository.h"
#include "storage/ReportRepository.h"
#include "app/DataRetentionService.h"
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

    void testRetentionDefaults()
    {
        SettingsRepository settings;
        QCOMPARE(settings.getInt("activity_retention_months"), 3);
        QCOMPARE(settings.getInt("report_retention_months"), 3);
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
        const ActivityEvent stored = timeline.events().last();
        QCOMPARE(stored.type, EventType::FileModified);
        QCOMPARE(stored.category, EventCategory::Coding);
        QCOMPARE(stored.filePath, QString("/test/file.cpp"));
    }

    void testEventPruning()
    {
        EventRepository repo;
        int deleted = repo.pruneOlderThan(QDate::currentDate().addDays(365));
        QVERIFY(deleted >= 0); // Should not crash
    }

    void testRetentionServiceCleansBothStores()
    {
        SettingsRepository settings;
        settings.setInt("activity_retention_months", 3);
        settings.setInt("report_retention_months", 3);

        EventRepository events;
        ActivityEvent oldEvent;
        oldEvent.timestamp = QDateTime(
            QDate::currentDate().addMonths(-6), QTime(12, 0), Qt::UTC);
        oldEvent.type = EventType::FileModified;
        oldEvent.category = EventCategory::Coding;
        oldEvent.description = "Expired event";
        QVERIFY(events.insertBatch({oldEvent}));

        ReportRepository reports;
        const QDate oldDate = QDate::currentDate().addMonths(-6).addDays(-1);
        QVERIFY(reports.saveReport("weekly", oldDate, "Expired report", "old",
                                   "test", "test") > 0);

        DataRetentionService service(&events, &reports, &settings);
        QSignalSpy cleanupSpy(&service, &DataRetentionService::cleanupFinished);
        service.runCleanupNow();

        QCOMPARE(cleanupSpy.count(), 1);
        const auto arguments = cleanupSpy.takeFirst();
        QVERIFY(arguments.at(0).toInt() >= 1);
        QVERIFY(arguments.at(1).toInt() >= 1);
        QCOMPARE(arguments.at(5).toBool(), true);
        QVERIFY(!reports.reportExists(oldDate, "weekly"));
    }

    void testReportPruning()
    {
        ReportRepository repo;
        const QDate oldDate = QDate::currentDate().addMonths(-6);
        const QDate recentDate = QDate::currentDate().addMonths(-1);

        QVERIFY(repo.saveReport("daily", oldDate, "Old report", "old",
                                "test", "test") > 0);
        QVERIFY(repo.saveReport("daily", recentDate, "Recent report", "recent",
                                "test", "test") > 0);

        const int deleted = repo.pruneOlderThan(
            QDate::currentDate().addMonths(-3));
        QCOMPARE(deleted, 1);
        QVERIFY(!repo.reportExists(oldDate, "daily"));
        QVERIFY(repo.reportExists(recentDate, "daily"));
    }

private:
    QTemporaryDir *m_tempDir = nullptr;
};

QTEST_MAIN(TestDatabase)
#include "test_database.moc"
