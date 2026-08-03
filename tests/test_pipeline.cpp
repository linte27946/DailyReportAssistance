#include <QtTest>
#include "core/Event.h"
#include "core/Timeline.h"
#include "pipeline/EventFilter.h"
#include "pipeline/TimelineAssembler.h"

class TestPipeline : public QObject {
    Q_OBJECT

private slots:
    void testEventFilterDedup()
    {
        EventFilter filter;
        filter.setDedupWindowMs(5000);

        RawEvent e1;
        e1.timestamp = QDateTime::currentDateTimeUtc();
        e1.type = EventType::FileModified;
        e1.filePath = "C:/project/main.cpp";
        e1.processName = "code.exe";

        RawEvent e2 = e1; // Identical event
        e2.timestamp = e1.timestamp.addMSecs(1000);

        QList<RawEvent> input{e1, e2};
        QList<RawEvent> filtered = filter.filter(input);

        // Second event should be filtered as duplicate
        QCOMPARE(filtered.size(), 1);
    }

    void testEventFilterAllowsDifferentFiles()
    {
        EventFilter filter;
        filter.setDedupWindowMs(5000);

        RawEvent e1;
        e1.timestamp = QDateTime::currentDateTimeUtc();
        e1.type = EventType::FileModified;
        e1.filePath = "C:/project/main.cpp";

        RawEvent e2;
        e2.timestamp = QDateTime::currentDateTimeUtc();
        e2.type = EventType::FileModified;
        e2.filePath = "C:/project/utils.h";

        QList<RawEvent> input{e1, e2};
        QList<RawEvent> filtered = filter.filter(input);

        // Both files are different, both should pass
        QCOMPARE(filtered.size(), 2);
    }

    void testTimelineCoalesce()
    {
        Timeline timeline;

        // Two coding events close together should coalesce
        ActivityEvent e1;
        e1.timestamp = QDateTime::currentDateTimeUtc();
        e1.endTimestamp = e1.timestamp.addSecs(60);
        e1.category = EventCategory::Coding;
        e1.description = "Editing main.cpp";
        timeline.addEvent(e1);

        ActivityEvent e2;
        e2.timestamp = e1.endTimestamp.addSecs(30); // 30s gap
        e2.endTimestamp = e2.timestamp.addSecs(120);
        e2.category = EventCategory::Coding;
        e2.description = "Editing utils.h";
        timeline.addEvent(e2);

        // Different category — should NOT coalesce
        ActivityEvent e3;
        e3.timestamp = e2.endTimestamp.addSecs(60);
        e3.endTimestamp = e3.timestamp.addSecs(30);
        e3.category = EventCategory::Building;
        e3.description = "Building project";
        timeline.addEvent(e3);

        Timeline coalesced = TimelineAssembler::coalesce(timeline, 300);

        // e1 and e2 coalesce into one, e3 stays separate
        QCOMPARE(coalesced.count(), 2);
    }

    void testTimelineSummary()
    {
        Timeline timeline;
        QDate today = QDate::currentDate();

        ActivityEvent e1;
        e1.timestamp = QDateTime(today, QTime(9, 0, 0), Qt::UTC);
        e1.category = EventCategory::Coding;
        e1.durationSecs = 3600; // 1 hour
        timeline.addEvent(e1);

        ActivityEvent e2;
        e2.timestamp = QDateTime(today, QTime(10, 0, 0), Qt::UTC);
        e2.type = EventType::GitCommit;
        e2.category = EventCategory::VersionControl;
        e2.durationSecs = 300;
        timeline.addEvent(e2);

        ActivitySummary summary = timeline.computeSummary(today);
        QCOMPARE(summary.totalActiveSecs, 3900);
        QCOMPARE(summary.gitCommitCount, 1);
    }
};

QTEST_MAIN(TestPipeline)
#include "test_pipeline.moc"
