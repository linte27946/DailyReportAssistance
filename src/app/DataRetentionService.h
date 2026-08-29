#pragma once

#include <QObject>
#include <QDate>
#include <QDateTime>
#include <QTimer>

class EventRepository;
class ReportRepository;
class SettingsRepository;

/// Applies the user's local data-retention policy at startup and once per day.
class DataRetentionService : public QObject {
    Q_OBJECT

public:
    explicit DataRetentionService(EventRepository *eventRepo,
                                  ReportRepository *reportRepo,
                                  SettingsRepository *settingsRepo,
                                  QObject *parent = nullptr);

    void start();
    void reloadSettings();

public slots:
    void runCleanupNow();

signals:
    void cleanupFinished(int activityEventsDeleted,
                         int reportsDeleted,
                         const QDate &activityCutoff,
                         const QDate &reportCutoff,
                         const QDateTime &completedAt,
                         bool userInitiated);

private:
    void performCleanup(bool userInitiated);
    int activityRetentionMonths() const;
    int reportRetentionMonths() const;

    EventRepository *m_eventRepo = nullptr;
    ReportRepository *m_reportRepo = nullptr;
    SettingsRepository *m_settingsRepo = nullptr;
    QTimer m_cleanupTimer;
    bool m_started = false;
};
