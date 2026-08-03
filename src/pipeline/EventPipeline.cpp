#include "EventPipeline.h"
#include "EventCollector.h"
#include "EventFilter.h"
#include "ActivityClassifier.h"
#include "TimelineAssembler.h"
#include "PipelineWorker.h"
#include <QThread>
#include <spdlog/spdlog.h>

EventPipeline::EventPipeline(QObject *parent)
    : QObject(parent)
{
    // Create pipeline stages
    m_collector = new EventCollector(this);
    m_filter = new EventFilter(this);
    m_classifier = new ActivityClassifier(this);
    m_assembler = new TimelineAssembler(this);

    // Create worker thread
    m_workerThread = new QThread(this);
    m_worker = new PipelineWorker(m_filter, m_classifier, m_assembler);
    m_worker->moveToThread(m_workerThread);

    // Connect collector batch → worker thread (queued connection)
    connect(m_collector, &EventCollector::batchReady,
            m_worker, &PipelineWorker::processBatch,
            Qt::QueuedConnection);

    // Forward timeline updates from assembler
    connect(m_assembler, &TimelineAssembler::timelineUpdated,
            this, &EventPipeline::timelineUpdated,
            Qt::QueuedConnection);

    // Forward errors
    connect(m_worker, &PipelineWorker::processingError,
            this, &EventPipeline::pipelineError,
            Qt::QueuedConnection);
}

EventPipeline::~EventPipeline()
{
    stop();
}

bool EventPipeline::start()
{
    spdlog::info("EventPipeline starting...");
    m_workerThread->start();
    spdlog::info("EventPipeline started.");
    return true;
}

void EventPipeline::stop()
{
    spdlog::info("EventPipeline stopping...");

    if (m_workerThread->isRunning()) {
        m_workerThread->quit();
        m_workerThread->wait(5000);
    }

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
    m_collector->collectEvent(event);
}

void EventPipeline::onRawEventBatch(const QList<RawEvent> &events)
{
    for (const auto &e : events) {
        m_collector->collectEvent(e);
    }
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
