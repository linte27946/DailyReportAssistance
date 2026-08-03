#include <QApplication>
#include <QMessageBox>
#include <QTimer>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <filesystem>

#include "app/Application.h"
#include "app/SingleInstance.h"
#include "util/Log.h"
#include "ui/MainWindow.h"
#include "ui/SystemTray.h"
#include "report/TemplateEngine.h"
#include "report/ReportGenerator.h"
#include "report/ReportScheduler.h"
#include "llm/LlmClient.h"
#include "storage/EventRepository.h"
#include "storage/ReportRepository.h"
#include "storage/SettingsRepository.h"
#include "monitor/MonitorEngine.h"
#include "monitor/FileSystemMonitor.h"
#include "monitor/ProcessMonitor.h"
#include "monitor/WindowFocusMonitor.h"
#include "monitor/InputActivityMonitor.h"
#include "monitor/BrowserUrlMonitor.h"
#include "monitor/GitMonitor.h"
#include "monitor/BuildMonitor.h"
#include "pipeline/EventPipeline.h"

namespace fs = std::filesystem;

int main(int argc, char *argv[])
{
    // Initialize logging
    Log::init();

    spdlog::info("DailyReport starting...");

    // Check single instance - only one instance of the app should run
    SingleInstance guard("DailyReport_SingleInstance");
    if (!guard.tryLock()) {
        spdlog::warn("Another instance is already running. Exiting.");
        guard.notifyExistingInstance();
        return 0;
    }

    // Create Qt application
    Application app(argc, argv);

    // Ensure data directory exists
    QString dataDir = Application::dataDirectory();
    if (!fs::exists(dataDir.toStdString())) {
        fs::create_directories(dataDir.toStdString());
    }

    // Initialize application services
    if (!app.initialize()) {
        spdlog::critical("Failed to initialize application services.");
        QMessageBox::critical(nullptr, "DailyReport",
                              "Failed to initialize application services.\n"
                              "Please check the log files for details.");
        return 1;
    }

    // --- Create application services ---

    // Core services
    auto *templateEngine = new TemplateEngine(&app);
    auto *llmClient = new LlmClient(&app);
    auto *eventRepo = new EventRepository();
    auto *reportRepo = new ReportRepository();
    auto *settingsRepo = new SettingsRepository();

    // Report generation
    auto *reportGenerator = new ReportGenerator(
        templateEngine, llmClient, eventRepo, reportRepo, &app);
    auto *reportScheduler = new ReportScheduler(reportGenerator, &app);

    // Monitoring
    auto *monitorEngine = new MonitorEngine(&app);
    monitorEngine->registerMonitor(std::make_unique<FileSystemMonitor>());
    monitorEngine->registerMonitor(std::make_unique<ProcessMonitor>());
    monitorEngine->registerMonitor(std::make_unique<WindowFocusMonitor>());
    monitorEngine->registerMonitor(std::make_unique<InputActivityMonitor>());
    monitorEngine->registerMonitor(std::make_unique<BrowserUrlMonitor>());
    monitorEngine->registerMonitor(std::make_unique<GitMonitor>());
    monitorEngine->registerMonitor(std::make_unique<BuildMonitor>());

    // Event pipeline
    auto *pipeline = new EventPipeline(&app);
    QObject::connect(monitorEngine, &MonitorEngine::rawEventCaptured,
                     pipeline, &EventPipeline::onRawEvent);

    // UI
    auto *mainWindow = new MainWindow(
        templateEngine, reportGenerator, eventRepo, reportRepo, settingsRepo);
    auto *systemTray = new SystemTray(mainWindow, reportGenerator, reportScheduler, &app);

    // Wire tray ↔ window
    QObject::connect(systemTray, &SystemTray::showMainWindowRequested,
                     mainWindow, &QMainWindow::show);
    QObject::connect(systemTray, &SystemTray::showSettingsRequested,
                     mainWindow, &MainWindow::showSettings);
    QObject::connect(systemTray, &SystemTray::exitRequested,
                     &app, &QApplication::quit);
    QObject::connect(mainWindow, &MainWindow::closeToTray,
                     mainWindow, &QMainWindow::hide);

    // Initialize system tray
    if (!systemTray->initialize()) {
        spdlog::error("Failed to initialize system tray icon.");
    }

    // Start monitoring
    monitorEngine->startAll();

    // Show window on first run (setup wizard will guide user)
    mainWindow->show();

    spdlog::info("DailyReport initialized successfully.");
    int result = app.run();

    // Cleanup
    monitorEngine->stopAll();

    return result;
}
