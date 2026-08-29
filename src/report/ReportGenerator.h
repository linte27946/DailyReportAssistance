#pragma once

#include <QObject>
#include <QDate>
#include <QFuture>
#include <QFutureWatcher>
#include <QElapsedTimer>
#include <QFutureSynchronizer>
#include <QMutex>
#include "core/Timeline.h"
#include "llm/LlmClient.h"

class TemplateEngine;
class EventRepository;
class ReportRepository;

/// Result of a report generation.
struct ReportResult {
    bool success = false;
    QString contentMd;
    QString errorMessage;
    QDate reportDate;
    QString reportType;
    QString llmBackend;
    QString llmModel;
    double generationTimeSecs = 0;
};

/// Orchestrates the generation of daily/weekly reports.
class ReportGenerator : public QObject {
    Q_OBJECT

public:
    explicit ReportGenerator(TemplateEngine *templateEngine,
                             LlmClient *llmClient,
                             EventRepository *eventRepo,
                             ReportRepository *reportRepo,
                             QObject *parent = nullptr);
    ~ReportGenerator() override;

    /// Generate a daily report for the given date.
    QFuture<ReportResult> generateDailyReport(const QDate &date = QDate::currentDate());

    /// Generate a weekly report for the week containing the given date.
    QFuture<ReportResult> generateWeeklyReport(const QDate &date = QDate::currentDate());

    /// Regenerate an existing report (re-uses the same date/type).
    QFuture<ReportResult> regenerateReport(const QDate &date, const QString &type);

    /// Build a self-contained prompt package for use with a free AI website.
    /// This does not call an API and never includes source/document contents.
    QString buildExternalPrompt(const QDate &date = QDate::currentDate(),
                                const QString &type = "daily");

    void setReportLanguage(const QString &language);

    bool reportExists(const QDate &date, const QString &type) const;

    void waitForFinished() { m_futureSynchronizer.waitForFinished(); }

signals:
    void generationStarted(const QString &reportType, const QDate &date);
    void generationProgress(int percent);
    void generationCompleted(const ReportResult &result);
    void generationFailed(const QString &error);

private:
    ReportResult doGenerate(const QDate &date, const QString &type);

    TemplateEngine *m_templateEngine = nullptr;
    LlmClient *m_llmClient = nullptr;
    EventRepository *m_eventRepo = nullptr;
    ReportRepository *m_reportRepo = nullptr;
    QString m_reportLanguage = "zh-CN";
    mutable QMutex m_configMutex;
    QFutureSynchronizer<ReportResult> m_futureSynchronizer;
};
