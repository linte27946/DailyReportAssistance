#include "PipelineWorker.h"
#include <spdlog/spdlog.h>

PipelineWorker::PipelineWorker(EventFilter *filter,
                               ActivityClassifier *classifier,
                               TimelineAssembler *assembler,
                               QObject *parent)
    : QObject(parent)
    , m_filter(filter)
    , m_classifier(classifier)
    , m_assembler(assembler)
{
}

void PipelineWorker::processBatch(const QList<RawEvent> &events)
{
    if (events.isEmpty()) return;

    try {
        // Stage 1: Filter
        QList<RawEvent> filtered = m_filter->filter(events);

        if (filtered.isEmpty()) return;

        // Stage 2: Classify
        QList<ActivityEvent> classified = m_classifier->classifyBatch(filtered);

        if (classified.isEmpty()) return;

        if (!m_sessionId.isEmpty()) {
            for (auto &event : classified) {
                if (event.sessionId.isEmpty())
                    event.sessionId = m_sessionId;
            }
        }

        emit eventsProcessed(classified);

        // Stage 3: Assemble into timeline
        m_assembler->addEvents(classified);

    } catch (const std::exception &e) {
        QString error = QString("Pipeline processing error: %1").arg(e.what());
        spdlog::error(error.toStdString());
        emit processingError(error);
    }
}
