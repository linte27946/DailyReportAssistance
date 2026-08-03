#pragma once

#include <QList>
#include <QDate>
#include <QSqlQuery>
#include <QSqlError>
#include <spdlog/spdlog.h>
#include "Database.h"
#include "core/Event.h"
#include "core/Timeline.h"
#include "util/JsonUtils.h"

/// CRUD operations for activity_events table.
class EventRepository {
public:
    EventRepository() = default;

    /// Insert a batch of events into the database.
    bool insertBatch(const QList<ActivityEvent> &events)
    {
        if (events.isEmpty()) return true;

        auto db = Database::instance().connection();
        QSqlQuery query(db);

        query.prepare(
            "INSERT INTO activity_events "
            "(timestamp, end_timestamp, event_type, category, description, "
            " application, window_title, file_path, file_extension, url, "
            " duration_secs, metadata_json, session_id) "
            "VALUES "
            "(:timestamp, :end_timestamp, :event_type, :category, :description, "
            " :application, :window_title, :file_path, :file_extension, :url, "
            " :duration_secs, :metadata_json, :session_id)"
        );

        db.transaction();

        int inserted = 0;
        for (const auto &e : events) {
            query.bindValue(":timestamp", e.timestamp.toString(Qt::ISODateWithMs));
            query.bindValue(":end_timestamp",
                            e.endTimestamp.isValid()
                                ? e.endTimestamp.toString(Qt::ISODateWithMs)
                                : QVariant());
            query.bindValue(":event_type", eventTypeToString(e.type));
            query.bindValue(":category", eventCategoryToString(e.category));
            query.bindValue(":description", e.description);
            query.bindValue(":application", e.application);
            query.bindValue(":window_title", e.windowTitle);
            query.bindValue(":file_path", e.filePath);
            query.bindValue(":file_extension", e.fileExtension);
            query.bindValue(":url", e.url);
            query.bindValue(":duration_secs", e.durationSecs);
            query.bindValue(":metadata_json", JsonUtils::toString(e.metadata));
            query.bindValue(":session_id", e.sessionId);

            if (!query.exec()) {
                spdlog::error("Failed to insert event: {}",
                              query.lastError().text().toStdString());
                db.rollback();
                return false;
            }
            inserted++;
        }

        db.commit();
        spdlog::debug("Inserted {} events into database.", inserted);
        return true;
    }

    /// Query the timeline for a specific date.
    Timeline queryTimeline(const QDate &date)
    {
        return queryDateRange(date, date);
    }

    /// Query the timeline for a date range (inclusive).
    Timeline queryDateRange(const QDate &start, const QDate &end)
    {
        Timeline timeline;
        auto db = Database::instance().connection();
        QSqlQuery query(db);

        query.prepare(
            "SELECT id, timestamp, end_timestamp, event_type, category, description, "
            "       application, window_title, file_path, file_extension, url, "
            "       duration_secs, metadata_json, session_id "
            "FROM activity_events "
            "WHERE date(timestamp) BETWEEN :start AND :end "
            "ORDER BY timestamp ASC"
        );
        query.bindValue(":start", start.toString(Qt::ISODate));
        query.bindValue(":end", end.toString(Qt::ISODate));

        if (!query.exec()) {
            spdlog::error("Failed to query timeline: {}",
                          query.lastError().text().toStdString());
            return timeline;
        }

        while (query.next()) {
            ActivityEvent e;
            e.id = query.value("id").toLongLong();
            e.timestamp = QDateTime::fromString(query.value("timestamp").toString(),
                                                Qt::ISODateWithMs);
            QString endTs = query.value("end_timestamp").toString();
            if (!endTs.isEmpty())
                e.endTimestamp = QDateTime::fromString(endTs, Qt::ISODateWithMs);
            e.description = query.value("description").toString();
            e.application = query.value("application").toString();
            e.windowTitle = query.value("window_title").toString();
            e.filePath = query.value("file_path").toString();
            e.fileExtension = query.value("file_extension").toString();
            e.url = query.value("url").toString();
            e.durationSecs = query.value("duration_secs").toInt();
            e.sessionId = query.value("session_id").toString();
            e.metadata = QJsonDocument::fromJson(
                query.value("metadata_json").toString().toUtf8()).object();

            timeline.addEvent(e);
        }

        return timeline;
    }

    /// Get aggregated activity summary for a date.
    ActivitySummary getActivitySummary(const QDate &date)
    {
        Timeline timeline = queryTimeline(date);
        return timeline.computeSummary(date);
    }

    /// Delete events older than the given cutoff date.
    int pruneOlderThan(const QDate &cutoff)
    {
        auto db = Database::instance().connection();
        QSqlQuery query(db);

        query.prepare("DELETE FROM activity_events WHERE date(timestamp) < :cutoff");
        query.bindValue(":cutoff", cutoff.toString(Qt::ISODate));

        if (!query.exec()) {
            spdlog::error("Failed to prune events: {}",
                          query.lastError().text().toStdString());
            return -1;
        }

        int deleted = query.numRowsAffected();
        spdlog::info("Pruned {} events older than {}.", deleted,
                     cutoff.toString(Qt::ISODate).toStdString());
        return deleted;
    }

    /// Get the total event count.
    int64_t eventCount()
    {
        auto db = Database::instance().connection();
        QSqlQuery query(db);
        query.exec("SELECT COUNT(*) FROM activity_events");
        if (query.next())
            return query.value(0).toLongLong();
        return 0;
    }
};
