#include "TimelineAssembler.h"
#include <spdlog/spdlog.h>

TimelineAssembler::TimelineAssembler(QObject *parent)
    : QObject(parent)
{
}

void TimelineAssembler::addEvents(const QList<ActivityEvent> &events)
{
    if (events.isEmpty()) return;

    {
        QMutexLocker lock(&m_mutex);
        m_timeline.addEvents(events);
        m_timeline.sort();
    }

    Timeline coalesced = coalesce(timeline());
    emit timelineUpdated(coalesced);
}

Timeline TimelineAssembler::timeline() const
{
    QMutexLocker lock(&m_mutex);
    return m_timeline;
}

Timeline TimelineAssembler::todayTimeline() const
{
    QMutexLocker lock(&m_mutex);
    return m_timeline.forDate(QDate::currentDate());
}

Timeline TimelineAssembler::timelineForDate(const QDate &date) const
{
    QMutexLocker lock(&m_mutex);
    return m_timeline.forDate(date);
}

ActivitySummary TimelineAssembler::todaySummary() const
{
    Timeline today = todayTimeline();
    return today.computeSummary(QDate::currentDate());
}

Timeline TimelineAssembler::coalesce(const Timeline &timeline, int maxGapSecs)
{
    Timeline result;
    const auto &events = timeline.events();
    if (events.isEmpty()) return result;

    ActivityEvent currentSpan = events.first();
    if (!currentSpan.endTimestamp.isValid())
        currentSpan.endTimestamp = currentSpan.timestamp;

    for (int i = 1; i < events.size(); ++i) {
        const auto &e = events[i];

        bool sameCategory = (e.category == currentSpan.category);
        int gapSecs = currentSpan.endTimestamp.secsTo(e.timestamp);

        if (sameCategory && gapSecs <= maxGapSecs && gapSecs >= 0) {
            // Coalesce: extend the current span
            currentSpan.endTimestamp = e.endTimestamp.isValid() ? e.endTimestamp : e.timestamp;
            currentSpan.durationSecs += e.durationSecs;
            currentSpan.durationSecs += gapSecs;

            // Merge metadata
            if (e.metadata.contains("fileCount")) {
                int newCount = currentSpan.metadata["fileCount"].toInt() +
                               e.metadata["fileCount"].toInt();
                currentSpan.metadata["fileCount"] = newCount;
            }

            currentSpan.description = currentSpan.description + "; " + e.description;
        } else {
            // End current span and start a new one
            result.addEvent(currentSpan);
            currentSpan = e;
            if (!currentSpan.endTimestamp.isValid())
                currentSpan.endTimestamp = currentSpan.timestamp;
        }
    }

    // Don't forget the last span
    result.addEvent(currentSpan);

    return result;
}

void TimelineAssembler::pruneBefore(const QDate &date)
{
    QMutexLocker lock(&m_mutex);
    Timeline pruned;
    QDateTime cutoff(date, QTime(0, 0), Qt::UTC);
    for (const auto &e : m_timeline.events()) {
        if (e.timestamp >= cutoff)
            pruned.addEvent(e);
    }
    m_timeline = pruned;
    spdlog::info("TimelineAssembler: pruned events before {}", date.toString(Qt::ISODate).toStdString());
}
