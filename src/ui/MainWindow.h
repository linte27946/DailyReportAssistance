#pragma once

#include <QMainWindow>
#include <QStackedWidget>

class ReportViewer;
class ReportHistoryWidget;
class SettingsDialog;
class TimelineWidget;
class TemplateEngine;
class ReportGenerator;
class EventRepository;
class ReportRepository;
class SettingsRepository;

/// Main application window with stacked views for reports, settings, and timeline.
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(TemplateEngine *templateEngine,
                        ReportGenerator *reportGenerator,
                        EventRepository *eventRepo,
                        ReportRepository *reportRepo,
                        SettingsRepository *settingsRepo,
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
    void createToolBar();

    QStackedWidget *m_stack = nullptr;
    ReportViewer *m_reportViewer = nullptr;
    ReportHistoryWidget *m_historyWidget = nullptr;
    SettingsDialog *m_settingsDialog = nullptr;
    TimelineWidget *m_timelineWidget = nullptr;

    TemplateEngine *m_templateEngine = nullptr;
    ReportGenerator *m_reportGenerator = nullptr;
    ReportRepository *m_reportRepo = nullptr;
};
