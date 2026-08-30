#include <QtTest>
#include "llm/PromptTemplate.h"
#include "report/TemplateEngine.h"

class TestTemplateEngine : public QObject {
    Q_OBJECT

private slots:
    void testSimplePlaceholderReplacement()
    {
        PromptTemplate tpl("Hello, {{name}}! Today is {{day}}.");

        QMap<QString, QString> context;
        context["name"] = "Developer";
        context["day"] = "Monday";

        QString result = tpl.render(context);
        QCOMPARE(result, "Hello, Developer! Today is Monday.");
    }

    void testMissingPlaceholder()
    {
        PromptTemplate tpl("Hello, {{name}}!");

        QMap<QString, QString> context;
        // name not provided

        QString result = tpl.render(context);
        QCOMPARE(result, "Hello, {{name}}!"); // Should preserve original
    }

    void testMultipleOccurrences()
    {
        PromptTemplate tpl("{{x}} + {{x}} = 2*{{x}}");

        QMap<QString, QString> context;
        context["x"] = "5";

        QString result = tpl.render(context);
        QCOMPARE(result, "5 + 5 = 2*5");
    }

    void testDefaultTemplatesExist()
    {
        TemplateEngine engine;
        QStringList names = engine.templateNames();
        QVERIFY(names.contains("daily_report"));
        QVERIFY(names.contains("weekly_report"));

        QString daily = engine.templateContent("daily_report");
        QVERIFY(!daily.isEmpty());
        QVERIFY(daily.contains("{{date}}"));
        QVERIFY(daily.contains("{{active_hours}}"));
        QVERIFY(daily.contains("{{distraction_context}}"));
        QVERIFY(daily.contains("de-duplicated wall-clock"));

        QString weekly = engine.templateContent("weekly_report");
        QVERIFY(!weekly.isEmpty());
        QVERIFY(weekly.contains("{{week_start}}"));
    }

    void testBuildReportContext()
    {
        ActivitySummary summary;
        summary.date = QDate(2025, 1, 15);
        summary.totalActiveSecs = 18000; // 5 hours
        summary.fileEditCount = 42;
        summary.gitCommitCount = 5;
        summary.categoryDurationSecs[EventCategory::Coding] = 10800;
        summary.categoryDurationSecs[EventCategory::Documentation] = 3600;
        summary.topFiles << "/proj/main.cpp" << "/proj/utils.h";

        Timeline timeline;
        QMap<QString, QString> context = TemplateEngine::buildReportContext(
            summary, timeline, summary.date);

        QCOMPARE(context["date"], "2025-01-15");
        QCOMPARE(context["active_hours"], "5.0");
        QCOMPARE(context["file_edit_count"], "42");
        QCOMPARE(context["git_commit_count"], "5");
    }

    void testDeveloperAndResearchContextAreStructured()
    {
        ActivitySummary summary;
        summary.date = QDate(2026, 8, 29);
        Timeline timeline;

        ActivityEvent editor;
        editor.timestamp = QDateTime(summary.date, QTime(9, 15), Qt::UTC);
        editor.type = EventType::EditorContextChanged;
        editor.category = EventCategory::Coding;
        editor.description = "Editing Dashboard.tsx in project customer-portal with Visual Studio Code";
        timeline.addEvent(editor);

        ActivityEvent page;
        page.timestamp = QDateTime(summary.date, QTime(10, 30), Qt::UTC);
        page.type = EventType::UrlVisited;
        page.category = EventCategory::Documentation;
        page.description = "Browser page: CSS grid guide (developer.mozilla.org)";
        page.metadata["pageTitle"] = "CSS grid guide";
        page.metadata["domain"] = "developer.mozilla.org";
        timeline.addEvent(page);

        ActivityEvent document;
        document.timestamp = QDateTime(summary.date, QTime(11, 0), Qt::UTC);
        document.type = EventType::DocumentViewed;
        document.category = EventCategory::Documentation;
        document.description = "Reference document: frontend-architecture.pdf";
        timeline.addEvent(document);

        const auto context = TemplateEngine::buildReportContext(
            summary, timeline, summary.date);
        QVERIFY(context["editor_contexts"].contains("Dashboard.tsx"));
        QVERIFY(context["web_pages"].contains("developer.mozilla.org"));
        QVERIFY(context["documents_viewed"].contains("frontend-architecture.pdf"));
        QVERIFY(context["research_context"].contains("CSS grid guide"));
    }

    void testEntertainmentContextIsSeparateFromResearch()
    {
        ActivitySummary summary;
        summary.date = QDate(2026, 8, 30);
        Timeline timeline;

        ActivityEvent page;
        page.timestamp = QDateTime(summary.date, QTime(10, 30), Qt::UTC);
        page.type = EventType::UrlVisited;
        page.category = EventCategory::Distraction;
        page.description = "Entertainment page: 游戏直播 (live.bilibili.com)";
        page.metadata["pageTitle"] = "游戏直播";
        page.metadata["domain"] = "live.bilibili.com";
        timeline.addEvent(page);

        const auto context = TemplateEngine::buildReportContext(
            summary, timeline, summary.date);
        QVERIFY(context["distraction_context"].contains("游戏直播"));
        QVERIFY(!context["research_context"].contains("游戏直播"));
        QVERIFY(!context["development_context"].contains("游戏直播"));
        QVERIFY(!context["web_pages"].contains("游戏直播"));
    }
};

QTEST_MAIN(TestTemplateEngine)
#include "test_template_engine.moc"
