#include <QApplication>
#include <QMessageBox>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <filesystem>

#include "app/Application.h"
#include "app/SingleInstance.h"
#include "util/Log.h"

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

    spdlog::info("DailyReport initialized successfully.");
    return app.run();
}
