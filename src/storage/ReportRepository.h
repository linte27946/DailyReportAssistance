#pragma once

#include <QString>
#include <QDate>
#include <QList>
#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <spdlog/spdlog.h>
#include "Database.h"

/// Record representing a stored report.
struct ReportRecord {
    int64_t id = 0;
    QString reportType;      // 'daily', 'weekly', 'custom'
    QDate reportDate;
    QString title;
    QString contentMd;        // Markdown content
    QString llmBackend;
    QString llmModel;
    double generationTimeSecs = 0;
    int tokenCount = 0;
    QDateTime createdAt;
};

/// CRUD operations for the reports table.
class ReportRepository {
public:
    ReportRepository() = default;

    /// Save a generated report to the database.
    /// Returns the new report ID, or -1 on failure.
    int64_t saveReport(const QString &reportType,
                       const QDate &reportDate,
                       const QString &title,
                       const QString &contentMd,
                       const QString &llmBackend,
                       const QString &llmModel,
                       double generationTimeSecs = 0,
                       int tokenCount = 0)
    {
        auto db = Database::instance().connection();
        QSqlQuery query(db);

        query.prepare(
            "INSERT INTO reports "
            "(report_type, report_date, title, content_md, llm_backend, llm_model, "
            " generation_time_secs, token_count) "
            "VALUES "
            "(:type, :date, :title, :content, :backend, :model, :gen_time, :tokens)"
        );
        query.bindValue(":type", reportType);
        query.bindValue(":date", reportDate.toString(Qt::ISODate));
        query.bindValue(":title", title);
        query.bindValue(":content", contentMd);
        query.bindValue(":backend", llmBackend);
        query.bindValue(":model", llmModel);
        query.bindValue(":gen_time", generationTimeSecs);
        query.bindValue(":tokens", tokenCount);

        if (!query.exec()) {
            spdlog::error("Failed to save report: {}",
                          query.lastError().text().toStdString());
            return -1;
        }

        int64_t id = query.lastInsertId().toLongLong();
        spdlog::info("Saved report (id={}) for {}", id,
                     reportDate.toString(Qt::ISODate).toStdString());
        return id;
    }

    /// Get a paginated list of reports.
    QList<ReportRecord> getReports(int page = 0, int pageSize = 20)
    {
        QList<ReportRecord> results;
        auto db = Database::instance().connection();
        QSqlQuery query(db);

        query.prepare(
            "SELECT id, report_type, report_date, title, content_md, "
            "       llm_backend, llm_model, generation_time_secs, token_count, created_at "
            "FROM reports "
            "ORDER BY report_date DESC, created_at DESC "
            "LIMIT :limit OFFSET :offset"
        );
        query.bindValue(":limit", pageSize);
        query.bindValue(":offset", page * pageSize);

        if (!query.exec()) {
            spdlog::error("Failed to query reports: {}",
                          query.lastError().text().toStdString());
            return results;
        }

        while (query.next()) {
            ReportRecord r;
            r.id = query.value("id").toLongLong();
            r.reportType = query.value("report_type").toString();
            r.reportDate = QDate::fromString(query.value("report_date").toString(), Qt::ISODate);
            r.title = query.value("title").toString();
            r.contentMd = query.value("content_md").toString();
            r.llmBackend = query.value("llm_backend").toString();
            r.llmModel = query.value("llm_model").toString();
            r.generationTimeSecs = query.value("generation_time_secs").toDouble();
            r.tokenCount = query.value("token_count").toInt();
            r.createdAt = QDateTime::fromString(query.value("created_at").toString(),
                                                Qt::ISODate);
            results.append(r);
        }

        return results;
    }

    /// Get a single report by ID.
    ReportRecord getReport(int64_t id)
    {
        ReportRecord r;
        auto db = Database::instance().connection();
        QSqlQuery query(db);

        query.prepare(
            "SELECT id, report_type, report_date, title, content_md, "
            "       llm_backend, llm_model, generation_time_secs, token_count, created_at "
            "FROM reports WHERE id = :id"
        );
        query.bindValue(":id", static_cast<qlonglong>(id));

        if (query.exec() && query.next()) {
            r.id = query.value("id").toLongLong();
            r.reportType = query.value("report_type").toString();
            r.reportDate = QDate::fromString(query.value("report_date").toString(), Qt::ISODate);
            r.title = query.value("title").toString();
            r.contentMd = query.value("content_md").toString();
            r.llmBackend = query.value("llm_backend").toString();
            r.llmModel = query.value("llm_model").toString();
            r.generationTimeSecs = query.value("generation_time_secs").toDouble();
            r.tokenCount = query.value("token_count").toInt();
            r.createdAt = QDateTime::fromString(query.value("created_at").toString(),
                                                Qt::ISODate);
        }
        return r;
    }

    /// Check if a report already exists for a given date and type.
    bool reportExists(const QDate &date, const QString &type)
    {
        auto db = Database::instance().connection();
        QSqlQuery query(db);
        query.prepare("SELECT COUNT(*) FROM reports WHERE report_date = :date AND report_type = :type");
        query.bindValue(":date", date.toString(Qt::ISODate));
        query.bindValue(":type", type);

        if (query.exec() && query.next()) {
            return query.value(0).toInt() > 0;
        }
        return false;
    }

    /// Delete a report by ID.
    bool deleteReport(int64_t id)
    {
        auto db = Database::instance().connection();
        QSqlQuery query(db);
        query.prepare("DELETE FROM reports WHERE id = :id");
        query.bindValue(":id", static_cast<qlonglong>(id));

        if (!query.exec()) {
            spdlog::error("Failed to delete report: {}",
                          query.lastError().text().toStdString());
            return false;
        }
        return true;
    }

    /// Delete reports whose covered date is older than the cutoff date.
    int pruneOlderThan(const QDate &cutoff)
    {
        auto db = Database::instance().connection();
        QSqlQuery query(db);
        query.prepare("DELETE FROM reports WHERE report_date < :cutoff");
        query.bindValue(":cutoff", cutoff.toString(Qt::ISODate));

        if (!query.exec()) {
            spdlog::error("Failed to prune reports: {}",
                          query.lastError().text().toStdString());
            return -1;
        }

        const int deleted = query.numRowsAffected();
        spdlog::info("Pruned {} reports older than {}.", deleted,
                     cutoff.toString(Qt::ISODate).toStdString());
        return deleted;
    }

    /// Get the total number of reports.
    int totalCount()
    {
        auto db = Database::instance().connection();
        QSqlQuery query(db);
        query.exec("SELECT COUNT(*) FROM reports");
        if (query.next())
            return query.value(0).toInt();
        return 0;
    }
};
