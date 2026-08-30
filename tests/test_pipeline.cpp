#include <QtTest>
#include "core/Event.h"
#include "core/Timeline.h"
#include "pipeline/EventFilter.h"
#include "pipeline/TimelineAssembler.h"
#include "pipeline/EventPipeline.h"

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

    void testPipelineEmitsPersistableEvents()
    {
        EventPipeline pipeline;
        pipeline.setSessionId("test-session");
        QSignalSpy processedSpy(&pipeline, &EventPipeline::eventsProcessed);
        QVERIFY(pipeline.start());

        RawEvent event;
        event.timestamp = QDateTime::currentDateTimeUtc();
        event.type = EventType::FileModified;
        event.source = "test";
        event.filePath = "/tmp/project/main.cpp";
        event.description = "Modified main.cpp";
        pipeline.onRawEvent(event);

        QTRY_COMPARE_WITH_TIMEOUT(processedSpy.count(), 1, 2500);
        const QList<ActivityEvent> events =
            qvariant_cast<QList<ActivityEvent>>(processedSpy.takeFirst().at(0));
        QCOMPARE(events.size(), 1);
        QCOMPARE(events.first().category, EventCategory::Coding);
        QCOMPARE(events.first().sessionId, QString("test-session"));
        pipeline.stop();
    }

    void testTimelineSummary()
    {
        Timeline timeline;
        const QDate today = QDate::currentDate().addDays(-1);
        const QDateTime start = QDateTime(
            today, QTime(9, 0), Qt::LocalTime).toUTC();
        const QDateTime end = start.addSecs(3600);

        ActivityEvent sessionStart;
        sessionStart.timestamp = start;
        sessionStart.type = EventType::SessionStarted;
        sessionStart.sessionId = "work-session";
        timeline.addEvent(sessionStart);

        ActivityEvent active = sessionStart;
        active.type = EventType::UserActive;
        timeline.addEvent(active);

        ActivityEvent focus = sessionStart;
        focus.type = EventType::WindowFocusChanged;
        focus.category = EventCategory::Coding;
        timeline.addEvent(focus);

        ActivityEvent commit = sessionStart;
        commit.timestamp = start.addSecs(1800);
        commit.type = EventType::GitCommit;
        commit.category = EventCategory::VersionControl;
        commit.durationSecs = 300;
        timeline.addEvent(commit);

        // Parallel processes all ran during the same real hour. Their summed
        // runtime must not inflate the wall-clock active time.
        for (int i = 0; i < 13; ++i) {
            ActivityEvent process = sessionStart;
            process.timestamp = end.addSecs(-i);
            process.type = EventType::ProcessEnded;
            process.category = EventCategory::Other;
            process.durationSecs = 3600;
            timeline.addEvent(process);
        }

        ActivityEvent sessionEnd = sessionStart;
        sessionEnd.timestamp = end;
        sessionEnd.type = EventType::SessionEnded;
        timeline.addEvent(sessionEnd);

        ActivitySummary summary = timeline.computeSummary(today);
        QCOMPARE(summary.totalActiveSecs, 3600);
        QCOMPARE(summary.categoryDurationSecs.value(EventCategory::Coding), 3600);
        QCOMPARE(summary.gitCommitCount, 1);
    }

    void testTimelineSummarySubtractsIdleIntervals()
    {
        Timeline timeline;
        const QDate today = QDate::currentDate().addDays(-1);
        const QDateTime start = QDateTime(
            today, QTime(9, 0), Qt::LocalTime).toUTC();

        auto add = [&](EventType type, int offsetSecs) -> ActivityEvent & {
            ActivityEvent event;
            event.timestamp = start.addSecs(offsetSecs);
            event.type = type;
            event.sessionId = "idle-session";
            timeline.addEvent(event);
            return timeline.events().last();
        };

        add(EventType::SessionStarted, 0);
        add(EventType::UserActive, 0);
        ActivityEvent &focus = add(EventType::WindowFocusChanged, 0);
        focus.category = EventCategory::Coding;

        ActivityEvent &idle = add(EventType::UserIdle, 25 * 60);
        idle.metadata["idleStartTime"] = start.addSecs(20 * 60)
            .toString(Qt::ISODateWithMs);
        ActivityEvent &resumed = add(EventType::UserActive, 30 * 60);
        resumed.metadata["idleStartTime"] = start.addSecs(20 * 60)
            .toString(Qt::ISODateWithMs);
        add(EventType::SessionEnded, 60 * 60);

        const ActivitySummary summary = timeline.computeSummary(today);
        QCOMPARE(summary.totalActiveSecs, 50 * 60);
        QCOMPARE(summary.totalIdleSecs, 10 * 60);
        QCOMPARE(summary.categoryDurationSecs.value(EventCategory::Coding), 50 * 60);
    }

    void testBrowserPageCategorySplitsForegroundTime()
    {
        Timeline timeline;
        const QDate date = QDate::currentDate().addDays(-1);
        const QDateTime start = QDateTime(
            date, QTime(9, 0), Qt::LocalTime).toUTC();

        auto add = [&](EventType type, int offsetSecs,
                       EventCategory category = EventCategory::Other) {
            ActivityEvent event;
            event.timestamp = start.addSecs(offsetSecs);
            event.type = type;
            event.category = category;
            event.sessionId = "browser-session";
            timeline.addEvent(event);
        };

        add(EventType::SessionStarted, 0);
        add(EventType::UserActive, 0);
        add(EventType::WindowFocusChanged, 0, EventCategory::Browsing);
        add(EventType::UrlVisited, 15 * 60, EventCategory::Distraction);
        add(EventType::WindowFocusChanged, 45 * 60, EventCategory::Coding);
        add(EventType::SessionEnded, 60 * 60);

        const ActivitySummary summary = timeline.computeSummary(date);
        QCOMPARE(summary.totalActiveSecs, 60 * 60);
        QCOMPARE(summary.categoryDurationSecs.value(EventCategory::Browsing),
                 15 * 60);
        QCOMPARE(summary.categoryDurationSecs.value(EventCategory::Distraction),
                 30 * 60);
        QCOMPARE(summary.categoryDurationSecs.value(EventCategory::Coding),
                 15 * 60);
    }

    void testMeetingFillsOnlyIdleWhenRatioIsGreaterThanThirtyPercent()
    {
        Timeline timeline;
        const QDate date = QDate::currentDate().addDays(-1);
        const QDateTime start = QDateTime(
            date, QTime(9, 0), Qt::LocalTime).toUTC();

        auto add = [&](EventType type, int offset,
                       EventCategory category = EventCategory::Other) {
            ActivityEvent event;
            event.timestamp = start.addSecs(offset);
            event.type = type;
            event.category = category;
            event.sessionId = "meeting-session";
            timeline.addEvent(event);
        };
        add(EventType::SessionStarted, 0);
        add(EventType::UserActive, 0);
        add(EventType::WindowFocusChanged, 0, EventCategory::Coding);
        add(EventType::UserIdle, 35 * 60, EventCategory::Idle);
        add(EventType::SessionEnded, 60 * 60);

        ActivityEvent meeting;
        meeting.timestamp = start;
        meeting.endTimestamp = start.addSecs(60 * 60);
        meeting.durationSecs = 60 * 60;
        meeting.type = EventType::MeetingAttended;
        meeting.category = EventCategory::Meeting;
        meeting.metadata["subject"] = "Architecture review";
        meeting.metadata["idleThresholdPercent"] = 30;
        timeline.addEvent(meeting);

        const ActivitySummary summary = timeline.computeSummary(date);
        QCOMPARE(summary.totalActiveSecs, 60 * 60);
        QCOMPARE(summary.totalIdleSecs, 0);
        QCOMPARE(summary.categoryDurationSecs.value(EventCategory::Coding),
                 35 * 60);
        QCOMPARE(summary.categoryDurationSecs.value(EventCategory::Meeting),
                 25 * 60);
        QCOMPARE(summary.meetingDurationSecs, 25 * 60);
        QCOMPARE(summary.meetingCount, 1);
    }

    void testMeetingDoesNotApplyAtExactlyThirtyPercentIdle()
    {
        Timeline timeline;
        const QDate date = QDate::currentDate().addDays(-1);
        const QDateTime start = QDateTime(
            date, QTime(10, 0), Qt::LocalTime).toUTC();

        auto add = [&](EventType type, int offset,
                       EventCategory category = EventCategory::Other) {
            ActivityEvent event;
            event.timestamp = start.addSecs(offset);
            event.type = type;
            event.category = category;
            event.sessionId = "boundary-session";
            timeline.addEvent(event);
        };
        add(EventType::SessionStarted, 0);
        add(EventType::UserActive, 0);
        add(EventType::WindowFocusChanged, 0, EventCategory::Coding);
        add(EventType::UserIdle, 42 * 60, EventCategory::Idle);
        add(EventType::SessionEnded, 60 * 60);

        ActivityEvent meeting;
        meeting.timestamp = start;
        meeting.endTimestamp = start.addSecs(60 * 60);
        meeting.type = EventType::MeetingAttended;
        meeting.category = EventCategory::Meeting;
        meeting.metadata["subject"] = "Scheduled review";
        meeting.metadata["idleThresholdPercent"] = 30;
        timeline.addEvent(meeting);

        const ActivitySummary summary = timeline.computeSummary(date);
        QCOMPARE(summary.totalActiveSecs, 42 * 60);
        QCOMPARE(summary.totalIdleSecs, 18 * 60);
        QCOMPARE(summary.meetingDurationSecs, 0);
        QCOMPARE(summary.meetingCount, 0);
        QCOMPARE(summary.categoryDurationSecs.value(EventCategory::Meeting), 0);
    }
};

QTEST_MAIN(TestPipeline)
#include "test_pipeline.moc"
