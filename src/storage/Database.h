#pragma once

#include <QString>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QMutex>
#include <QThread>
#include <QCoreApplication>
#include <QDir>
#include <QStandardPaths>
#include <spdlog/spdlog.h>
#include "DatabaseMigrator.h"

/// RAII database manager with connection-per-thread support.
/// SQLite databases are not thread-safe; each thread must use its own connection.
class Database {
public:
    /// Get the singleton instance.
    static Database &instance()
    {
        static Database db;
        return db;
    }

    /// Initialize the database. Must be called once at startup from the main thread.
    bool initialize(const QString &dbPath = {})
    {
        QMutexLocker lock(&m_mutex);

        m_dbPath = dbPath.isEmpty()
            ? defaultDatabasePath()
            : dbPath;

        // Ensure the directory exists
        QDir dir = QFileInfo(m_dbPath).absoluteDir();
        if (!dir.exists()) {
            dir.mkpath(".");
        }

        spdlog::info("Database path: {}", m_dbPath.toStdString());

        // Open the main connection
        {
            QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", mainConnectionName());
            db.setDatabaseName(m_dbPath);
            if (!db.open()) {
                spdlog::error("Failed to open database: {}", db.lastError().text().toStdString());
                return false;
            }

            // Enable WAL mode for better concurrent read performance
            QSqlQuery query(db);
            query.exec("PRAGMA journal_mode=WAL");
            query.exec("PRAGMA foreign_keys=ON");
            query.exec("PRAGMA busy_timeout=5000");
        }

        // Run migrations
        DatabaseMigrator migrator(migrationDir());
        {
            auto db = connection();
            if (!migrator.migrate(db)) {
                spdlog::error("Database migration failed.");
                return false;
            }
        }

        m_initialized = true;
        return true;
    }

    /// Get a database connection for the current thread.
    /// Creates a new named connection if one doesn't exist for this thread.
    QSqlDatabase connection()
    {
        QString connName = threadConnectionName();

        if (QSqlDatabase::contains(connName)) {
            QSqlDatabase db = QSqlDatabase::database(connName);
            if (db.isOpen()) return db;
        }

        // Create a new connection for this thread
        QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", connName);
        db.setDatabaseName(m_dbPath);

        if (!db.open()) {
            spdlog::error("Failed to open database for thread {}: {}",
                          (void*)QThread::currentThreadId(),
                          db.lastError().text().toStdString());
        }

        return db;
    }

    /// Close the connection for the current thread.
    void closeConnection()
    {
        QString connName = threadConnectionName();
        if (QSqlDatabase::contains(connName)) {
            QSqlDatabase::removeDatabase(connName);
        }
    }

    /// Check if the database is initialized.
    bool isInitialized() const { return m_initialized; }

private:
    Database() = default;
    ~Database()
    {
        // Close all connections
        QStringList connNames = QSqlDatabase::connectionNames();
        for (const auto &name : connNames) {
            QSqlDatabase::removeDatabase(name);
        }
    }

    Database(const Database &) = delete;
    Database &operator=(const Database &) = delete;

    static QString defaultDatabasePath()
    {
        return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
               + "/data/dailyreport.db";
    }

    static QString migrationDir()
    {
        // Look for migrations relative to the executable, then fallback to source tree
        QString exeDir = QCoreApplication::applicationDirPath();
        QStringList candidates = {
            exeDir + "/migrations",
            exeDir + "/../src/storage/migrations",
            exeDir + "/../../src/storage/migrations",
            exeDir + "/../../../src/storage/migrations",
        };
        for (const auto &c : candidates) {
            if (QDir(c).exists()) return c;
        }
        return "src/storage/migrations"; // fallback for development
    }

    static QString mainConnectionName() { return "dailyreport_main"; }

    static QString threadConnectionName()
    {
        return QString("dailyreport_thread_%1")
            .arg((quintptr)QThread::currentThreadId());
    }

    QString m_dbPath;
    QMutex m_mutex;
    bool m_initialized = false;
};
