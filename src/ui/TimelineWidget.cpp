#include "TimelineWidget.h"
#include "storage/EventRepository.h"
#include "core/Timeline.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QPushButton>
#include <QLabel>

TimelineWidget::TimelineWidget(EventRepository *eventRepo, QWidget *parent)
    : QWidget(parent)
    , m_eventRepo(eventRepo)
{
    setupUi();
    refresh();
}

void TimelineWidget::setupUi()
{
    auto *layout = new QVBoxLayout(this);

    auto *header = new QHBoxLayout();
    header->addWidget(new QLabel("Date:"));
    m_dateEdit = new QDateEdit(QDate::currentDate());
    m_dateEdit->setCalendarPopup(true);
    header->addWidget(m_dateEdit);

    auto *refreshBtn = new QPushButton("Refresh");
    connect(refreshBtn, &QPushButton::clicked, this, &TimelineWidget::refresh);
    header->addWidget(refreshBtn);
    header->addStretch();

    auto *totalLabel = new QLabel("Total events: -");
    header->addWidget(totalLabel);
    layout->addLayout(header);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(5);
    m_table->setHorizontalHeaderLabels({"Time", "Category", "Type", "Description", "Application"});
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->verticalHeader()->setVisible(false);
    m_table->setColumnWidth(0, 100);
    m_table->setColumnWidth(1, 100);
    m_table->setColumnWidth(2, 120);
    m_table->setColumnWidth(3, 280);

    layout->addWidget(m_table);
}

void TimelineWidget::refresh()
{
    m_table->setRowCount(0);
    QDate date = m_dateEdit->date();
    Timeline timeline = m_eventRepo->queryTimeline(date);
    const auto &events = timeline.events();

    m_table->setRowCount(events.size());
    int row = 0;
    for (const auto &e : events) {
        m_table->setItem(row, 0, new QTableWidgetItem(
            e.timestamp.toLocalTime().toString("HH:mm:ss")));
        m_table->setItem(row, 1, new QTableWidgetItem(
            eventCategoryToString(e.category)));
        m_table->setItem(row, 2, new QTableWidgetItem(
            eventTypeToString(e.type)));
        m_table->setItem(row, 3, new QTableWidgetItem(e.description));
        m_table->setItem(row, 4, new QTableWidgetItem(e.application));
        row++;
    }
}
