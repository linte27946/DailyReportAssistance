#include "ReportHistoryWidget.h"
#include "UiLanguage.h"
#include "DialogUtils.h"
#include "ReportDeleteDialog.h"
#include "storage/ReportRepository.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QPushButton>
#include <QLabel>
#include <QShortcut>
#include <QTimer>
#include <QStyle>

ReportHistoryWidget::ReportHistoryWidget(ReportRepository *reportRepo, QWidget *parent)
    : QWidget(parent)
    , m_reportRepo(reportRepo)
{
    setupUi();
    refresh();
}

void ReportHistoryWidget::setupUi()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(22, 20, 22, 22);
    layout->setSpacing(14);

    auto *filterLayout = new QHBoxLayout();
    filterLayout->setSpacing(9);
    auto *filterLabel = new QLabel();
    UiLanguage::bindText(filterLabel, "Filter:", "筛选：");
    filterLayout->addWidget(filterLabel);
    m_typeFilter = new QComboBox();
    m_typeFilter->addItem("", "");
    m_typeFilter->addItem("", "daily");
    m_typeFilter->addItem("", "weekly");
    UiLanguage::bindComboItem(m_typeFilter, 0, "All", "全部");
    UiLanguage::bindComboItem(m_typeFilter, 1, "Daily", "日报");
    UiLanguage::bindComboItem(m_typeFilter, 2, "Weekly", "周报");
    filterLayout->addWidget(m_typeFilter);
    filterLayout->addStretch();

    m_countLabel = new QLabel();
    m_countLabel->setObjectName("summaryBadge");
    filterLayout->addWidget(m_countLabel);

    auto *refreshBtn = new QPushButton();
    refreshBtn->setIcon(style()->standardIcon(QStyle::SP_BrowserReload));
    UiLanguage::bindText(refreshBtn, "Refresh", "刷新");
    connect(refreshBtn, &QPushButton::clicked, this, &ReportHistoryWidget::refresh);
    filterLayout->addWidget(refreshBtn);

    m_openButton = new QPushButton();
    m_openButton->setIcon(style()->standardIcon(QStyle::SP_DialogOpenButton));
    UiLanguage::bindText(m_openButton, "Open selected", "打开所选报告");
    connect(m_openButton, &QPushButton::clicked,
            this, &ReportHistoryWidget::openSelectedReport);
    filterLayout->addWidget(m_openButton);

    m_deleteButton = new QPushButton();
    m_deleteButton->setObjectName("dangerButton");
    m_deleteButton->setIcon(style()->standardIcon(QStyle::SP_TrashIcon));
    UiLanguage::bindText(m_deleteButton, "Delete selected", "删除所选报告");
    connect(m_deleteButton, &QPushButton::clicked,
            this, &ReportHistoryWidget::deleteSelectedReport);
    filterLayout->addWidget(m_deleteButton);
    connect(m_typeFilter, &QComboBox::currentTextChanged, this, &ReportHistoryWidget::refresh);

    layout->addLayout(filterLayout);

    m_feedbackLabel = new QLabel(this);
    m_feedbackLabel->setObjectName("inlineSuccess");
    m_feedbackLabel->setVisible(false);
    layout->addWidget(m_feedbackLabel);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(5);
    m_table->setHorizontalHeaderLabels({"", "", "", "", ""});
    UiLanguage::bindHeader(m_table, 0, "Date", "日期");
    UiLanguage::bindHeader(m_table, 1, "Type", "类型");
    UiLanguage::bindHeader(m_table, 2, "Title", "标题");
    UiLanguage::bindHeader(m_table, 3, "AI provider", "AI 服务");
    UiLanguage::bindHeader(m_table, 4, "Generated", "生成时间");
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setAlternatingRowColors(true);
    m_table->setShowGrid(false);
    m_table->verticalHeader()->setVisible(false);
    m_table->verticalHeader()->setDefaultSectionSize(40);
    m_table->setColumnWidth(0, 120);
    m_table->setColumnWidth(1, 80);
    m_table->setColumnWidth(2, 250);
    m_table->setColumnWidth(3, 100);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);

    connect(m_table, &QTableWidget::cellDoubleClicked,
            this, [this](int, int) { openSelectedReport(); });
    connect(m_table, &QTableWidget::itemSelectionChanged,
            this, &ReportHistoryWidget::updateActionState);

    auto *deleteShortcut = new QShortcut(QKeySequence::Delete, this);
    connect(deleteShortcut, &QShortcut::activated,
            this, &ReportHistoryWidget::deleteSelectedReport);

    layout->addWidget(m_table);
    updateActionState();
}

