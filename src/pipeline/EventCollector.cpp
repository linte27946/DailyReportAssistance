#include "EventCollector.h"
#include <spdlog/spdlog.h>

EventCollector::EventCollector(QObject *parent)
    : QObject(parent)
{
    m_flushTimer = new QTimer(this);
    m_flushTimer->setInterval(m_batchIntervalMs);
    connect(m_flushTimer, &QTimer::timeout, this, &EventCollector::flushBatch);
    m_flushTimer->start();
}

void EventCollector::collectEvent(const RawEvent &event)
{
    QMutexLocker lock(&m_mutex);
    m_buffer.append(event);

    if (m_buffer.size() >= m_batchSize) {
        // Flush immediately when batch size is reached
        // Use QMetaObject::invokeMethod to avoid deadlock
        QMetaObject::invokeMethod(this, &EventCollector::flushBatch, Qt::QueuedConnection);
    }
}

void EventCollector::flushBatch()
{
    QList<RawEvent> batch;
    {
        QMutexLocker lock(&m_mutex);
        if (m_buffer.isEmpty()) return;
        batch.swap(m_buffer);
    }

    if (!batch.isEmpty()) {
        spdlog::debug("EventCollector: flushing batch of {} events", batch.size());
        emit batchReady(batch);
    }
}

int EventCollector::pendingCount()
{
    QMutexLocker lock(&m_mutex);
    return m_buffer.size();
}
