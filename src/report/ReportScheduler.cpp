#include "ReportScheduler.h"
#include "ReportGenerator.h"
#include <QDateTime>
#include <spdlog/spdlog.h>

ReportScheduler::ReportScheduler(ReportGenerator *generator, QObject *parent)
    : QObject(parent)
    , m_generator(generator)
{
    m_scheduleTimer = new QTimer(this);
    m_scheduleTimer->setInterval(60000);  // Check every minute
    connect(m_scheduleTimer, &QTimer::timeout, this, &ReportScheduler::checkSchedule);
}

void ReportScheduler::setEnabled(bool enabled)
{
    if (enabled == m_enabled) {
        if (enabled && !m_scheduleTimer->isActive())
            m_scheduleTimer->start();
        return;
    }
    m_enabled = enabled;
    if (enabled) {
        spdlog::info("ReportScheduler: Enabled. Daily at {}, Weekly on day {} at {}",
                     m_dailyTime.toString("HH:mm").toStdString(),
                     m_weeklyDay, m_weeklyTime.toString("HH:mm").toStdString());
        m_lastDailyDate = QDate();  // Reset to trigger on next check
        m_lastWeeklyDate = QDate();
        m_scheduleTimer->start();
    } else {
        spdlog::info("ReportScheduler: Disabled.");
        m_scheduleTimer->stop();
    }
}

void ReportScheduler::generateNow(const QString &type)
{
    QDate today = QDate::currentDate();
    spdlog::info("ReportScheduler: Manual {} report generation triggered.", type.toStdString());

    QFuture<ReportResult> future;
    if (type == "weekly") {
        future = m_generator->generateWeeklyReport(today);
    } else {
        future = m_generator->generateDailyReport(today);
    }

    auto *watcher = new QFutureWatcher<ReportResult>(this);
    connect(watcher, &QFutureWatcher<ReportResult>::finished, this, [this, watcher]() {
        ReportResult result = watcher->result();
        if (result.success) {
            emit reportGenerated(result.reportType, result.reportDate, result.contentMd);
        } else {
            emit reportGenerationFailed(result.reportType, result.errorMessage);
        }
        watcher->deleteLater();
    });
    watcher->setFuture(future);
}

void ReportScheduler::checkSchedule()
{
    if (!m_enabled) return;

    QDateTime now = QDateTime::currentDateTime();
    QDate today = now.date();
    QTime currentTime = now.time();

    // Check daily report schedule
    if (currentTime >= m_dailyTime && m_lastDailyDate != today) {
        if (!m_generator->reportExists(today, "daily")) {
            spdlog::info("ReportScheduler: Triggering daily report for {}", today.toString(Qt::ISODate).toStdString());
            m_lastDailyDate = today;
            generateNow("daily");
        } else {
            m_lastDailyDate = today;
        }
    }

    // Check weekly report schedule
    if (today.dayOfWeek() == m_weeklyDay && currentTime >= m_weeklyTime
        && m_lastWeeklyDate != today) {
        if (!m_generator->reportExists(today, "weekly")) {
            // Find Monday of this week
            QDate weekMonday = today.addDays(-(today.dayOfWeek() - 1));
            spdlog::info("ReportScheduler: Triggering weekly report for week of {}",
                         weekMonday.toString(Qt::ISODate).toStdString());
            m_lastWeeklyDate = today;
            generateNow("weekly");
        } else {
            m_lastWeeklyDate = today;
        }
    }
}

void ReportScheduler::onGenerationFinished()
{
    // Handle completion from async generation
}
