#pragma once

#include <QObject>
#include <QString>
#include "core/Event.h"

/// Abstract interface for all activity monitors.
/// Each monitor runs on its own thread and emits rawEventCaptured signals
/// that are delivered via queued connections to the pipeline thread.
class IMonitor : public QObject {
    Q_OBJECT

public:
    explicit IMonitor(QObject *parent = nullptr) : QObject(parent) {}
    virtual ~IMonitor() = default;

    /// Start the monitor. Returns true on success.
    virtual bool start() = 0;

    /// Stop the monitor gracefully.
    virtual void stop() = 0;

    /// Human-readable name for logging and identification.
    virtual QString name() const = 0;

    /// Whether the monitor is currently running.
    bool isRunning() const { return m_running; }

signals:
    /// Emitted when a raw event is captured.
    void rawEventCaptured(const RawEvent &event);

    /// Emitted when the monitor encounters an error.
    void monitorError(const QString &monitorName, const QString &errorMessage);

protected:
    void setRunning(bool running) { m_running = running; }

private:
    bool m_running = false;
};
