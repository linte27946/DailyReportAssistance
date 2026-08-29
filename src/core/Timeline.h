#pragma once

#include <QList>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <algorithm>
#include "Event.h"

/// An ordered container of ActivityEvent objects representing a chronological timeline.
/// Supports insertion, sorting, filtering, and coalescing of events.
class Timeline {
public:
    Timeline() = default;

    void addEvent(const ActivityEvent &event) { m_events.append(event); }
    void addEvents(const QList<ActivityEvent> &events) { m_events.append(events); }
    void clear() { m_events.clear(); }
    bool isEmpty() const { return m_events.isEmpty(); }
    int count() const { return m_events.size(); }

    const QList<ActivityEvent> &events() const { return m_events; }
    QList<ActivityEvent> &events() { return m_events; }

    /// Sort events by timestamp ascending.
    void sort()
    {
        std::sort(m_events.begin(), m_events.end(),
                  [](const ActivityEvent &a, const ActivityEvent &b) {
                      return a.timestamp < b.timestamp;
                  });
    }

    /// Filter events by date.
    Timeline forDate(const QDate &date) const
    {
        Timeline result;
        for (const auto &e : m_events) {
            if (e.timestamp.toLocalTime().date() == date)
                result.addEvent(e);
        }
        return result;
    }

    /// Filter events by date range (inclusive).
    Timeline forDateRange(const QDate &start, const QDate &end) const
    {
        Timeline result;
        QDateTime startDt(start, QTime(0, 0, 0), Qt::LocalTime);
        QDateTime endDt(end.addDays(1), QTime(0, 0, 0), Qt::LocalTime);
        startDt = startDt.toUTC();
        endDt = endDt.toUTC();
        for (const auto &e : m_events) {
            if (e.timestamp >= startDt && e.timestamp < endDt)
                result.addEvent(e);
        }
        return result;
    }

    /// Filter events by category.
    Timeline forCategory(EventCategory cat) const
    {
        Timeline result;
        for (const auto &e : m_events) {
            if (e.category == cat)
                result.addEvent(e);
        }
        return result;
    }

    /// Build an ActivitySummary from this timeline.
    ActivitySummary computeSummary(const QDate &date) const
    {
        ActivitySummary summary;
        summary.date = date;

        Timeline dayTimeline = forDate(date);

        dayTimeline.sort();

        // Focus-change events are point-in-time observations. Estimate each
        // focused window's duration from the following focus event, capped to
        // avoid counting long sleep/offline gaps as active work.
        QMap<int, int> estimatedDurations;
        int previousFocusIndex = -1;
        for (int i = 0; i < dayTimeline.events().size(); ++i) {
            const auto &event = dayTimeline.events()[i];
            if (event.type != EventType::WindowFocusChanged) continue;
            if (previousFocusIndex >= 0) {
                const auto &previous = dayTimeline.events()[previousFocusIndex];
                const qint64 seconds = previous.timestamp.secsTo(event.timestamp);
                if (seconds > 0)
                    estimatedDurations[previousFocusIndex] = static_cast<int>(qMin<qint64>(seconds, 1800));
            }
            previousFocusIndex = i;
        }
        if (previousFocusIndex >= 0) {
            const auto &previous = dayTimeline.events()[previousFocusIndex];
            const QDateTime periodEnd = date == QDate::currentDate()
                ? QDateTime::currentDateTimeUtc()
                : QDateTime(date.addDays(1), QTime(0, 0), Qt::LocalTime).toUTC();
            const qint64 seconds = previous.timestamp.secsTo(periodEnd);
            if (seconds > 0)
                estimatedDurations[previousFocusIndex] = static_cast<int>(qMin<qint64>(seconds, 1800));
        }

        for (int i = 0; i < dayTimeline.events().size(); ++i) {
            const auto &e = dayTimeline.events()[i];
            const int durationSecs = e.durationSecs > 0
                ? e.durationSecs
                : estimatedDurations.value(i, 0);
            summary.categoryDurationSecs[e.category] += durationSecs;

            switch (e.category) {
            case EventCategory::Coding:
                if (e.type == EventType::FileModified
                    || e.type == EventType::FileCreated
                    || e.type == EventType::FileRenamed) {
                    summary.fileEditCount++;
                }
                break;
            case EventCategory::VersionControl:
                if (e.type == EventType::GitCommit) summary.gitCommitCount++;
                break;
            case EventCategory::Building:
                if (e.type == EventType::BuildStarted) summary.buildCount++;
                if (e.type == EventType::BuildCompleted) {
                    if (e.metadata.contains("success") && !e.metadata["success"].toBool())
                        summary.buildFailureCount++;
                }
                break;
            case EventCategory::Idle:
                summary.totalIdleSecs += durationSecs;
                break;
            default:
                break;
            }

            if (e.type == EventType::UserActive
                && e.metadata.contains("idleDurationSecs")) {
                const int idleDuration = e.metadata["idleDurationSecs"].toInt();
                summary.totalIdleSecs += idleDuration;
                summary.categoryDurationSecs[EventCategory::Idle] += idleDuration;
            } else if (e.category != EventCategory::Idle) {
                summary.totalActiveSecs += durationSecs;
            }
        }

        // Gather top files
        QMap<QString, int> fileCounts;
        for (const auto &e : dayTimeline.events()) {
            if (!e.filePath.isEmpty() && e.category == EventCategory::Coding)
                fileCounts[e.filePath]++;
        }
        QList<QPair<QString, int>> sortedFiles;
        for (auto it = fileCounts.begin(); it != fileCounts.end(); ++it)
            sortedFiles.append({it.key(), it.value()});
        std::sort(sortedFiles.begin(), sortedFiles.end(),
                  [](const auto &a, const auto &b) { return a.second > b.second; });
        for (int i = 0; i < qMin(10, sortedFiles.size()); ++i)
            summary.topFiles.append(sortedFiles[i].first);

        // Gather top applications
        QMap<QString, int> appCounts;
        for (const auto &e : dayTimeline.events()) {
            if (!e.application.isEmpty())
                appCounts[e.application]++;
        }
        QList<QPair<QString, int>> sortedApps;
        for (auto it = appCounts.begin(); it != appCounts.end(); ++it)
            sortedApps.append({it.key(), it.value()});
        std::sort(sortedApps.begin(), sortedApps.end(),
                  [](const auto &a, const auto &b) { return a.second > b.second; });
        for (int i = 0; i < qMin(5, sortedApps.size()); ++i)
            summary.topApplications.append(sortedApps[i].first);

        return summary;
    }

    QJsonArray toJson() const
    {
        QJsonArray arr;
        for (const auto &e : m_events)
            arr.append(e.toJson());
        return arr;
    }

private:
    QList<ActivityEvent> m_events;
};

Q_DECLARE_METATYPE(Timeline)
