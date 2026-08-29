#pragma once

#include <QObject>
#include <QThread>
#include "EventCollector.h"
#include "EventFilter.h"
#include "ActivityClassifier.h"
#include "TimelineAssembler.h"

/// Worker object that runs the filter → classify → assemble loop on a dedicated thread.
class PipelineWorker : public QObject {
    Q_OBJECT

public:
    explicit PipelineWorker(EventFilter *filter,
                            ActivityClassifier *classifier,
                            TimelineAssembler *assembler,
                            QObject *parent = nullptr);

    void setSessionId(const QString &sessionId) { m_sessionId = sessionId; }

public slots:
    /// Process a batch of raw events through the pipeline.
    void processBatch(const QList<RawEvent> &events);

signals:
    void timelineUpdated(const Timeline &timeline);
    void eventsProcessed(const QList<ActivityEvent> &events);
    void processingError(const QString &error);

private:
    EventFilter *m_filter = nullptr;
    ActivityClassifier *m_classifier = nullptr;
    TimelineAssembler *m_assembler = nullptr;
    QString m_sessionId;
};
