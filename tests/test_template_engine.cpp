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
};

QTEST_MAIN(TestTemplateEngine)
#include "test_template_engine.moc"
