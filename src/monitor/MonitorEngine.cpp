#include "MonitorEngine.h"
#include <spdlog/spdlog.h>

MonitorEngine::MonitorEngine(QObject *parent)
    : QObject(parent)
{
}

MonitorEngine::~MonitorEngine()
{
    stopAll();
}

void MonitorEngine::registerMonitor(std::unique_ptr<IMonitor> monitor)
{
    if (!monitor) {
        spdlog::error("Attempted to register null monitor.");
        return;
    }

    spdlog::info("Registering monitor: {}", monitor->name().toStdString());

    MonitorEntry entry;
    entry.thread = new QThread(this);
    entry.monitor = std::move(monitor);

    // Move monitor to its dedicated thread
    entry.monitor->moveToThread(entry.thread);

    // Wire monitor signals to engine signals (queued connections)
    connect(entry.monitor.get(), &IMonitor::rawEventCaptured,
            this, &MonitorEngine::rawEventCaptured,
            Qt::QueuedConnection);

    connect(entry.monitor.get(), &IMonitor::monitorError,
            this, &MonitorEngine::engineError,
            Qt::QueuedConnection);

    // Start monitor when thread starts
    connect(entry.thread, &QThread::started,
            entry.monitor.get(), [this, name = entry.monitor->name()]() {
                spdlog::info("Monitor thread started: {}", name.toStdString());
            });

    // Cleanup thread when it finishes
    connect(entry.thread, &QThread::finished,
            entry.monitor.get(), [this, &entry]() {
                spdlog::info("Monitor thread finished: {}",
                             entry.monitor->name().toStdString());
            });

    m_monitors.push_back(std::move(entry));
}

bool MonitorEngine::startAll()
{
    if (m_running) {
        spdlog::warn("MonitorEngine is already running.");
        return true;
    }

    if (m_monitors.empty()) {
        spdlog::warn("No monitors registered.");
        m_running = true;
        return true;
    }

    spdlog::info("Starting {} monitors...", m_monitors.size());

    bool allStarted = true;
    for (auto &entry : m_monitors) {
        if (!entry.monitor->start()) {
            spdlog::error("Failed to start monitor: {}",
                          entry.monitor->name().toStdString());
            allStarted = false;
        }
    }

    if (allStarted) {
        // Start all monitor threads
        for (auto &entry : m_monitors) {
            entry.thread->start();
        }

        m_running = true;
        spdlog::info("All monitors started successfully.");
        emit allMonitorsStarted();
    }

    return allStarted;
}

void MonitorEngine::stopAll()
{
    if (!m_running) return;

    spdlog::info("Stopping all monitors...");

    // Signal stop to each monitor
    for (auto &entry : m_monitors) {
        entry.monitor->stop();
    }

    // Quit and wait for threads
    for (auto &entry : m_monitors) {
        if (entry.thread->isRunning()) {
            entry.thread->quit();
            entry.thread->wait(5000); // 5 second timeout
        }
    }

    m_running = false;
    spdlog::info("All monitors stopped.");
    emit allMonitorsStopped();
}

bool MonitorEngine::isRunning() const
{
    return m_running;
}

IMonitor *MonitorEngine::monitor(const QString &name) const
{
    for (const auto &entry : m_monitors) {
        if (entry.monitor->name() == name)
            return entry.monitor.get();
    }
    return nullptr;
}
