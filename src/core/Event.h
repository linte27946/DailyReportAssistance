#pragma once

#include <QString>
#include <QDateTime>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QDataStream>
#include <cstdint>
#include "EventType.h"

/// Low-level raw event captured directly from a monitor.
struct RawEvent {
    int64_t id = 0;
    QDateTime timestamp;       // UTC timestamp
    EventType type = EventType::Unknown;
    QString source;            // Monitor name (e.g., "FileSystemMonitor")
    QString description;       // Human-readable summary
    QString processName;       // Name of related process
    QString windowTitle;       // Active window title at event time
    QString filePath;          // Full path (file events)
    QString url;               // URL (browser events)
    QJsonObject metadata;      // Flexible extra data

    QJsonObject toJson() const
    {
        QJsonObject obj;
        obj["timestamp"] = timestamp.toString(Qt::ISODateWithMs);
        obj["type"] = eventTypeToString(type);
        obj["source"] = source;
        obj["description"] = description;
        obj["processName"] = processName;
        obj["windowTitle"] = windowTitle;
        obj["filePath"] = filePath;
        obj["url"] = url;
        obj["metadata"] = metadata;
        return obj;
    }

    static RawEvent fromJson(const QJsonObject &obj)
    {
        RawEvent e;
        e.timestamp = QDateTime::fromString(obj["timestamp"].toString(), Qt::ISODateWithMs);
        e.source = obj["source"].toString();
        e.description = obj["description"].toString();
        e.processName = obj["processName"].toString();
        e.windowTitle = obj["windowTitle"].toString();
        e.filePath = obj["filePath"].toString();
        e.url = obj["url"].toString();
        e.metadata = obj["metadata"].toObject();
        return e;
    }
};

/// Classified activity event — enriched with category and ready for timeline assembly.
struct ActivityEvent {
    int64_t id = 0;
    QDateTime timestamp;        // UTC timestamp
    QDateTime endTimestamp;     // For duration events (builds, sessions)
    EventType type = EventType::Unknown;
    EventCategory category = EventCategory::Unknown;
    QString description;        // Human-readable description
    QString application;       // Process name
    QString windowTitle;
    QString filePath;
    QString fileExtension;
    QString url;
    int durationSecs = 0;       // Duration in seconds (0 = instantaneous)
    QString sessionId;          // UUID linking events to a login session
    QJsonObject metadata;       // Flexible extra data (git hash, build status, etc.)

    QJsonObject toJson() const
    {
        QJsonObject obj;
        obj["timestamp"] = timestamp.toString(Qt::ISODateWithMs);
        if (endTimestamp.isValid())
            obj["endTimestamp"] = endTimestamp.toString(Qt::ISODateWithMs);
        obj["type"] = eventTypeToString(type);
        obj["category"] = eventCategoryToString(category);
        obj["description"] = description;
        obj["application"] = application;
        obj["windowTitle"] = windowTitle;
        obj["filePath"] = filePath;
        obj["fileExtension"] = fileExtension;
        obj["url"] = url;
        obj["durationSecs"] = durationSecs;
        obj["sessionId"] = sessionId;
        obj["metadata"] = metadata;
        return obj;
    }

    static ActivityEvent fromJson(const QJsonObject &obj)
    {
        ActivityEvent e;
        e.timestamp = QDateTime::fromString(obj["timestamp"].toString(), Qt::ISODateWithMs);
        if (obj.contains("endTimestamp"))
            e.endTimestamp = QDateTime::fromString(obj["endTimestamp"].toString(), Qt::ISODateWithMs);
        e.description = obj["description"].toString();
        e.application = obj["application"].toString();
        e.windowTitle = obj["windowTitle"].toString();
        e.filePath = obj["filePath"].toString();
        e.fileExtension = obj["fileExtension"].toString();
        e.url = obj["url"].toString();
        e.durationSecs = obj["durationSecs"].toInt();
        e.sessionId = obj["sessionId"].toString();
        e.metadata = obj["metadata"].toObject();
        return e;
    }
};

/// Summary statistics for a time period.
struct ActivitySummary {
    QDate date;
    QMap<EventCategory, int> categoryDurationSecs;  // Total seconds per category
    int totalActiveSecs = 0;                         // Total active (non-idle) seconds
    int totalIdleSecs = 0;                           // Total idle seconds
    int fileEditCount = 0;
    int gitCommitCount = 0;
    int buildCount = 0;
    int buildFailureCount = 0;
    QStringList topFiles;                             // Most edited files
    QStringList topApplications;                      // Most used applications

    double activeHours() const { return totalActiveSecs / 3600.0; }
    int categoryPercent(EventCategory cat) const
    {
        if (totalActiveSecs == 0) return 0;
        return (categoryDurationSecs.value(cat, 0) * 100) / totalActiveSecs;
    }

    QJsonObject toJson() const
    {
        QJsonObject obj;
        obj["date"] = date.toString(Qt::ISODate);
        obj["totalActiveSecs"] = totalActiveSecs;
        obj["totalIdleSecs"] = totalIdleSecs;
        obj["fileEditCount"] = fileEditCount;
        obj["gitCommitCount"] = gitCommitCount;
        obj["buildCount"] = buildCount;
        obj["buildFailureCount"] = buildFailureCount;

        QJsonObject catDurations;
        for (auto it = categoryDurationSecs.begin(); it != categoryDurationSecs.end(); ++it) {
            catDurations[eventCategoryToString(it.key())] = it.value();
        }
        obj["categoryDurationSecs"] = catDurations;

        QJsonArray files;
        for (const auto &f : topFiles) files.append(f);
        obj["topFiles"] = files;

        QJsonArray apps;
        for (const auto &a : topApplications) apps.append(a);
        obj["topApplications"] = apps;

        return obj;
    }
};
