#include "ReportGenerator.h"
#include "TemplateEngine.h"
#include "storage/EventRepository.h"
#include "storage/ReportRepository.h"
#include "storage/Database.h"
#include <QFutureInterface>
#include <QTimer>
#include <QThread>
#include <QtConcurrent/QtConcurrent>
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

ReportGenerator::~ReportGenerator()
{
    m_futureSynchronizer.waitForFinished();
}

void ReportGenerator::setReportLanguage(const QString &language)
{
    QMutexLocker locker(&m_configMutex);
    m_reportLanguage = language;
}

bool ReportGenerator::reportExists(const QDate &date, const QString &type) const
{
    const QDate effectiveDate = type == "weekly"
        ? date.addDays(-(date.dayOfWeek() - 1)) : date;
    return m_reportRepo->reportExists(effectiveDate, type);
}

QFuture<ReportResult> ReportGenerator::generateDailyReport(const QDate &date)
{
    auto *fi = new QFutureInterface<ReportResult>();
    fi->reportStarted();
    const QFuture<ReportResult> future = fi->future();
    m_futureSynchronizer.addFuture(future);

    (void)QtConcurrent::run([this, fi, date]() {
        ReportResult result = doGenerate(date, "daily");
        Database::instance().closeConnection();
        fi->reportResult(result);
        fi->reportFinished();
        delete fi;
    });

    return future;
}

QFuture<ReportResult> ReportGenerator::generateWeeklyReport(const QDate &date)
{
    auto *fi = new QFutureInterface<ReportResult>();
    fi->reportStarted();
    const QFuture<ReportResult> future = fi->future();
    m_futureSynchronizer.addFuture(future);

    (void)QtConcurrent::run([this, fi, date]() {
        ReportResult result = doGenerate(date, "weekly");
        Database::instance().closeConnection();
        fi->reportResult(result);
        fi->reportFinished();
        delete fi;
    });

    return future;
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
        QString userPrompt;
        auto renderTemplate = [&]() {
            userPrompt = m_templateEngine->render(templateName, context);
        };
        if (QThread::currentThread() == m_templateEngine->thread()) {
            renderTemplate();
        } else {
            QMetaObject::invokeMethod(m_templateEngine, renderTemplate,
                                      Qt::BlockingQueuedConnection);
        }

        if (userPrompt.isEmpty()) {
            result.success = false;
            result.errorMessage = "Failed to render template.";
            emit generationFailed(result.errorMessage);
            return result;
        }

        // System prompt
        QString reportLanguage;
        {
            QMutexLocker locker(&m_configMutex);
            reportLanguage = m_reportLanguage;
        }
        const QString languageName = reportLanguage == "en"
            ? "English" : reportLanguage == "ja-JP" ? "Japanese" : "Simplified Chinese";
        QString systemPrompt = QString(
            "You are a professional assistant that helps software developers "
            "write clear, concise, and informative work reports. "
            "Respond with well-formatted Markdown. "
            "Use the data provided to create an accurate report. "
            "Write the final report in %1."
        ).arg(languageName);

        // Call LLM (blocking wait on the async result)
        QFuture<QString> llmFuture;
        QString requestBackend;
        QString requestModel;
        auto startRequest = [&]() {
            requestBackend = m_llmClient->activeBackend();
            ILlmBackend *backend = m_llmClient->activeBackendPtr();
            requestModel = backend ? backend->model() : QString();
            llmFuture = m_llmClient->generateReport(systemPrompt, userPrompt);
        };
        if (QThread::currentThread() == m_llmClient->thread()) {
            startRequest();
        } else {
            QMetaObject::invokeMethod(m_llmClient, startRequest,
                                      Qt::BlockingQueuedConnection);
        }

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
        result.llmBackend = requestBackend;

        // Save to database
        QString title = type == "daily"
            ? QString("Daily Report - %1").arg(date.toString("yyyy-MM-dd"))
            : QString("Weekly Report - %1").arg(result.reportDate.toString("yyyy-MM-dd"));

        result.llmModel = requestModel;

        const int64_t reportId = m_reportRepo->saveReport(
            type, result.reportDate, title, result.contentMd,
            result.llmBackend, result.llmModel, result.generationTimeSecs, 0);
        if (reportId < 0) {
            result.success = false;
            result.errorMessage = "The report was generated but could not be saved.";
            emit generationFailed(result.errorMessage);
            return result;
        }

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
