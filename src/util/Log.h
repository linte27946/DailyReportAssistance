#pragma once

#include <spdlog/spdlog.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <iostream>
#include <QString>
#include <QStandardPaths>
#include <vector>

/// Centralized logging setup using spdlog.
class Log {
public:
    static void init()
    {
        try {
            // Log directory
            QString logPath = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
                              + "/logs/dailyreport.log";

            // Console sink (DEBUG level for development)
            auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            consoleSink->set_level(spdlog::level::debug);
            consoleSink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%t] %v");

            // Rotating file sink (INFO level, 5MB max, 3 files)
            auto fileSink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
                logPath.toStdString(), 1024 * 1024 * 5, 3);
            fileSink->set_level(spdlog::level::info);
            fileSink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] [%t] %v");

            std::vector<spdlog::sink_ptr> sinks{consoleSink, fileSink};
            auto logger = std::make_shared<spdlog::logger>("DailyReport", sinks.begin(), sinks.end());
            logger->set_level(spdlog::level::debug);
            logger->flush_on(spdlog::level::info);
            spdlog::set_default_logger(logger);

            spdlog::info("Logging initialized. Log file: {}", logPath.toStdString());
        } catch (const spdlog::spdlog_ex &ex) {
            // Fallback: log to stderr
            std::cerr << "Log initialization failed: " << ex.what() << std::endl;
        }
    }
};
