#include "SystemTray.h"
#include "MainWindow.h"
#include "report/ReportGenerator.h"
#include "report/ReportScheduler.h"
#include <QApplication>
#include <QStyle>
#include <spdlog/spdlog.h>

SystemTray::SystemTray(MainWindow *mainWindow,
                       ReportGenerator *reportGenerator,
                       ReportScheduler *reportScheduler,
                       QObject *parent)
    : QObject(parent)
    , m_mainWindow(mainWindow)
    , m_reportGenerator(reportGenerator)
    , m_reportScheduler(reportScheduler)
{
}

bool SystemTray::initialize()
{
    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        spdlog::warn("System tray is not available on this system.");
        return false;
    }

    createTrayIcon();
    createMenu();

    m_trayIcon->setContextMenu(m_trayMenu);
    m_trayIcon->setToolTip("DailyReport - Monitoring...");
    m_trayIcon->show();

    // Connect report scheduler notifications
    connect(m_reportScheduler, &ReportScheduler::reportGenerated,
            this, &SystemTray::onReportGenerated);

    spdlog::info("SystemTray initialized.");
    return true;
}

void SystemTray::createTrayIcon()
{
    m_trayIcon = new QSystemTrayIcon(this);

    // Use application icon or a built-in icon
    QIcon icon = QApplication::windowIcon();
    if (icon.isNull()) {
        icon = QApplication::style()->standardIcon(QStyle::SP_ComputerIcon);
    }
    m_trayIcon->setIcon(icon);

    connect(m_trayIcon, &QSystemTrayIcon::activated,
            this, &SystemTray::onTrayActivated);
}

void SystemTray::createMenu()
{
    m_trayMenu = new QMenu();

    m_viewReportAction = m_trayMenu->addAction("View Today's Report");
    m_viewReportAction->setIcon(QApplication::style()->standardIcon(QStyle::SP_FileDialogDetailedView));
    connect(m_viewReportAction, &QAction::triggered, this, [this]() {
        m_mainWindow->show();
        m_mainWindow->raise();
        m_mainWindow->activateWindow();
        m_mainWindow->showReportViewer();
    });

    m_trayMenu->addSeparator();

    m_generateDailyAction = m_trayMenu->addAction("Generate Daily Report Now");
    connect(m_generateDailyAction, &QAction::triggered,
            this, &SystemTray::onGenerateDailyReport);

    m_generateWeeklyAction = m_trayMenu->addAction("Generate Weekly Report Now");
    connect(m_generateWeeklyAction, &QAction::triggered,
            this, &SystemTray::onGenerateWeeklyReport);

    m_trayMenu->addSeparator();

    m_settingsAction = m_trayMenu->addAction("Settings...");
    m_settingsAction->setIcon(QApplication::style()->standardIcon(QStyle::SP_FileDialogDetailedView));
    connect(m_settingsAction, &QAction::triggered, this, [this]() {
        m_mainWindow->show();
        m_mainWindow->raise();
        m_mainWindow->showSettings();
    });

    m_trayMenu->addSeparator();

    m_exitAction = m_trayMenu->addAction("Exit DailyReport");
    m_exitAction->setIcon(QApplication::style()->standardIcon(QStyle::SP_DialogCloseButton));
    connect(m_exitAction, &QAction::triggered, this, [this]() {
        spdlog::info("Exit requested from system tray.");
        emit exitRequested();
        QApplication::quit();
    });
}

void SystemTray::onTrayActivated(QSystemTrayIcon::ActivationReason reason)
{
    switch (reason) {
    case QSystemTrayIcon::DoubleClick:
    case QSystemTrayIcon::Trigger:  // Left click
        m_mainWindow->show();
        m_mainWindow->raise();
        m_mainWindow->activateWindow();
        break;
    case QSystemTrayIcon::MiddleClick:
        onGenerateDailyReport();
        break;
    default:
        break;
    }
}

void SystemTray::onGenerateDailyReport()
{
    m_reportScheduler->generateNow("daily");
}

void SystemTray::onGenerateWeeklyReport()
{
    m_reportScheduler->generateNow("weekly");
}

void SystemTray::onReportGenerated(const QString &type, const QDate &date, const QString &content)
{
    QString typeLabel = (type == "weekly") ? "Weekly" : "Daily";
    showNotification(
        QString("%1 Report Ready").arg(typeLabel),
        QString("Your %1 report for %2 has been generated.")
            .arg(typeLabel.toLower(), date.toString("yyyy-MM-dd")));
}

void SystemTray::showNotification(const QString &title, const QString &message,
                                   QSystemTrayIcon::MessageIcon icon)
{
    if (m_trayIcon && m_trayIcon->supportsMessages()) {
        m_trayIcon->showMessage(title, message, icon, 5000);
    }
}

void SystemTray::setTooltip(const QString &text)
{
    if (m_trayIcon) {
        m_trayIcon->setToolTip(text);
    }
}
