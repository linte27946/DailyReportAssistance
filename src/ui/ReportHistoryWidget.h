#pragma once

#include <QWidget>
#include <QTableWidget>
#include <QComboBox>

class ReportRepository;

/// Displays a list of previously generated reports with filtering.
class ReportHistoryWidget : public QWidget {
    Q_OBJECT

public:
    explicit ReportHistoryWidget(ReportRepository *reportRepo, QWidget *parent = nullptr);

    void refresh();

signals:
    void reportSelected(int64_t reportId);

private:
    void setupUi();

    ReportRepository *m_reportRepo = nullptr;
    QTableWidget *m_table = nullptr;
    QComboBox *m_typeFilter = nullptr;
};
