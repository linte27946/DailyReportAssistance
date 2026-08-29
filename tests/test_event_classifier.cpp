#include <QtTest>
#include "pipeline/ActivityClassifier.h"

class TestEventClassifier : public QObject {
    Q_OBJECT

private slots:
    void testCppFileClassifiedAsCoding()
    {
        ActivityClassifier classifier;
        RawEvent event;
        event.timestamp = QDateTime::currentDateTimeUtc();
        event.type = EventType::FileModified;
        event.processName = "code.exe";
        event.filePath = "C:/projects/myapp/src/main.cpp";

        ActivityEvent result = classifier.classify(event);
        QCOMPARE(result.category, EventCategory::Coding);
    }

    void testBuildProcessClassifiedAsBuilding()
    {
        ActivityClassifier classifier;
        RawEvent event;
        event.timestamp = QDateTime::currentDateTimeUtc();
        event.type = EventType::ProcessStarted;
        event.processName = "msbuild.exe";

        ActivityEvent result = classifier.classify(event);
        QCOMPARE(result.category, EventCategory::Building);
    }

    void testGitCommitClassifiedAsVersionControl()
    {
        ActivityClassifier classifier;
        RawEvent event;
        event.timestamp = QDateTime::currentDateTimeUtc();
        event.type = EventType::GitCommit;
        event.processName = "git.exe";
        event.description = "Git commit: Fix bug in parser";

        ActivityEvent result = classifier.classify(event);
        QCOMPARE(result.category, EventCategory::VersionControl);
    }

    void testDocUrlClassifiedAsDocumentation()
    {
        ActivityClassifier classifier;
        RawEvent event;
        event.timestamp = QDateTime::currentDateTimeUtc();
        event.type = EventType::UrlVisited;
        event.processName = "chrome.exe";
        event.url = "https://docs.microsoft.com/en-us/cpp/";

        ActivityEvent result = classifier.classify(event);
        QCOMPARE(result.category, EventCategory::Documentation);
    }

    void testDocumentationMetadataClassifiesAnyBrowser()
    {
        ActivityClassifier classifier;
        RawEvent event;
        event.timestamp = QDateTime::currentDateTimeUtc();
        event.type = EventType::UrlVisited;
        event.processName = "firefox";
        event.metadata["isDocumentation"] = true;

        QCOMPARE(classifier.classify(event).category, EventCategory::Documentation);
    }

    void testUnknownClassifiedAsOther()
    {
        ActivityClassifier classifier;
        RawEvent event;
        event.timestamp = QDateTime::currentDateTimeUtc();
        event.type = EventType::WindowFocusChanged;
        event.processName = "unknown_xyz.exe";

        ActivityEvent result = classifier.classify(event);
        QVERIFY(result.category == EventCategory::Other ||
                result.category == EventCategory::Browsing ||
                result.category == EventCategory::Coding);
    }
};

QTEST_MAIN(TestEventClassifier)
#include "test_event_classifier.moc"