void ReportHistoryWidget::refresh()
{
    m_table->setRowCount(0);
    auto reports = m_reportRepo->getReports(0, 100);
    const QString filter = m_typeFilter->currentData().toString();

    int row = 0;
    for (const auto &r : reports) {
        if (!filter.isEmpty() && r.reportType.toLower() != filter)
            continue;

        m_table->insertRow(row);
        auto *dateItem = new QTableWidgetItem(r.reportDate.toString("yyyy-MM-dd"));
        dateItem->setData(Qt::UserRole, QVariant::fromValue(r.id));
        m_table->setItem(row, 0, dateItem);
        m_table->setItem(row, 1, new QTableWidgetItem(
            r.reportType == "weekly"
                ? UiLanguage::text("Weekly", "周报")
                : UiLanguage::text("Daily", "日报")));
        m_table->setItem(row, 2, new QTableWidgetItem(r.title));
        m_table->setItem(row, 3, new QTableWidgetItem(r.llmBackend));
        m_table->setItem(row, 4, new QTableWidgetItem(r.createdAt.toString("yyyy-MM-dd HH:mm")));
        row++;
    }

    UiLanguage::bindText(
        m_countLabel,
        row == 0 ? "No reports" : QString("%1 reports").arg(row),
        row == 0 ? "暂无报告" : QString("%1 份报告").arg(row));
    updateActionState();
}

int64_t ReportHistoryWidget::selectedReportId() const
{
    const int row = m_table->currentRow();
    if (row < 0) return 0;
    const auto *item = m_table->item(row, 0);
    return item ? item->data(Qt::UserRole).toLongLong() : 0;
}

void ReportHistoryWidget::updateActionState()
{
    const bool hasSelection = selectedReportId() > 0;
    m_openButton->setEnabled(hasSelection);
    m_deleteButton->setEnabled(hasSelection);
}

void ReportHistoryWidget::openSelectedReport()
{
    const int64_t id = selectedReportId();
    if (id > 0) emit reportSelected(id);
}

void ReportHistoryWidget::deleteSelectedReport()
{
    const int64_t id = selectedReportId();
    if (id <= 0) return;

    const ReportRecord report = m_reportRepo->getReport(id);
    if (report.id <= 0) {
        DialogUtils::warning(
            this, UiLanguage::text("Report not found", "未找到报告"),
            UiLanguage::text(
                "This report no longer exists. The list will be refreshed.",
                "该报告已不存在，列表将自动刷新。"));
        refresh();
        return;
    }

    const bool confirmed = ReportDeleteDialog::confirm(
        report.title, report.reportType, report.reportDate, this);
    if (!confirmed) return;

    if (!m_reportRepo->deleteReport(id)) {
        DialogUtils::warning(
            this, UiLanguage::text("Delete failed", "删除失败"),
            UiLanguage::text(
                "The report could not be deleted. Check the application log.",
                "无法删除该报告，请检查应用日志。"));
        return;
    }

    emit reportDeleted(id);
    refresh();
    m_feedbackLabel->setText(UiLanguage::text(
        QString("Deleted report: %1").arg(report.title),
        QString("已删除报告：%1").arg(report.title)));
    m_feedbackLabel->setVisible(true);
    QTimer::singleShot(4000, this, [this]() {
        m_feedbackLabel->setVisible(false);
    });
}
