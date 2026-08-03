#pragma once

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QString>
#include <QDir>
#include <QFile>
#include <QMap>
#include <spdlog/spdlog.h>

/// Manages database schema migrations.
/// Reads .sql files from the migrations/ directory and applies them in version order.
class DatabaseMigrator {
public:
    /// Initialize the migrator with the path to migration SQL files.
    explicit DatabaseMigrator(const QString &migrationsDir)
        : m_migrationsDir(migrationsDir)
    {
    }

    /// Run all pending migrations. Returns true on success.
    bool migrate(QSqlDatabase &db)
    {
        spdlog::info("Running database migrations...");

        // Ensure schema_version table exists (bootstrap)
        QSqlQuery query(db);
        query.exec("CREATE TABLE IF NOT EXISTS schema_version ("
                   "version INTEGER PRIMARY KEY,"
                   "applied_at TEXT NOT NULL DEFAULT (datetime('now')),"
                   "description TEXT"
                   ")");

        if (query.lastError().isValid()) {
            spdlog::error("Failed to create schema_version table: {}",
                          query.lastError().text().toStdString());
            return false;
        }

        // Get current version
        int currentVersion = getCurrentVersion(db);
        spdlog::info("Current schema version: {}", currentVersion);

        // Find and sort migration files
        QDir dir(m_migrationsDir);
        QStringList filters;
        filters << "*.sql";
        QFileInfoList files = dir.entryInfoList(filters, QDir::Files, QDir::Name);

        for (const auto &fileInfo : files) {
            // Parse version number from filename (e.g., "001_initial_schema.sql" -> 1)
            QString name = fileInfo.baseName();
            QStringList parts = name.split('_');
            if (parts.isEmpty()) continue;
            bool ok = false;
            int version = parts.first().toInt(&ok);
            if (!ok) continue;

            if (version <= currentVersion) continue;

            spdlog::info("Applying migration {}: {}", version, name.toStdString());

            // Read and execute the migration
            QFile file(fileInfo.absoluteFilePath());
            if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                spdlog::error("Failed to open migration file: {}",
                              fileInfo.absoluteFilePath().toStdString());
                return false;
            }

            QString sql = QString::fromUtf8(file.readAll());
            file.close();

            // Execute SQL statements (split by semicolons on their own)
            QStringList statements = sql.split(';', Qt::SkipEmptyParts);
            for (const auto &stmt : statements) {
                QString trimmed = stmt.trimmed();
                if (trimmed.isEmpty()) continue;

                if (!query.exec(trimmed)) {
                    spdlog::error("Migration {} failed: {}",
                                  version, query.lastError().text().toStdString());
                    spdlog::error("SQL: {}", trimmed.toStdString());
                    return false;
                }
            }

            // Record the migration
            query.prepare("INSERT INTO schema_version (version, description) VALUES (:version, :desc)");
            query.bindValue(":version", version);
            query.bindValue(":desc", name);
            if (!query.exec()) {
                spdlog::error("Failed to record migration {}: {}",
                              version, query.lastError().text().toStdString());
                return false;
            }

            spdlog::info("Migration {} applied successfully.", version);
        }

        spdlog::info("Migrations complete.");
        return true;
    }

private:
    int getCurrentVersion(QSqlDatabase &db)
    {
        QSqlQuery query(db);
        query.exec("SELECT MAX(version) FROM schema_version");
        if (query.next() && !query.value(0).isNull()) {
            return query.value(0).toInt();
        }
        return 0;
    }

    QString m_migrationsDir;
};
