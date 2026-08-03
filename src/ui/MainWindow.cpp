#include "MainWindow.h"
#include "ReportViewer.h"
#include "ReportHistoryWidget.h"
#include "SettingsDialog.h"
#include "TimelineWidget.h"
#include "report/TemplateEngine.h"
#include "report/ReportGenerator.h"
#include "storage/EventRepository.h"
#include "storage/ReportRepository.h"
#include "storage/SettingsRepository.h"
#include <QMenuBar>
#include <QToolBar>
#include <QCloseEvent>
#include <QApplication>
#include <QMessageBox>
#include <spdlog/spdlog.h>

MainWindow::MainWindow(TemplateEngine *templateEngine,
                       ReportGenerator *reportGenerator,
                       EventRepository *eventRepo,
                       ReportRepository *reportRepo,
                       SettingsRepository *settingsRepo,
                       QWidget *parent)
    : QMainWindow(parent)
    , m_templateEngine(templateEngine)
    , m_reportGenerator(reportGenerator)
    , m_reportRepo(reportRepo)
{
    setWindowTitle("DailyReport");
    setMinimumSize(900, 600);
    resize(1100, 700);

    // Create child widgets
    m_reportViewer = new ReportViewer(reportGenerator, reportRepo, this);
    m_historyWidget = new ReportHistoryWidget(reportRepo, this);
    m_settingsDialog = new SettingsDialog(settingsRepo, this);
    m_timelineWidget = new TimelineWidget(eventRepo, this);

    connect(m_historyWidget, &ReportHistoryWidget::reportSelected,
            m_reportViewer, &ReportViewer::loadReport);

    setupUi();
    createMenuBar();

    // Default to report viewer
    m_stack->setCurrentWidget(m_reportViewer);
}

void MainWindow::setupUi()
{
    m_stack = new QStackedWidget(this);
    m_stack->addWidget(m_reportViewer);   // index 0
    m_stack->addWidget(m_historyWidget);  // index 1
    m_stack->addWidget(m_timelineWidget); // index 2
    m_stack->addWidget(m_settingsDialog); // index 3
    setCentralWidget(m_stack);
}

void MainWindow::createMenuBar()
{
    QMenu *fileMenu = menuBar()->addMenu("&File");
    fileMenu->addAction("Generate Daily Report", this, [this]() {
        m_reportViewer->generateReport("daily");
    });
    fileMenu->addAction("Generate Weekly Report", this, [this]() {
        m_reportViewer->generateReport("weekly");
    });
    fileMenu->addSeparator();
    fileMenu->addAction("E&xit", this, []() { QApplication::quit(); });

    QMenu *viewMenu = menuBar()->addMenu("&View");
    viewMenu->addAction("Report Viewer", this, &MainWindow::showReportViewer);
    viewMenu->addAction("Report History", this, &MainWindow::showHistory);
    viewMenu->addAction("Activity Timeline", this, &MainWindow::showTimeline);
    viewMenu->addAction("Settings", this, &MainWindow::showSettings);

    QMenu *helpMenu = menuBar()->addMenu("&Help");
    helpMenu->addAction("About DailyReport", this, [this]() {
        QMessageBox::about(this, "About DailyReport",
                           "DailyReport v1.0.0\n\n"
                           "Automatic developer activity tracking and report generation.\n"
                           "Supports OpenAI, Anthropic, and local LLMs.");
    });
}

void MainWindow::showReportViewer() { m_stack->setCurrentWidget(m_reportViewer); }
void MainWindow::showSettings() { m_stack->setCurrentWidget(m_settingsDialog); }
void MainWindow::showHistory() { m_stack->setCurrentWidget(m_historyWidget); }
void MainWindow::showTimeline() { m_stack->setCurrentWidget(m_timelineWidget); }

void MainWindow::closeEvent(QCloseEvent *event)
{
    // Minimize to tray instead of closing
    hide();
    event->ignore();
    emit closeToTray();
}
