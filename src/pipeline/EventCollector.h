#pragma once

#include <QObject>
#include <QList>
#include <QTimer>
#include <QMutex>
#include <QMutexLocker>
#include "core/Event.h"

/// Collects raw events into batches for downstream processing.
/// Uses a mutex-protected queue and emits batches on a timer.
class EventCollector : public QObject {
    Q_OBJECT

public:
    explicit EventCollector(QObject *parent = nullptr);

    void start();
    void stop(bool flushPending = true);

    /// Set batch parameters.
    void setBatchInterval(int ms) { m_batchIntervalMs = ms; }
    void setBatchSize(int size) { m_batchSize = size; }

    /// Number of events currently in the buffer.
    int pendingCount();

signals:
    void batchReady(const QList<RawEvent> &events);

public slots:
    void collectEvent(const RawEvent &event);

public slots:
    void flushBatch();

private:
    QList<RawEvent> m_buffer;
    QMutex m_mutex;
    QTimer *m_flushTimer = nullptr;
    int m_batchIntervalMs = 1000;   // Flush every 1 second
    int m_batchSize = 50;            // Or when 50 events accumulate
};
