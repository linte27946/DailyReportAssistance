#pragma once

#include <QApplication>
#include <QString>
#include <QStandardPaths>
#include <QDir>
#include <QUuid>
#include <memory>
#include <spdlog/spdlog.h>

#include "storage/Database.h"

/// Custom QApplication subclass that owns all application-level services.
class Application : public QApplication {
    Q_OBJECT

public:
    Application(int &argc, char *argv[])
        : QApplication(argc, argv)
    {
        setApplicationName("DailyReport");
        setApplicationDisplayName("DailyReport");
        setOrganizationName("DailyReport");
        setApplicationVersion("1.0.0");

        // Generate a unique session ID for this run
        m_sessionId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        spdlog::info("Session ID: {}", m_sessionId.toStdString());
    }

    ~Application()
    {
        shutdown();
    }

    /// Get the application data directory.
    static QString dataDirectory()
    {
        return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    }

    /// Get the current session ID (changes each time the app starts).
    QString sessionId() const { return m_sessionId; }

    /// Initialize all application services. Called once after construction.
    bool initialize()
    {
        spdlog::info("Initializing application services...");

        // Initialize the database
        if (!Database::instance().initialize()) {
            spdlog::critical("Failed to initialize database.");
            return false;
        }

        spdlog::info("Application services initialized successfully.");
        return true;
    }

    /// Run the application event loop.
    int run()
    {
        spdlog::info("Starting application event loop.");
        return exec();
    }

    /// Shutdown all application services.
    void shutdown()
    {
        spdlog::info("Shutting down application...");
        Database::instance().closeConnection();
        spdlog::info("Shutdown complete.");
    }

private:
    QString m_sessionId;
};
