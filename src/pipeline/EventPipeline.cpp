#include "EventPipeline.h"
#include "EventCollector.h"
#include "EventFilter.h"
#include "ActivityClassifier.h"
#include "TimelineAssembler.h"
#include "PipelineWorker.h"
#include <spdlog/spdlog.h>

EventPipeline::EventPipeline(QObject *parent)
    : QObject(parent)
{
    // Create pipeline stages
    m_collector = new EventCollector(this);
    m_filter = new EventFilter(this);
    m_classifier = new ActivityClassifier(this);
    m_assembler = new TimelineAssembler(this);

    // Classification is deliberately kept on this object's thread. Monitors
    // already run independently, while these small in-memory batches complete
    // quickly and avoiding mixed QObject thread affinity makes shutdown safe.
    m_worker = new PipelineWorker(m_filter, m_classifier, m_assembler, this);

    // Connect collector batch → worker thread (queued connection)
    connect(m_collector, &EventCollector::batchReady,
            m_worker, &PipelineWorker::processBatch,
            Qt::AutoConnection);

    // Forward timeline updates from assembler
    connect(m_assembler, &TimelineAssembler::timelineUpdated,
            this, &EventPipeline::timelineUpdated,
            Qt::AutoConnection);

    connect(m_assembler, &TimelineAssembler::timelineUpdated,
            this, [this](const Timeline &) {
                emit summaryUpdated(m_assembler->todaySummary());
            });

    connect(m_worker, &PipelineWorker::eventsProcessed,
            this, &EventPipeline::eventsProcessed);

    // Forward errors
    connect(m_worker, &PipelineWorker::processingError,
            this, &EventPipeline::pipelineError,
            Qt::AutoConnection);
}

EventPipeline::~EventPipeline()
{
    stop();
}

bool EventPipeline::start()
{
    if (m_running) return true;
    spdlog::info("EventPipeline starting...");
    m_collector->start();
    m_running = true;
    spdlog::info("EventPipeline started.");
    return true;
}

void EventPipeline::stop()
{
    if (!m_running) return;
    spdlog::info("EventPipeline stopping...");
    m_collector->stop(true);
    m_running = false;
    spdlog::info("EventPipeline stopped.");
}

Timeline EventPipeline::currentTimeline() const
{
    return m_assembler->todayTimeline();
}

ActivitySummary EventPipeline::todaySummary() const
{
    return m_assembler->todaySummary();
}

void EventPipeline::onRawEvent(const RawEvent &event)
{
    if (!m_running) return;
    m_collector->collectEvent(event);
}

void EventPipeline::onRawEventBatch(const QList<RawEvent> &events)
{
    if (!m_running) return;
    for (const auto &e : events) {
        m_collector->collectEvent(e);
    }
}

void EventPipeline::setSessionId(const QString &sessionId)
{
    m_worker->setSessionId(sessionId);
}

void EventPipeline::loadClassificationRules(const QByteArray &json)
{
    QJsonDocument doc = QJsonDocument::fromJson(json);
    if (doc.isObject()) {
        m_classifier->loadRules(doc.object());
        spdlog::info("EventPipeline: Loaded classification rules.");
    }
}

void EventPipeline::loadFilterRules(const QByteArray &json)
{
    QJsonDocument doc = QJsonDocument::fromJson(json);
    if (doc.isObject()) {
        m_filter->loadConfig(doc.object());
        spdlog::info("EventPipeline: Loaded filter rules.");
    }
}
