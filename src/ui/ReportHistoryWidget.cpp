#include "ReportHistoryWidget.h"
#include "UiLanguage.h"
#include "storage/ReportRepository.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QPushButton>
#include <QLabel>
#include <QMessageBox>

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

    auto *filterLayout = new QHBoxLayout();
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

    auto *refreshBtn = new QPushButton();
    UiLanguage::bindText(refreshBtn, "Refresh", "刷新");
    connect(refreshBtn, &QPushButton::clicked, this, &ReportHistoryWidget::refresh);
    filterLayout->addWidget(refreshBtn);
    connect(m_typeFilter, &QComboBox::currentTextChanged, this, &ReportHistoryWidget::refresh);

    layout->addLayout(filterLayout);

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
    m_table->verticalHeader()->setVisible(false);
    m_table->setColumnWidth(0, 120);
    m_table->setColumnWidth(1, 80);
    m_table->setColumnWidth(2, 250);
    m_table->setColumnWidth(3, 100);

    connect(m_table, &QTableWidget::cellDoubleClicked, this, [this](int row, int) {
        auto *item = m_table->item(row, 0);
        if (item) {
            int64_t id = item->data(Qt::UserRole).toLongLong();
            emit reportSelected(id);
        }
    });

    layout->addWidget(m_table);
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
}
