#pragma once

#include <QWidget>
#include <QTableWidget>
#include <QComboBox>

class ReportRepository;
class QLabel;
class QPushButton;

/// Displays a list of previously generated reports with filtering.
class ReportHistoryWidget : public QWidget {
    Q_OBJECT

public:
    explicit ReportHistoryWidget(ReportRepository *reportRepo, QWidget *parent = nullptr);

    void refresh();

signals:
    void reportSelected(int64_t reportId);
    void reportDeleted(int64_t reportId);

private:
    void setupUi();
    int64_t selectedReportId() const;
    void updateActionState();
    void openSelectedReport();
    void deleteSelectedReport();

    ReportRepository *m_reportRepo = nullptr;
    QTableWidget *m_table = nullptr;
    QComboBox *m_typeFilter = nullptr;
    QLabel *m_countLabel = nullptr;
    QLabel *m_feedbackLabel = nullptr;
    QPushButton *m_openButton = nullptr;
    QPushButton *m_deleteButton = nullptr;
};
