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

    if (m_running) {
        spdlog::error("Cannot register monitor while MonitorEngine is running.");
        emit engineError(monitor->name(), "Cannot register a monitor while monitoring is active.");
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

    const QString monitorName = entry.monitor->name();
    connect(entry.thread, &QThread::started, this, [monitorName]() {
        spdlog::info("Monitor thread started: {}", monitorName.toStdString());
    });
    connect(entry.thread, &QThread::finished, this, [monitorName]() {
        spdlog::info("Monitor thread finished: {}", monitorName.toStdString());
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
        m_running = false;
        return true;
    }

    spdlog::info("Starting {} monitors...", m_monitors.size());

    bool anyStarted = false;
    QThread *targetThread = thread();
    for (auto &entry : m_monitors) {
        if (entry.monitor->thread() != entry.thread)
            entry.monitor->moveToThread(entry.thread);
        entry.thread->start();

        bool started = false;
        const bool invoked = QMetaObject::invokeMethod(
            entry.monitor.get(),
            [&entry, &started]() { started = entry.monitor->start(); },
            Qt::BlockingQueuedConnection);

        if (!invoked || !started) {
            spdlog::error("Failed to start monitor: {}",
                          entry.monitor->name().toStdString());
            emit engineError(entry.monitor->name(), "Monitor failed to start on this platform.");
            QMetaObject::invokeMethod(
                entry.monitor.get(),
                [&entry, targetThread]() {
                    entry.monitor->moveToThread(targetThread);
                },
                Qt::BlockingQueuedConnection);
            entry.thread->quit();
            entry.thread->wait(5000);
        } else {
            anyStarted = true;
        }
    }

    m_running = anyStarted;
    if (anyStarted) {
        spdlog::info("Available monitors started successfully.");
        emit allMonitorsStarted();
    }

    return anyStarted;
}

void MonitorEngine::stopAll()
{
    if (!m_running) return;

    spdlog::info("Stopping all monitors...");

    QThread *targetThread = thread();

    // Stop in each monitor's own thread, then transfer ownership affinity back
    // to the engine thread before the worker event loop is shut down.
    for (auto &entry : m_monitors) {
        if (!entry.thread->isRunning()) continue;
        QMetaObject::invokeMethod(
            entry.monitor.get(),
            [&entry, targetThread]() {
                entry.monitor->stop();
                entry.monitor->moveToThread(targetThread);
            },
            Qt::BlockingQueuedConnection);
    }

    // Quit and wait for threads
    for (auto &entry : m_monitors) {
        if (entry.thread->isRunning()) {
            entry.thread->quit();
            if (!entry.thread->wait(5000)) {
                spdlog::error("Timed out waiting for monitor thread: {}",
                              entry.monitor->name().toStdString());
            }
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
