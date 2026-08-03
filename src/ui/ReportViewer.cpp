#include "ReportViewer.h"
#include "report/MarkdownRenderer.h"
#include "storage/ReportRepository.h"
#include <QVBoxLayout>
#include <QToolBar>
#include <QLabel>
#include <QFileDialog>
#include <QMessageBox>
#include <QTextStream>
#include <spdlog/spdlog.h>

ReportViewer::ReportViewer(ReportGenerator *generator,
                           ReportRepository *reportRepo,
                           QWidget *parent)
    : QWidget(parent)
    , m_generator(generator)
    , m_reportRepo(reportRepo)
{
    setupUi();
}

void ReportViewer::setupUi()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    // Toolbar
    m_toolbar = new QToolBar("Report Toolbar", this);
    m_toolbar->addAction("Generate Daily", this, [this]() { generateReport("daily"); });
    m_toolbar->addAction("Generate Weekly", this, [this]() { generateReport("weekly"); });
    m_toolbar->addAction("Refresh", this, &ReportViewer::refresh);
    m_toolbar->addSeparator();
    m_toolbar->addAction("Export as Markdown", this, &ReportViewer::exportToFile);
    m_toolbar->addAction("Export as HTML", this, [this]() {
        QString html = MarkdownRenderer::toHtml(m_currentContent);
        QString path = QFileDialog::getSaveFileName(this, "Save as HTML",
                                                     "report.html", "HTML (*.html)");
        if (!path.isEmpty()) {
            QFile f(path);
            if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
                f.write(html.toUtf8());
                f.close();
            }
        }
    });

    layout->addWidget(m_toolbar);

    // Status label
    m_statusLabel = new QLabel("Ready", this);
    m_statusLabel->setStyleSheet("padding: 4px; color: #888;");
    layout->addWidget(m_statusLabel);

    // Report browser
    m_browser = new QTextBrowser(this);
    m_browser->setOpenExternalLinks(true);
    m_browser->setPlaceholderText(
        "No report generated yet.\n\n"
        "Click 'Generate Daily' or 'Generate Weekly' to create your first report.\n\n"
        "Make sure you have configured an LLM backend in Settings.");
    layout->addWidget(m_browser);
}

void ReportViewer::generateReport(const QString &type)
{
    m_statusLabel->setText(QString("Generating %1 report...").arg(type));

    QDate today = QDate::currentDate();
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
            onGenerationCompleted(result);
        } else {
            onGenerationFailed(result.errorMessage);
        }
        watcher->deleteLater();
    });
    watcher->setFuture(future);
}

void ReportViewer::loadReport(int64_t reportId)
{
    ReportRecord record = m_reportRepo->getReport(reportId);
    if (record.id == 0) return;

    m_currentReportId = reportId;
    m_currentContent = record.contentMd;

    QString html = MarkdownRenderer::toHtml(record.contentMd);
    m_browser->setHtml(html);
    m_statusLabel->setText(QString("Loaded: %1 (%2)")
                               .arg(record.title, record.createdAt.toString("yyyy-MM-dd HH:mm")));
}

void ReportViewer::refresh()
{
    if (m_currentReportId > 0) {
        loadReport(m_currentReportId);
    }
}

void ReportViewer::onGenerationCompleted(const ReportResult &result)
{
    m_currentContent = result.contentMd;
    QString html = MarkdownRenderer::toHtml(result.contentMd);
    m_browser->setHtml(html);
    m_statusLabel->setText(QString("Report generated in %.1fs using %s")
                               .arg(result.generationTimeSecs)
                               .arg(result.llmBackend));
    spdlog::info("ReportViewer: {} report displayed.", result.reportType.toStdString());
}

void ReportViewer::onGenerationFailed(const QString &error)
{
    m_statusLabel->setText(QString("Error: %1").arg(error));
    QMessageBox::warning(this, "Report Generation Failed", error);
}

void ReportViewer::exportToFile()
{
    if (m_currentContent.isEmpty()) {
        QMessageBox::information(this, "No Report", "Generate or load a report first.");
        return;
    }

    QString path = QFileDialog::getSaveFileName(this, "Export Report",
                                                 "daily_report.md", "Markdown (*.md)");
    if (path.isEmpty()) return;

    QFile file(path);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream stream(&file);
        stream << m_currentContent;
        file.close();
        m_statusLabel->setText("Report exported to: " + path);
    } else {
        QMessageBox::warning(this, "Export Failed", "Could not write to file: " + path);
    }
}
