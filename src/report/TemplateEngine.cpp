#include "TemplateEngine.h"
#include "storage/Database.h"
#include "storage/SettingsRepository.h"
#include <QSqlQuery>
#include <QSqlError>
#include <spdlog/spdlog.h>

TemplateEngine::TemplateEngine(QObject *parent)
    : QObject(parent)
{
    // Register default templates
    registerTemplate("daily_report", defaultDailyTemplate(), "Default daily report template");
    registerTemplate("weekly_report", defaultWeeklyTemplate(), "Default weekly report template");
}

void TemplateEngine::registerTemplate(const QString &name, const QString &content,
                                       const QString &description)
{
    m_templates[name] = {content, description};
}

void TemplateEngine::loadFromDatabase()
{
    auto db = Database::instance().connection();
    QSqlQuery query(db);
    query.exec("SELECT name, description, content_md FROM report_templates");

    while (query.next()) {
        QString name = query.value("name").toString();
        QString desc = query.value("description").toString();
        QString content = query.value("content_md").toString();
        m_templates[name] = {content, desc};
    }
    spdlog::info("TemplateEngine: Loaded {} templates.", m_templates.size());
}

void TemplateEngine::saveToDatabase()
{
    auto db = Database::instance().connection();
    QSqlQuery query(db);

    for (auto it = m_templates.begin(); it != m_templates.end(); ++it) {
        query.prepare(
            "INSERT INTO report_templates (name, description, content_md) "
            "VALUES (:name, :desc, :content) "
            "ON CONFLICT(name) DO UPDATE SET description = :desc2, content_md = :content2"
        );
        query.bindValue(":name", it.key());
        query.bindValue(":desc", it.value().description);
        query.bindValue(":content", it.value().content);
        query.bindValue(":desc2", it.value().description);
        query.bindValue(":content2", it.value().content);

        if (!query.exec()) {
            spdlog::error("TemplateEngine: Failed to save template '{}': {}",
                          it.key().toStdString(), query.lastError().text().toStdString());
        }
    }
}

QStringList TemplateEngine::templateNames() const
{
    return m_templates.keys();
}

QString TemplateEngine::templateContent(const QString &name) const
{
    return m_templates.value(name).content;
}

QString TemplateEngine::templateDescription(const QString &name) const
{
    return m_templates.value(name).description;
}

QString TemplateEngine::render(const QString &templateName,
                                const QMap<QString, QString> &context) const
{
    if (!m_templates.contains(templateName)) {
        spdlog::warn("TemplateEngine: Template '{}' not found.", templateName.toStdString());
        return {};
    }
    PromptTemplate tpl(m_templates[templateName].content);
    return tpl.render(context);
}

QString TemplateEngine::renderVariant(const QString &templateName,
                                       const QVariantMap &context) const
{
    if (!m_templates.contains(templateName)) return {};
    PromptTemplate tpl(m_templates[templateName].content);
    return tpl.renderVariant(context);
}

QMap<QString, QString> TemplateEngine::buildReportContext(
    const ActivitySummary &summary, const Timeline &timeline, const QDate &date)
{
    QMap<QString, QString> ctx;

    ctx["date"] = date.toString("yyyy-MM-dd");
    ctx["weekday"] = date.toString("dddd");
    ctx["active_hours"] = QString::number(summary.totalActiveSecs / 3600.0, 'f', 1);
    ctx["active_minutes"] = QString::number(summary.totalActiveSecs / 60);
    ctx["idle_hours"] = QString::number(summary.totalIdleSecs / 3600.0, 'f', 1);
    ctx["file_edit_count"] = QString::number(summary.fileEditCount);
    ctx["git_commit_count"] = QString::number(summary.gitCommitCount);
    ctx["build_count"] = QString::number(summary.buildCount);
    ctx["build_failure_count"] = QString::number(summary.buildFailureCount);

    // Category breakdown
    QStringList categories;
    for (auto it = summary.categoryDurationSecs.begin(); it != summary.categoryDurationSecs.end(); ++it) {
        double hours = it.value() / 3600.0;
        int pct = summary.categoryPercent(it.key());
        categories.append(QString("  - %1: %2h (%3%)")
                              .arg(eventCategoryToString(it.key()))
                              .arg(hours, 0, 'f', 1)
                              .arg(pct));
    }
    ctx["category_breakdown"] = categories.join("\n");

    // Top files
    QStringList files;
    for (const auto &f : summary.topFiles) {
        QFileInfo fi(f);
        files.append(QString("  - %1").arg(fi.fileName()));
    }
    ctx["top_files"] = files.isEmpty() ? "  (none)" : files.join("\n");

    // Top applications
    QStringList apps;
    for (const auto &a : summary.topApplications)
        apps.append(QString("  - %1").arg(a));
    ctx["top_applications"] = apps.isEmpty() ? "  (none)" : apps.join("\n");

    // Build a simple timeline table
    QStringList timelineRows;
    const auto &events = timeline.events();
    for (int i = 0; i < qMin(events.size(), 50); ++i) {
        const auto &e = events[i];
        timelineRows.append(QString("| %1 | %2 | %3 | %4 |")
                                .arg(e.timestamp.toLocalTime().toString("HH:mm:ss"))
                                .arg(eventCategoryToString(e.category))
                                .arg(e.description.left(60))
                                .arg(e.durationSecs > 0 ? QString("%1s").arg(e.durationSecs) : "-"));
    }
    if (timelineRows.isEmpty())
        ctx["timeline_table"] = "(No events recorded)";
    else
        ctx["timeline_table"] = "| Time | Category | Description | Duration |\n"
                                "|------|----------|-------------|----------|\n" +
                                timelineRows.join("\n");

    return ctx;
}

