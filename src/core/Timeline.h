#pragma once

#include <QList>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QMap>
#include <algorithm>
#include <climits>
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
        const QDateTime start = QDateTime(
            date, QTime(0, 0), Qt::LocalTime).toUTC();
        const QDateTime end = QDateTime(
            date.addDays(1), QTime(0, 0), Qt::LocalTime).toUTC();
        for (const auto &e : m_events) {
            const QDateTime eventEnd = e.endTimestamp.isValid()
                ? e.endTimestamp : e.timestamp;
            if (e.timestamp < end && eventEnd >= start)
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
            const QDateTime eventEnd = e.endTimestamp.isValid()
                ? e.endTimestamp : e.timestamp;
            if (e.timestamp < endDt && eventEnd >= startDt)
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

        using Span = QPair<QDateTime, QDateTime>;
        struct FocusSpan {
            QDateTime start;
            QDateTime end;
            EventCategory category = EventCategory::Other;
        };
        struct StateChange {
            QDateTime timestamp;
            bool active = false;
            int priority = 0;
        };

        const QDateTime dayStart = QDateTime(
            date, QTime(0, 0), Qt::LocalTime).toUTC();
        const QDateTime periodEnd = date == QDate::currentDate()
            ? QDateTime::currentDateTimeUtc()
            : QDateTime(date.addDays(1), QTime(0, 0), Qt::LocalTime).toUTC();

        auto clipped = [&](const QDateTime &start, const QDateTime &end) {
            return Span(qMax(start, dayStart), qMin(end, periodEnd));
        };
        auto normalize = [](QList<Span> spans) {
            spans.erase(std::remove_if(spans.begin(), spans.end(),
                                       [](const Span &span) {
                                           return !span.first.isValid()
                                               || !span.second.isValid()
                                               || span.first >= span.second;
                                       }),
                        spans.end());
            std::sort(spans.begin(), spans.end(),
                      [](const Span &a, const Span &b) {
                          return a.first < b.first;
                      });
            QList<Span> result;
            for (const Span &span : spans) {
                if (result.isEmpty() || span.first > result.last().second) {
                    result.append(span);
                } else if (span.second > result.last().second) {
                    result.last().second = span.second;
                }
            }
            return result;
        };
        auto spanSeconds = [](const QList<Span> &spans) {
            qint64 total = 0;
            for (const Span &span : spans)
                total += span.first.secsTo(span.second);
            return static_cast<int>(qMin<qint64>(total, INT_MAX));
        };
        auto intersections = [&](const QList<Span> &spans,
                                 const Span &candidate) {
            QList<Span> result;
            for (const Span &span : spans) {
                const QDateTime start = qMax(span.first, candidate.first);
                const QDateTime end = qMin(span.second, candidate.second);
                if (start < end) result.append({start, end});
            }
            return normalize(result);
        };
        auto subtract = [&](QList<Span> source, const QList<Span> &blocked) {
            source = normalize(source);
            const QList<Span> blockers = normalize(blocked);
            QList<Span> result;
            for (const Span &span : source) {
                QList<Span> pieces{span};
                for (const Span &blocker : blockers) {
                    QList<Span> next;
                    for (const Span &piece : pieces) {
                        if (blocker.second <= piece.first
                            || blocker.first >= piece.second) {
                            next.append(piece);
                            continue;
                        }
                        if (blocker.first > piece.first)
                            next.append({piece.first, blocker.first});
                        if (blocker.second < piece.second)
                            next.append({blocker.second, piece.second});
                    }
                    pieces = next;
                    if (pieces.isEmpty()) break;
                }
                result.append(pieces);
            }
            return normalize(result);
        };

        QMap<QString, QList<ActivityEvent>> eventsBySession;
        QString currentSession;
        QDateTime latestEventTime;
        for (const auto &event : dayTimeline.events()) {
            const QString session = event.sessionId.isEmpty()
                ? QStringLiteral("__legacy__") : event.sessionId;
            eventsBySession[session].append(event);
            if (!latestEventTime.isValid() || event.timestamp > latestEventTime) {
                latestEventTime = event.timestamp;
                currentSession = session;
            }
        }

        QList<Span> activeSpans;
        QList<Span> idleSpans;
        QList<FocusSpan> focusSpans;

        for (auto sessionIt = eventsBySession.cbegin();
             sessionIt != eventsBySession.cend(); ++sessionIt) {
            QList<ActivityEvent> sessionEvents = sessionIt.value();
            std::sort(sessionEvents.begin(), sessionEvents.end(),
                      [](const ActivityEvent &a, const ActivityEvent &b) {
                          return a.timestamp < b.timestamp;
                      });
            if (sessionEvents.isEmpty()) continue;

            QDateTime sessionEnd = sessionEvents.last().timestamp;
            bool hasExplicitEnd = false;
            for (const auto &event : sessionEvents) {
                if (event.type == EventType::SessionEnded) {
                    sessionEnd = qMin(sessionEnd, event.timestamp);
                    hasExplicitEnd = true;
                    break;
                }
            }
            if (!hasExplicitEnd && date == QDate::currentDate()
                && sessionIt.key() == currentSession) {
                sessionEnd = periodEnd;
            }
            sessionEnd = qMin(sessionEnd, periodEnd);

            QList<StateChange> changes;
            for (const auto &event : sessionEvents) {
                if (event.type == EventType::UserActive) {
                    const QDateTime idleStart = QDateTime::fromString(
                        event.metadata.value("idleStartTime").toString(),
                        Qt::ISODateWithMs);
                    if (idleStart.isValid() && idleStart < event.timestamp)
                        changes.append({idleStart, false, 2});
                    changes.append({event.timestamp, true, 1});
                } else if (event.type == EventType::UserIdle) {
                    QDateTime idleStart = QDateTime::fromString(
                        event.metadata.value("idleStartTime").toString(),
                        Qt::ISODateWithMs);
                    if (!idleStart.isValid()) idleStart = event.timestamp;
                    changes.append({idleStart, false, 2});
                } else if (event.type == EventType::SessionEnded) {
                    changes.append({event.timestamp, false, 3});
                }
            }
            std::sort(changes.begin(), changes.end(),
                      [](const StateChange &a, const StateChange &b) {
                          if (a.timestamp == b.timestamp)
                              return a.priority < b.priority;
                          return a.timestamp < b.timestamp;
                      });

            if (!changes.isEmpty()) {
                bool active = changes.first().active;
                QDateTime cursor = qMax(changes.first().timestamp, dayStart);
                for (int i = 1; i < changes.size(); ++i) {
                    const QDateTime boundary = qBound(
                        dayStart, changes.at(i).timestamp, sessionEnd);
                    if (boundary > cursor) {
                        (active ? activeSpans : idleSpans)
                            .append({cursor, boundary});
                    }
                    active = changes.at(i).active;
                    cursor = qMax(cursor, boundary);
                }
                if (sessionEnd > cursor)
                    (active ? activeSpans : idleSpans)
                        .append({cursor, sessionEnd});
            }

            QList<ActivityEvent> focusEvents;
            for (const auto &event : sessionEvents) {
                // A URL observation is emitted only for the foreground
                // browser. Treat it as a more specific focus transition so
                // documentation and opted-in entertainment time do not stay
                // hidden inside the generic Browsing category.
                if (event.type == EventType::WindowFocusChanged
                    || (event.type == EventType::UrlVisited
                        && event.category != EventCategory::Unknown
                        && event.category != EventCategory::Other)) {
                    focusEvents.append(event);
                }
            }
            for (int i = 0; i < focusEvents.size(); ++i) {
                const QDateTime end = i + 1 < focusEvents.size()
                    ? focusEvents.at(i + 1).timestamp : sessionEnd;
                const Span span = clipped(focusEvents.at(i).timestamp, end);
                if (span.first < span.second) {
                    focusSpans.append(
                        {span.first, span.second, focusEvents.at(i).category});
                }
            }
        }

        activeSpans = normalize(activeSpans);
        idleSpans = normalize(idleSpans);

        // Older databases may not have activity-state markers. In that case,
        // foreground focus is the safest fallback. Cap an unbounded final
        // observation so sleep/offline gaps cannot become work time.
        if (activeSpans.isEmpty()) {
            QList<Span> fallbackSpans;
            for (FocusSpan &focus : focusSpans) {
                if (focus.start.secsTo(focus.end) > 1800)
                    focus.end = focus.start.addSecs(1800);
                fallbackSpans.append({focus.start, focus.end});
            }
            activeSpans = normalize(fallbackSpans);
        }

        // A real foreground activity always wins over idle state from another
        // overlapping/stale session. Meetings may only fill the remaining
        // monitored idle time.
        idleSpans = subtract(idleSpans, activeSpans);
        QList<Span> monitoredSpans = activeSpans;
        monitoredSpans.append(idleSpans);
        monitoredSpans = normalize(monitoredSpans);

        QList<Span> meetingSpans;
        for (const auto &event : dayTimeline.events()) {
            if (event.type != EventType::MeetingAttended) continue;
            QDateTime end = event.endTimestamp;
            if (!end.isValid() && event.durationSecs > 0)
                end = event.timestamp.addSecs(event.durationSecs);
            const Span meeting = clipped(event.timestamp, end);
            if (meeting.first >= meeting.second) continue;

            const QList<Span> monitored = intersections(monitoredSpans, meeting);
            const QList<Span> idle = intersections(idleSpans, meeting);
            const int monitoredSecs = spanSeconds(monitored);
            const int idleSecs = spanSeconds(idle);
            const int threshold = qBound(
                1, event.metadata.value("idleThresholdPercent").toInt(30), 99);

            // Strictly greater than the configured threshold. Scheduled-only
            // meetings never reach this point because the importer only emits
            // events with actual enter and quit timestamps.
            if (monitoredSecs <= 0
                || static_cast<qint64>(idleSecs) * 100
                    <= static_cast<qint64>(monitoredSecs) * threshold) {
                continue;
            }

            meetingSpans.append(idle);
            summary.meetingCount++;
            const QString subject = event.metadata.value("subject").toString();
            summary.meetings.append(QString("%1-%2 · %3")
                .arg(meeting.first.toLocalTime().toString("HH:mm"),
                     meeting.second.toLocalTime().toString("HH:mm"),
                     subject.isEmpty() ? event.description : subject));
        }
        meetingSpans = normalize(meetingSpans);

        const int baseActiveSecs = spanSeconds(activeSpans);
        const int meetingSecs = spanSeconds(meetingSpans);
        summary.meetingDurationSecs = meetingSecs;
        summary.totalActiveSecs = baseActiveSecs + meetingSecs;
        summary.totalIdleSecs = qMax(0, spanSeconds(idleSpans) - meetingSecs);

        int categorizedActiveSecs = 0;
        for (const FocusSpan &focus : focusSpans) {
            for (const Span &active : activeSpans) {
                const QDateTime start = qMax(focus.start, active.first);
                const QDateTime end = qMin(focus.end, active.second);
                if (start >= end) continue;
                const int seconds = static_cast<int>(start.secsTo(end));
                const EventCategory category = focus.category == EventCategory::Idle
                    || focus.category == EventCategory::Unknown
                    ? EventCategory::Other : focus.category;
                summary.categoryDurationSecs[category] += seconds;
                categorizedActiveSecs += seconds;
            }
        }
        if (categorizedActiveSecs < baseActiveSecs) {
            summary.categoryDurationSecs[EventCategory::Other]
                += baseActiveSecs - categorizedActiveSecs;
        }
        if (meetingSecs > 0)
            summary.categoryDurationSecs[EventCategory::Meeting] += meetingSecs;

        // Duration-bearing process and build events often overlap each other
        // and foreground activity. They remain useful facts, but never add to
        // wall-clock work time.
        for (const auto &e : dayTimeline.events()) {
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
            default:
                break;
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
