#include "ReportGenerator.h"
#include "TemplateEngine.h"
#include "storage/EventRepository.h"
#include "storage/ReportRepository.h"
#include <QFutureInterface>
#include <spdlog/spdlog.h>

ReportGenerator::ReportGenerator(TemplateEngine *templateEngine,
                                 LlmClient *llmClient,
                                 EventRepository *eventRepo,
                                 ReportRepository *reportRepo,
                                 QObject *parent)
    : QObject(parent)
    , m_templateEngine(templateEngine)
    , m_llmClient(llmClient)
    , m_eventRepo(eventRepo)
    , m_reportRepo(reportRepo)
{
}

QFuture<ReportResult> ReportGenerator::generateDailyReport(const QDate &date)
{
    auto *fi = new QFutureInterface<ReportResult>();
    fi->reportStarted();

    QtConcurrent::run([this, fi, date]() {
        ReportResult result = doGenerate(date, "daily");
        fi->reportResult(result);
        fi->reportFinished();
        delete fi;
    });

    return fi->future();
}

QFuture<ReportResult> ReportGenerator::generateWeeklyReport(const QDate &date)
{
    auto *fi = new QFutureInterface<ReportResult>();
    fi->reportStarted();

    QtConcurrent::run([this, fi, date]() {
        ReportResult result = doGenerate(date, "weekly");
        fi->reportResult(result);
        fi->reportFinished();
        delete fi;
    });

    return fi->future();
}

QFuture<ReportResult> ReportGenerator::regenerateReport(const QDate &date, const QString &type)
{
    return type == "weekly" ? generateWeeklyReport(date) : generateDailyReport(date);
}

ReportResult ReportGenerator::doGenerate(const QDate &date, const QString &type)
{
    ReportResult result;
    result.reportDate = date;
    result.reportType = type;

    QElapsedTimer timer;
    timer.start();

    emit generationStarted(type, date);
    spdlog::info("ReportGenerator: Starting {} report for {}",
                 type.toStdString(), date.toString(Qt::ISODate).toStdString());

    try {
        Timeline timeline;
        QMap<QString, QString> context;

        if (type == "daily") {
            timeline = m_eventRepo->queryTimeline(date);
            ActivitySummary summary = timeline.computeSummary(date);
            context = TemplateEngine::buildReportContext(summary, timeline, date);
        } else {
            // Weekly: go back to the Monday of the given week
            QDate weekStart = date.addDays(-(date.dayOfWeek() - 1));
            QDate weekEnd = weekStart.addDays(6);
            timeline = m_eventRepo->queryDateRange(weekStart, weekEnd);

            QList<QPair<QDate, ActivitySummary>> dailySummaries;
            for (int i = 0; i < 7; ++i) {
                QDate d = weekStart.addDays(i);
                ActivitySummary s = timeline.forDate(d).computeSummary(d);
                dailySummaries.append({d, s});
            }
            context = TemplateEngine::buildWeeklyContext(dailySummaries, timeline, weekStart);

            // Use weekStart as the effective report date
            result.reportDate = weekStart;
        }

        // Render the template to get the user prompt
        QString templateName = type == "weekly" ? "weekly_report" : "daily_report";
        QString userPrompt = m_templateEngine->render(templateName, context);

        if (userPrompt.isEmpty()) {
            result.success = false;
            result.errorMessage = "Failed to render template.";
            emit generationFailed(result.errorMessage);
            return result;
        }

        // System prompt
        QString systemPrompt = QString(
            "You are a professional assistant that helps software developers "
            "write clear, concise, and informative work reports. "
            "Respond with well-formatted Markdown. "
            "Use the data provided to create an accurate report in the requested language."
        );

        // Call LLM (blocking wait on the async result)
        QFuture<QString> llmFuture = m_llmClient->generateReport(systemPrompt, userPrompt);

        // Wait for LLM response with timeout
        QEventLoop loop;
        QFutureWatcher<QString> watcher;
        QObject::connect(&watcher, &QFutureWatcher<QString>::finished, &loop, &QEventLoop::quit);
        watcher.setFuture(llmFuture);

        QTimer timeout;
        timeout.setSingleShot(true);
        QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
        timeout.start(180000); // 3 minute timeout

        loop.exec();

        if (!watcher.isFinished()) {
            result.success = false;
            result.errorMessage = "LLM generation timed out.";
            emit generationFailed(result.errorMessage);
            return result;
        }

        QString llmOutput = watcher.result();
        if (llmOutput.isEmpty() || llmOutput.startsWith("Error:")) {
            result.success = false;
            result.errorMessage = llmOutput;
            emit generationFailed(result.errorMessage);
            return result;
        }

        result.contentMd = llmOutput;
        result.success = true;
        result.generationTimeSecs = timer.elapsed() / 1000.0;
        result.llmBackend = m_llmClient->activeBackend();

        // Save to database
        QString title = type == "daily"
            ? QString("Daily Report - %1").arg(date.toString("yyyy-MM-dd"))
            : QString("Weekly Report - %1").arg(result.reportDate.toString("yyyy-MM-dd"));

        ILlmBackend *backend = m_llmClient->activeBackendPtr();
        QString model = backend ? "unknown" : "unknown";

        m_reportRepo->saveReport(type, result.reportDate, title, result.contentMd,
                                  result.llmBackend, model, result.generationTimeSecs, 0);

        spdlog::info("ReportGenerator: {} report generated in {:.1f}s",
                     type.toStdString(), result.generationTimeSecs);
        emit generationCompleted(result);

    } catch (const std::exception &e) {
        result.success = false;
        result.errorMessage = QString("Exception: %1").arg(e.what());
        spdlog::error("ReportGenerator: {}", result.errorMessage.toStdString());
        emit generationFailed(result.errorMessage);
    }

    return result;
}