QMap<QString, QString> TemplateEngine::buildWeeklyContext(
    const QList<QPair<QDate, ActivitySummary>> &dailySummaries,
    const Timeline &weekTimeline,
    const QDate &weekStart)
{
    Q_UNUSED(weekTimeline);
    QMap<QString, QString> ctx;

    ctx["week_start"] = weekStart.toString("yyyy-MM-dd");
    ctx["week_end"] = weekStart.addDays(6).toString("yyyy-MM-dd");

    int totalActive = 0, totalFiles = 0, totalCommits = 0, totalBuilds = 0, totalBuildFailures = 0;
    QStringList dailyLines;

    for (const auto &[date, summary] : dailySummaries) {
        totalActive += summary.totalActiveSecs;
        totalFiles += summary.fileEditCount;
        totalCommits += summary.gitCommitCount;
        totalBuilds += summary.buildCount;
        totalBuildFailures += summary.buildFailureCount;

        dailyLines.append(QString("| %1 | %2 | %3h | %4 files | %5 commits | %6 builds |")
                              .arg(date.toString("MM-dd"))
                              .arg(date.toString("dddd"))
                              .arg(summary.activeHours(), 0, 'f', 1)
                              .arg(summary.fileEditCount)
                              .arg(summary.gitCommitCount)
                              .arg(summary.buildCount));
    }

    ctx["total_active_hours"] = QString::number(totalActive / 3600.0, 'f', 1);
    ctx["total_file_edits"] = QString::number(totalFiles);
    ctx["total_git_commits"] = QString::number(totalCommits);
    ctx["total_builds"] = QString::number(totalBuilds);
    ctx["total_build_failures"] = QString::number(totalBuildFailures);

    ctx["daily_table"] = "| Date | Day | Active | Files | Commits | Builds |\n"
                         "|------|-----|--------|-------|---------|--------|\n" +
                         dailyLines.join("\n");

    return ctx;
}

QString TemplateEngine::defaultDailyTemplate()
{
    return R"(You are an assistant that generates daily work reports for software developers.

Based on the activity timeline below, write a concise, professional daily work report.
The report should be written in first person and cover the main activities of the day.
Group related activities together. Mention specific files worked on and any notable accomplishments.

**Report Date:** {{date}} ({{weekday}})
**Total Active Time:** {{active_hours}} hours
**Idle Time:** {{idle_hours}} hours

## Activity Summary

{{category_breakdown}}

## Key Metrics
- Files Edited: {{file_edit_count}}
- Git Commits: {{git_commit_count}}
- Builds: {{build_count}} ({{build_failure_count}} failures)

## Top Edited Files
{{top_files}}

## Top Applications
{{top_applications}}

## Activity Timeline
{{timeline_table}}

---

Please generate a natural-language daily report based on the above data. The report should include:
1. An overall summary of the day
2. Key accomplishments and work done
3. Time breakdown by activity category
4. Any issues or blockers encountered
5. Plan for tomorrow (if applicable)

Format the report in Markdown.
)";
}

QString TemplateEngine::defaultWeeklyTemplate()
{
    return R"(You are an assistant that generates weekly work reports for software developers.

Based on the activity data below, write a comprehensive weekly report.
The report should summarize the week's accomplishments, highlight key metrics, and identify trends.

**Week:** {{week_start}} to {{week_end}}
**Total Active Time:** {{total_active_hours}} hours

## Daily Breakdown

{{daily_table}}

## Weekly Totals
- Files Edited: {{total_file_edits}}
- Git Commits: {{total_git_commits}}
- Builds: {{total_builds}} ({{total_build_failures}} failures)

---

Please generate a natural-language weekly report. Include:
1. Weekly overview and key achievements
2. Day-by-day breakdown of significant work
3. Productivity trends and patterns
4. Areas of focus for next week
5. Any blockers or challenges faced

Format the report in Markdown.
)";
}
