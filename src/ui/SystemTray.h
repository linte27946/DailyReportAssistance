#pragma once

#include <QObject>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>

class MainWindow;
class ReportGenerator;
class ReportScheduler;

/// System tray icon with context menu — the primary user interaction point.
class SystemTray : public QObject {
    Q_OBJECT

public:
    explicit SystemTray(MainWindow *mainWindow,
                        ReportGenerator *reportGenerator,
                        ReportScheduler *reportScheduler,
                        QObject *parent = nullptr);

    /// Initialize the tray icon and menu.
    bool initialize();

    /// Show a notification balloon.
    void showNotification(const QString &title, const QString &message,
                          QSystemTrayIcon::MessageIcon icon = QSystemTrayIcon::Information);

    /// Update the tray tooltip.
    void setTooltip(const QString &text);

signals:
    void showMainWindowRequested();
    void showSettingsRequested();
    void generateReportRequested();
    void exitRequested();

private slots:
    void onTrayActivated(QSystemTrayIcon::ActivationReason reason);
    void onGenerateDailyReport();
    void onGenerateWeeklyReport();
    void onReportGenerated(const QString &type, const QDate &date, const QString &content);

private:
    QSystemTrayIcon *m_trayIcon = nullptr;
    QMenu *m_trayMenu = nullptr;
    MainWindow *m_mainWindow = nullptr;
    ReportGenerator *m_reportGenerator = nullptr;
    ReportScheduler *m_reportScheduler = nullptr;

    QAction *m_viewReportAction = nullptr;
    QAction *m_generateDailyAction = nullptr;
    QAction *m_generateWeeklyAction = nullptr;
    QAction *m_settingsAction = nullptr;
    QAction *m_exitAction = nullptr;

    void createMenu();
    void createTrayIcon();
};
