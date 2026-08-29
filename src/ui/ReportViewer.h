#pragma once

#include <QWidget>
#include <QLabel>
#include <QTextBrowser>
#include <QToolBar>
#include <QFutureWatcher>
#include "report/ReportGenerator.h"

class ReportRepository;

/// Displays a generated report in a rich text browser with export options.
class ReportViewer : public QWidget {
    Q_OBJECT

public:
    explicit ReportViewer(ReportGenerator *generator,
                          ReportRepository *reportRepo,
                          QWidget *parent = nullptr);

    /// Generate and display a new report.
    void generateReport(const QString &type);

    /// Load and display a report by ID.
    void loadReport(int64_t reportId);

    /// Refresh the current report.
    void refresh();

public slots:
    void onGenerationCompleted(const ReportResult &result);
    void onGenerationFailed(const QString &error);

private:
    void setupUi();
    void exportToFile();
    void copyExternalPrompt();
    void exportExternalPrompt();

    ReportGenerator *m_generator = nullptr;
    ReportRepository *m_reportRepo = nullptr;

    QTextBrowser *m_browser = nullptr;
    QToolBar *m_toolbar = nullptr;
    QLabel *m_statusLabel = nullptr;

    int64_t m_currentReportId = -1;
    QString m_currentContent;
};
