#pragma once

#include <QMainWindow>
#include <QStackedWidget>

class QLabel;
class QListWidget;
class ReportViewer;
class ReportHistoryWidget;
class SettingsDialog;
class TimelineWidget;
class TemplateEngine;
class ReportGenerator;
class EventRepository;
class ReportRepository;
class SettingsRepository;
class DataRetentionService;
class WeComMeetingMonitor;

/// Main application window with stacked views for reports, settings, and timeline.
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(TemplateEngine *templateEngine,
                        ReportGenerator *reportGenerator,
                        EventRepository *eventRepo,
                        ReportRepository *reportRepo,
                        SettingsRepository *settingsRepo,
                        DataRetentionService *retentionService,
                        WeComMeetingMonitor *weComMeetingMonitor,
                        QWidget *parent = nullptr);

    /// Switch to the report viewer tab.
    void showReportViewer();

    /// Switch to the settings tab.
    void showSettings();

    /// Switch to the history tab.
    void showHistory();

    /// Switch to the timeline tab.
    void showTimeline();

    /// Close event — minimize to tray instead of closing.
    void closeEvent(QCloseEvent *event) override;

signals:
    void closeToTray();
    void settingsSaved();

private:
    void setupUi();
    void createMenuBar();
    void selectPage(int index);
    void updatePageHeader(int index);

    QStackedWidget *m_stack = nullptr;
    QListWidget *m_navigation = nullptr;
    QLabel *m_pageTitle = nullptr;
    QLabel *m_pageSubtitle = nullptr;
    ReportViewer *m_reportViewer = nullptr;
    ReportHistoryWidget *m_historyWidget = nullptr;
    SettingsDialog *m_settingsDialog = nullptr;
    TimelineWidget *m_timelineWidget = nullptr;

    TemplateEngine *m_templateEngine = nullptr;
    ReportGenerator *m_reportGenerator = nullptr;
    ReportRepository *m_reportRepo = nullptr;
    SettingsRepository *m_settingsRepo = nullptr;
};
