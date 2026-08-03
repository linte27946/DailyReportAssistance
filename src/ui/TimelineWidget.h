#pragma once

#include <QWidget>
#include <QTableWidget>
#include <QDateEdit>

class EventRepository;

/// Visual timeline showing raw activity events for the selected date.
class TimelineWidget : public QWidget {
    Q_OBJECT

public:
    explicit TimelineWidget(EventRepository *eventRepo, QWidget *parent = nullptr);

    void refresh();

private:
    void setupUi();

    EventRepository *m_eventRepo = nullptr;
    QTableWidget *m_table = nullptr;
    QDateEdit *m_dateEdit = nullptr;
};
