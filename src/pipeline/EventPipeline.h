#pragma once

#include <QObject>
#include <QList>
#include <memory>
#include "core/Event.h"
#include "core/Timeline.h"

class EventCollector;
class EventFilter;
class ActivityClassifier;
class TimelineAssembler;
class PipelineWorker;

/// Orchestrates the event processing pipeline: Collect → Filter → Classify → Assemble.
/// Receives raw events from monitors and produces structured timelines.
class EventPipeline : public QObject {
    Q_OBJECT

public:
    explicit EventPipeline(QObject *parent = nullptr);
    ~EventPipeline();

    /// Start the pipeline processing.
    bool start();

    /// Stop the pipeline gracefully.
    void stop();

    /// Get the current assembled timeline for today.
    Timeline currentTimeline() const;

    /// Get today's activity summary.
    ActivitySummary todaySummary() const;

    /// Associate subsequently processed events with the current app session.
    void setSessionId(const QString &sessionId);

    /// Load classification rules from JSON.
    void loadClassificationRules(const QByteArray &json);

    /// Load filter rules from JSON.
    void loadFilterRules(const QByteArray &json);

signals:
    /// Emitted when a batch of classified events is assembled into the timeline.
    void timelineUpdated(const Timeline &timeline);

    /// Emitted with today's summary (updated periodically).
    void summaryUpdated(const ActivitySummary &summary);

    /// Emitted after filtering/classification so the application can persist
    /// exactly the events that appear in the timeline.
    void eventsProcessed(const QList<ActivityEvent> &events);

    /// Emitted when the pipeline encounters an error.
    void pipelineError(const QString &error);

public slots:
    /// Receive a raw event from the monitor engine.
    void onRawEvent(const RawEvent &event);

    /// Process a batch of raw events.
    void onRawEventBatch(const QList<RawEvent> &events);

private:
    EventCollector *m_collector = nullptr;
    EventFilter *m_filter = nullptr;
    ActivityClassifier *m_classifier = nullptr;
    TimelineAssembler *m_assembler = nullptr;
    PipelineWorker *m_worker = nullptr;
    bool m_running = false;
};
