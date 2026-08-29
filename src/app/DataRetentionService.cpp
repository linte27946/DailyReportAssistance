#include "DataRetentionService.h"

#include "storage/EventRepository.h"
#include "storage/ReportRepository.h"
#include "storage/SettingsRepository.h"
#include <spdlog/spdlog.h>

namespace {
constexpr int kDefaultRetentionMonths = 3;
constexpr int kMaximumRetentionMonths = 120;
constexpr int kDailyCleanupIntervalMs = 24 * 60 * 60 * 1000;
}

DataRetentionService::DataRetentionService(EventRepository *eventRepo,
                                           ReportRepository *reportRepo,
                                           SettingsRepository *settingsRepo,
                                           QObject *parent)
    : QObject(parent)
    , m_eventRepo(eventRepo)
    , m_reportRepo(reportRepo)
    , m_settingsRepo(settingsRepo)
{
    m_cleanupTimer.setInterval(kDailyCleanupIntervalMs);
    m_cleanupTimer.setTimerType(Qt::VeryCoarseTimer);
    connect(&m_cleanupTimer, &QTimer::timeout, this, [this]() {
        performCleanup(false);
    });
}

void DataRetentionService::start()
{
    if (m_started) return;
    m_started = true;
    performCleanup(false);
    m_cleanupTimer.start();
}

void DataRetentionService::reloadSettings()
{
    // The timer interval is fixed, but restarting it makes the next automatic
    // cleanup predictable after settings are changed.
    if (m_started)
        m_cleanupTimer.start();
}

void DataRetentionService::runCleanupNow()
{
    performCleanup(true);
}

void DataRetentionService::performCleanup(bool userInitiated)
{
    if (!m_eventRepo || !m_reportRepo || !m_settingsRepo) return;

    const QDate today = QDate::currentDate();
    const QDate activityCutoff = today.addMonths(-activityRetentionMonths());
    const QDate reportCutoff = today.addMonths(-reportRetentionMonths());

    const int eventsDeleted = m_eventRepo->pruneOlderThan(activityCutoff);
    const int reportsDeleted = m_reportRepo->pruneOlderThan(reportCutoff);
    const QDateTime completedAt = QDateTime::currentDateTime();

    if (eventsDeleted >= 0 && reportsDeleted >= 0) {
        m_settingsRepo->setValue("last_data_cleanup_at",
                                 completedAt.toString(Qt::ISODateWithMs));
        spdlog::info(
            "Data retention cleanup finished: {} activity events and {} reports removed.",
            eventsDeleted, reportsDeleted);
    }

    emit cleanupFinished(eventsDeleted, reportsDeleted,
                         activityCutoff, reportCutoff,
                         completedAt, userInitiated);
}

int DataRetentionService::activityRetentionMonths() const
{
    const QString configured = m_settingsRepo->getValue("activity_retention_months");
    if (!configured.isEmpty())
        return qBound(1, configured.toInt(), kMaximumRetentionMonths);

    // Compatibility with versions that exposed a single day-based setting.
    const int legacyDays = qMax(1, m_settingsRepo->getInt("data_retention_days", 90));
    return qBound(1, (legacyDays + 29) / 30, kMaximumRetentionMonths);
}

int DataRetentionService::reportRetentionMonths() const
{
    return qBound(1,
                  m_settingsRepo->getInt("report_retention_months",
                                         kDefaultRetentionMonths),
                  kMaximumRetentionMonths);
}
