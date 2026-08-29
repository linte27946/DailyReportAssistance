#include <QtTest>
#include <QSignalSpy>

#include "monitor/BrowserUrlMonitor.h"
#include "monitor/WorkContextMonitor.h"
#include "llm/LlmConfig.h"

class TestWorkContext : public QObject {
    Q_OBJECT

private slots:
    void extractsEditorAndDocumentMetadataWithoutContents()
    {
        WorkContextMonitor monitor;
        QVERIFY(monitor.start());
        QSignalSpy spy(&monitor, &IMonitor::rawEventCaptured);

        RawEvent editor;
        editor.timestamp = QDateTime::currentDateTimeUtc();
        editor.type = EventType::WindowFocusChanged;
        editor.processName = "Code.exe";
        editor.windowTitle = "Dashboard.tsx - customer-portal - Visual Studio Code";
        monitor.processWindowEvent(editor);

        QCOMPARE(spy.count(), 1);
        RawEvent captured = qvariant_cast<RawEvent>(spy.takeFirst().at(0));
        QCOMPARE(captured.type, EventType::EditorContextChanged);
        QCOMPARE(captured.metadata["fileName"].toString(), QString("Dashboard.tsx"));
        QCOMPARE(captured.metadata["workspace"].toString(), QString("customer-portal"));
        QCOMPARE(captured.metadata["contentCaptured"].toBool(), false);

        RawEvent document;
        document.timestamp = editor.timestamp.addSecs(10);
        document.type = EventType::WindowFocusChanged;
        document.processName = "AcroRd32.exe";
        document.windowTitle = "distributed-systems.pdf - Adobe Acrobat Reader";
        monitor.processWindowEvent(document);

        QCOMPARE(spy.count(), 1);
        captured = qvariant_cast<RawEvent>(spy.takeFirst().at(0));
        QCOMPARE(captured.type, EventType::DocumentViewed);
        QCOMPARE(captured.metadata["documentName"].toString(),
                 QString("distributed-systems.pdf"));
        QCOMPARE(captured.metadata["contentCaptured"].toBool(), false);
    }

    void stripsSensitiveUrlPartsByDefault()
    {
        const QString source =
            "https://user:pass@example.com/docs/topic?q=private#section";
        QCOMPARE(BrowserUrlMonitor::sanitizeUrl(source),
                 QString("https://example.com/docs/topic"));
        QCOMPARE(BrowserUrlMonitor::sanitizeUrl(source, true),
                 QString("https://example.com/docs/topic?q=private"));
        QVERIFY(BrowserUrlMonitor::sanitizeUrl("file:///private/report.pdf").isEmpty());
    }

    void usesCurrentDeepSeekDefaults()
    {
        const LlmConfig config = LlmConfig::deepSeekDefault();
        QCOMPARE(config.endpoint, QString("https://api.deepseek.com/chat/completions"));
        QCOMPARE(config.model, QString("deepseek-v4-flash"));
    }
};

QTEST_MAIN(TestWorkContext)
#include "test_work_context.moc"
