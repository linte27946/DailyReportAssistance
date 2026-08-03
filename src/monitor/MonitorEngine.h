#pragma once

#include <QObject>
#include <QThread>
#include <vector>
#include <memory>
#include "IMonitor.h"

/// Manages the lifecycle of all monitors.
/// Owns monitors and their threads, and handles start/stop coordination.
class MonitorEngine : public QObject {
    Q_OBJECT

public:
    explicit MonitorEngine(QObject *parent = nullptr);
    ~MonitorEngine();

    /// Register a monitor with its own dedicated thread.
    /// Takes ownership of the monitor. The engine will manage the thread lifecycle.
    void registerMonitor(std::unique_ptr<IMonitor> monitor);

    /// Start all registered monitors.
    /// Returns true if all monitors started successfully.
    bool startAll();

    /// Stop all monitors gracefully.
    void stopAll();

    /// Check if all monitors are running.
    bool isRunning() const;

    /// Get the count of registered monitors.
    int monitorCount() const { return m_monitors.size(); }

    /// Get a monitor by name.
    IMonitor *monitor(const QString &name) const;

signals:
    /// Relayed from any monitor.
    void rawEventCaptured(const RawEvent &event);

    /// Emitted when any monitor encounters an error.
    void engineError(const QString &monitorName, const QString &errorMessage);

    /// Emitted when all monitors have started.
    void allMonitorsStarted();

    /// Emitted when all monitors have stopped.
    void allMonitorsStopped();

private:
    struct MonitorEntry {
        std::unique_ptr<IMonitor> monitor;
        QThread *thread = nullptr;
    };

    std::vector<MonitorEntry> m_monitors;
    bool m_running = false;
};
