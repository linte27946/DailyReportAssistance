#pragma once

#include <QObject>
#include <QTimer>
#include <QTime>
#include <QDate>
#include <QFutureWatcher>

class ReportGenerator;

/// Schedules automatic daily and weekly report generation.
class ReportScheduler : public QObject {
    Q_OBJECT

public:
    explicit ReportScheduler(ReportGenerator *generator, QObject *parent = nullptr);

    /// Configure schedule times.
    void setDailyReportTime(const QTime &time) { m_dailyTime = time; }
    void setWeeklyReportDay(int dayOfWeek) { m_weeklyDay = dayOfWeek; }  // 1=Mon, 5=Fri
    void setWeeklyReportTime(const QTime &time) { m_weeklyTime = time; }

    /// Enable/disable automatic generation.
    void setEnabled(bool enabled);

    /// Force immediate generation.
    void generateNow(const QString &type = "daily");

signals:
    void reportGenerated(const QString &type, const QDate &date, const QString &content);
    void reportGenerationFailed(const QString &type, const QString &error);

private slots:
    void checkSchedule();
    void onGenerationFinished();

private:
    ReportGenerator *m_generator = nullptr;
    QTimer *m_scheduleTimer = nullptr;
    bool m_enabled = false;

    QTime m_dailyTime = QTime(17, 30);   // Default: 5:30 PM
    int m_weeklyDay = 5;                  // Default: Friday
    QTime m_weeklyTime = QTime(17, 0);   // Default: 5:00 PM

    QDate m_lastDailyDate;
    QDate m_lastWeeklyDate;
};
