#include "InputActivityMonitor.h"
#include <QProcess>
#include <QStandardPaths>
#include <spdlog/spdlog.h>

#ifdef _WIN32
#include <windows.h>
#endif

InputActivityMonitor::InputActivityMonitor(QObject *parent)
    : IMonitor(parent)
{
    m_checkTimer = new QTimer(this);
    m_checkTimer->setInterval(kCheckIntervalMs);
    connect(m_checkTimer, &QTimer::timeout, this, &InputActivityMonitor::checkActivity);
}

InputActivityMonitor::~InputActivityMonitor()
{
    stop();
}

bool InputActivityMonitor::start()
{
    spdlog::info("InputActivityMonitor starting (AFK threshold: {}s)...", m_afkThresholdSecs);
#ifndef _WIN32
    if (QStandardPaths::findExecutable("xprintidle").isEmpty()) {
        const QString error = "xprintidle is not installed; idle tracking is unavailable.";
        spdlog::warn("InputActivityMonitor: {}", error.toStdString());
        emit monitorError(name(), error);
        return false;
    }
#endif
    m_isIdle = false;
    m_checkTimer->start();
    setRunning(true);
    return true;
}

void InputActivityMonitor::stop()
{
    m_checkTimer->stop();

    // If we were idle, emit the end of idle
    if (m_isIdle) {
        RawEvent event;
        event.timestamp = QDateTime::currentDateTimeUtc();
        event.type = EventType::UserActive;
        event.source = "InputActivityMonitor";
        event.description = "User became active (monitor stopping)";
        emit rawEventCaptured(event);
    }

    setRunning(false);
    spdlog::info("InputActivityMonitor stopped.");
}

void InputActivityMonitor::checkActivity()
{
    int idleSecs = getIdleTimeSecs();

    if (!m_isIdle && idleSecs >= m_afkThresholdSecs) {
        // Transition to idle
        m_isIdle = true;
        m_idleStartTime = QDateTime::currentDateTimeUtc().addSecs(-idleSecs);

        RawEvent event;
        event.timestamp = QDateTime::currentDateTimeUtc();
        event.type = EventType::UserIdle;
        event.source = "InputActivityMonitor";
        event.description = QString("User idle (AFK for %1+ seconds)").arg(idleSecs);
        event.metadata["idleSecs"] = idleSecs;
        event.metadata["idleStartTime"] = m_idleStartTime.toString(Qt::ISODateWithMs);

        spdlog::debug("User became idle ({} secs)", idleSecs);
        emit rawEventCaptured(event);

    } else if (m_isIdle && idleSecs < m_afkThresholdSecs) {
        // Transition back to active
        m_isIdle = false;

        RawEvent event;
        event.timestamp = QDateTime::currentDateTimeUtc();
        event.type = EventType::UserActive;
        event.source = "InputActivityMonitor";
        event.description = "User became active";
        event.metadata["idleStartTime"] = m_idleStartTime.toString(Qt::ISODateWithMs);

        // Calculate idle duration
        int idleDuration = m_idleStartTime.secsTo(event.timestamp);
        event.metadata["idleDurationSecs"] = idleDuration;

        spdlog::debug("User became active (was idle for {} secs)", idleDuration);
        emit rawEventCaptured(event);
    }
}

int InputActivityMonitor::getIdleTimeSecs()
{
#ifdef _WIN32
    LASTINPUTINFO lii;
    lii.cbSize = sizeof(LASTINPUTINFO);
    if (GetLastInputInfo(&lii)) {
        DWORD tickCount = GetTickCount();
        DWORD idleMs = tickCount - lii.dwTime;
        return static_cast<int>(idleMs / 1000);
    }
    return 0;
#else
    const QString program = QStandardPaths::findExecutable("xprintidle");
    if (program.isEmpty()) return -1;
    QProcess process;
    process.start(program);
    if (!process.waitForFinished(1000)
        || process.exitStatus() != QProcess::NormalExit
        || process.exitCode() != 0) {
        return -1;
    }
    bool ok = false;
    const qint64 idleMs = process.readAllStandardOutput().trimmed().toLongLong(&ok);
    return ok ? static_cast<int>(idleMs / 1000) : -1;
#endif
}
