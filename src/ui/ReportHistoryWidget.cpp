#include "ReportHistoryWidget.h"
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
    filterLayout->addWidget(new QLabel("Filter:"));
    m_typeFilter = new QComboBox();
    m_typeFilter->addItems({"All", "Daily", "Weekly"});
    filterLayout->addWidget(m_typeFilter);
    filterLayout->addStretch();

    auto *refreshBtn = new QPushButton("Refresh");
    connect(refreshBtn, &QPushButton::clicked, this, &ReportHistoryWidget::refresh);
    filterLayout->addWidget(refreshBtn);
    connect(m_typeFilter, &QComboBox::currentTextChanged, this, &ReportHistoryWidget::refresh);

    layout->addLayout(filterLayout);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(5);
    m_table->setHorizontalHeaderLabels({"Date", "Type", "Title", "Backend", "Generated"});
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
    QString filter = m_typeFilter->currentText().toLower();
    if (filter == "all") filter = {};

    int row = 0;
    for (const auto &r : reports) {
        if (!filter.isEmpty() && r.reportType.toLower() != filter)
            continue;

        m_table->insertRow(row);
        auto *dateItem = new QTableWidgetItem(r.reportDate.toString("yyyy-MM-dd"));
        dateItem->setData(Qt::UserRole, QVariant::fromValue(r.id));
        m_table->setItem(row, 0, dateItem);
        m_table->setItem(row, 1, new QTableWidgetItem(r.reportType));
        m_table->setItem(row, 2, new QTableWidgetItem(r.title));
        m_table->setItem(row, 3, new QTableWidgetItem(r.llmBackend));
        m_table->setItem(row, 4, new QTableWidgetItem(r.createdAt.toString("yyyy-MM-dd HH:mm")));
        row++;
    }
}
