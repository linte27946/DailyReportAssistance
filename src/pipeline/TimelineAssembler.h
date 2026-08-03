#pragma once

#include <QObject>
#include <QMutex>
#include "core/Event.h"
#include "core/Timeline.h"

/// Assembles classified ActivityEvents into a coherent timeline.
/// Performs coalescing (merging contiguous same-category events) and sorting.
class TimelineAssembler : public QObject {
    Q_OBJECT

public:
    explicit TimelineAssembler(QObject *parent = nullptr);

    /// Add a batch of classified events to the timeline.
    void addEvents(const QList<ActivityEvent> &events);

    /// Get the current timeline snapshot.
    Timeline timeline() const;

    /// Get today's timeline.
    Timeline todayTimeline() const;

    /// Get timeline for a specific date.
    Timeline timelineForDate(const QDate &date) const;

    /// Get today's summary.
    ActivitySummary todaySummary() const;

    /// Coalesce consecutive events of the same category.
    static Timeline coalesce(const Timeline &timeline, int maxGapSecs = 300);

    /// Clear old data (events before a given date).
    void pruneBefore(const QDate &date);

signals:
    void timelineUpdated(const Timeline &updatedTimeline);

private:
    Timeline m_timeline;
    mutable QMutex m_mutex;
};
