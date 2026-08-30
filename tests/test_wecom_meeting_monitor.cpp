#include <QtTest>

#include "monitor/WeComMeetingMonitor.h"

class TestWeComMeetingMonitor : public QObject {
    Q_OBJECT

private slots:
    void parsesAuthorizedCliStatus()
    {
        bool authorized = false;
        QString botId;
        QString error;
        QVERIFY2(WeComMeetingMonitor::parseAuthorizationResponse(
                     "Status: authorized\r\nBot ID: ww-daily-report\r\n",
                     &authorized, &botId, &error),
                 qPrintable(error));
        QVERIFY(authorized);
        QCOMPARE(botId, QString("ww-daily-report"));
    }

    void parsesUnauthorizedCliStatus()
    {
        bool authorized = true;
        QString botId = "stale";
        QString error;
        QVERIFY2(WeComMeetingMonitor::parseAuthorizationResponse(
                     "Status: unauthorized\n", &authorized, &botId, &error),
                 qPrintable(error));
        QVERIFY(!authorized);
        QVERIFY(botId.isEmpty());
    }

    void rejectsUnknownCliStatus()
    {
        bool authorized = true;
        QString error;
        QVERIFY(!WeComMeetingMonitor::parseAuthorizationResponse(
            "credentials maybe available", &authorized, nullptr, &error));
        QVERIFY(!authorized);
        QVERIFY(!error.isEmpty());
    }

    void parsesPaginatedMeetingList()
    {
        const QByteArray json = R"({
            "data": {
                "created_meetings": [
                    {"meeting_id":"created-1","sub_meeting_id":"sub-1"}
                ],
                "attended_meetings": [
                    {"meeting_id":"attended-1"}
                ],
                "has_more": true,
                "next_cursor": "cursor-2"
            }
        })";
        WeComMeetingListPage page;
        QString error;
        QVERIFY2(WeComMeetingMonitor::parseListResponse(json, &page, &error),
                 qPrintable(error));
        QCOMPARE(page.meetingIds.size(), 2);
        QCOMPARE(page.meetingIds.first().value("sub_meeting_id").toString(),
                 QString("sub-1"));
        QVERIFY(page.hasMore);
        QCOMPARE(page.nextCursor, QString("cursor-2"));
    }

    void importsOnlyActualAttendance()
    {
        const QByteArray json = R"({
            "meetings": [
                {
                    "meeting_id":"joined",
                    "subject":"Technical review",
                    "current_user_enter_time":"2026-08-29 09:00:00",
                    "current_user_quit_time":"2026-08-29 10:00:00",
                    "timezone":{"timezone_id":"Asia/Shanghai"}
                },
                {
                    "meeting_id":"invitation-only",
                    "subject":"Postponed meeting",
                    "current_user_enter_time":"",
                    "current_user_quit_time":"",
                    "timezone":{"timezone_id":"Asia/Shanghai"}
                }
            ]
        })";
        QList<WeComMeetingDetails> meetings;
        QString error;
        QVERIFY2(WeComMeetingMonitor::parseDetailsResponse(
                     json, &meetings, &error), qPrintable(error));
        QCOMPARE(meetings.size(), 1);
        QCOMPARE(meetings.first().meetingId, QString("joined"));
        QCOMPARE(meetings.first().enteredAt.secsTo(meetings.first().quitAt),
                 60 * 60);
    }

    void rejectsMalformedResponse()
    {
        WeComMeetingListPage page;
        QString error;
        QVERIFY(!WeComMeetingMonitor::parseListResponse(
            QByteArray("not json"), &page, &error));
        QVERIFY(!error.isEmpty());
    }
};

QTEST_MAIN(TestWeComMeetingMonitor)
#include "test_wecom_meeting_monitor.moc"
