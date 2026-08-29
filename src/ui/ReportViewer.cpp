#include "ReportViewer.h"
#include "UiLanguage.h"
#include "report/MarkdownRenderer.h"
#include "storage/ReportRepository.h"
#include <QVBoxLayout>
#include <QToolBar>
#include <QLabel>
#include <QFileDialog>
#include <QApplication>
#include <QClipboard>
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
    layout->setContentsMargins(22, 20, 22, 22);
    layout->setSpacing(12);

    m_toolbar = new QToolBar("Report Toolbar", this);
    m_toolbar->setMovable(false);
    m_toolbar->setFloatable(false);
    QAction *refreshAction = m_toolbar->addAction("", this, &ReportViewer::refresh);
    UiLanguage::bindText(refreshAction, "Refresh", "刷新");
    QAction *markdownAction = m_toolbar->addAction("", this, &ReportViewer::exportToFile);
    UiLanguage::bindText(markdownAction, "Export as Markdown", "导出为 Markdown");
    QAction *htmlAction = m_toolbar->addAction("", this, [this]() {
        QString html = MarkdownRenderer::toHtml(m_currentContent);
        QString path = QFileDialog::getSaveFileName(
            this, UiLanguage::text("Save as HTML", "保存为 HTML"),
            "report.html", "HTML (*.html)");
        if (!path.isEmpty()) {
            QFile f(path);
            if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
                f.write(html.toUtf8());
                f.close();
            }
        }
    });
    UiLanguage::bindText(htmlAction, "Export as HTML", "导出为 HTML");
    m_toolbar->addSeparator();
    QAction *copyPromptAction = m_toolbar->addAction(
        "", this, &ReportViewer::copyExternalPrompt);
    QAction *exportPromptAction = m_toolbar->addAction(
        "", this, &ReportViewer::exportExternalPrompt);
    UiLanguage::bindText(copyPromptAction,
                         "Copy AI prompt", "复制 AI 提示词");
    UiLanguage::bindText(exportPromptAction,
                         "Export AI package", "导出 AI 总结包");

    layout->addWidget(m_toolbar);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setObjectName("reportStatus");
    m_statusLabel->setStyleSheet(
        "QLabel#reportStatus { color: #667389; background: #f5f7fa; "
        "border-radius: 6px; padding: 9px 12px; }");
    UiLanguage::bindText(m_statusLabel, "Ready", "就绪");
    layout->addWidget(m_statusLabel);

    m_browser = new QTextBrowser(this);
    m_browser->setOpenExternalLinks(true);
    UiLanguage::bindPlaceholder(
        m_browser,
        "No report yet.\n\n"
        "Use Generate daily or Generate weekly in the top-right corner.\n"
        "Configure an AI provider in Settings, or copy/export an AI package without an API key.",
        "还没有报告。\n\n"
        "点击右上角的“生成日报”或“生成周报”。\n"
        "你可以配置 AI 服务，也可以在没有 API Key 时复制提示词或导出 AI 总结包。");
    layout->addWidget(m_browser);
}

void ReportViewer::copyExternalPrompt()
{
    const QString prompt = m_generator->buildExternalPrompt(QDate::currentDate(), "daily");
    QApplication::clipboard()->setText(prompt);
    m_statusLabel->setText(UiLanguage::text(
        "AI prompt copied. Paste it into any AI chat website.",
        "AI 提示词已复制，可粘贴到任意 AI 对话网站。"));
}

void ReportViewer::exportExternalPrompt()
{
    const QDate date = QDate::currentDate();
    const QString path = QFileDialog::getSaveFileName(
        this,
        UiLanguage::text("Export AI summary package", "导出 AI 总结包"),
        QString("dailyreport_ai_package_%1.md").arg(date.toString(Qt::ISODate)),
        "Markdown (*.md);;Text (*.txt)");
    if (path.isEmpty()) return;

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(
            this, UiLanguage::text("Export failed", "导出失败"),
            UiLanguage::text("Could not write to file: ", "无法写入文件：") + path);
        return;
    }
    QTextStream stream(&file);
    stream << m_generator->buildExternalPrompt(date, "daily");
    file.close();
    m_statusLabel->setText(UiLanguage::text(
        "AI summary package exported to: " + path,
        "AI 总结包已导出到：" + path));
}

void ReportViewer::generateReport(const QString &type)
{
    const QString typeLabel = type == "weekly"
        ? UiLanguage::text("weekly", "周") : UiLanguage::text("daily", "日");
    m_statusLabel->setText(UiLanguage::text(
        QString("Generating %1 report...").arg(typeLabel),
        QString("正在生成%1报……").arg(typeLabel)));

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
    m_statusLabel->setText(UiLanguage::text(
        QString("Loaded: %1 (%2)")
            .arg(record.title, record.createdAt.toString("yyyy-MM-dd HH:mm")),
        QString("已载入：%1（%2）")
            .arg(record.title, record.createdAt.toString("yyyy-MM-dd HH:mm"))));
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
    m_statusLabel->setText(UiLanguage::text(
        QString("Report generated in %1s using %2")
            .arg(result.generationTimeSecs).arg(result.llmBackend),
        QString("报告已生成，用时 %1 秒，AI 服务：%2")
            .arg(result.generationTimeSecs).arg(result.llmBackend)));
    spdlog::info("ReportViewer: {} report displayed.", result.reportType.toStdString());
}

void ReportViewer::onGenerationFailed(const QString &error)
{
    m_statusLabel->setText(UiLanguage::text(
        QString("Error: %1").arg(error), QString("错误：%1").arg(error)));
    QMessageBox::warning(
        this, UiLanguage::text("Report generation failed", "报告生成失败"), error);
}

void ReportViewer::exportToFile()
{
    if (m_currentContent.isEmpty()) {
        QMessageBox::information(
            this, UiLanguage::text("No report", "没有报告"),
            UiLanguage::text("Generate or load a report first.", "请先生成或载入报告。"));
        return;
    }

    QString path = QFileDialog::getSaveFileName(
        this, UiLanguage::text("Export report", "导出报告"),
        "daily_report.md", "Markdown (*.md)");
    if (path.isEmpty()) return;

    QFile file(path);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream stream(&file);
        stream << m_currentContent;
        file.close();
        m_statusLabel->setText(UiLanguage::text(
            "Report exported to: " + path, "报告已导出到：" + path));
    } else {
        QMessageBox::warning(
            this, UiLanguage::text("Export failed", "导出失败"),
            UiLanguage::text("Could not write to file: ", "无法写入文件：") + path);
    }
}
