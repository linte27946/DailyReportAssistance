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
    const QDateTime now = QDateTime::currentDateTimeUtc();
    const int idleSecs = getIdleTimeSecs();
    m_isIdle = idleSecs >= m_afkThresholdSecs;
    if (m_isIdle)
        m_idleStartTime = now.addSecs(-idleSecs);
    m_lastStateEmissionDate = now.toLocalTime().date();
    m_checkTimer->start();
    setRunning(true);
    emitStateSnapshot(now, "monitor started");
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
        event.metadata["idleStartTime"] = m_idleStartTime.toString(Qt::ISODateWithMs);
        event.metadata["idleDurationSecs"] = m_idleStartTime.secsTo(event.timestamp);
        event.metadata["monitorStopping"] = true;
        emit rawEventCaptured(event);
    }

    setRunning(false);
    spdlog::info("InputActivityMonitor stopped.");
}

void InputActivityMonitor::checkActivity()
{
    const QDateTime now = QDateTime::currentDateTimeUtc();
    const QDate localDate = now.toLocalTime().date();
    const bool dateChanged = m_lastStateEmissionDate.isValid()
        && localDate != m_lastStateEmissionDate;
    int idleSecs = getIdleTimeSecs();
    bool stateChanged = false;

    if (!m_isIdle && idleSecs >= m_afkThresholdSecs) {
        // Transition to idle
        m_isIdle = true;
        m_idleStartTime = now.addSecs(-idleSecs);

        RawEvent event;
        event.timestamp = now;
        event.type = EventType::UserIdle;
        event.source = "InputActivityMonitor";
        event.description = QString("User idle (AFK for %1+ seconds)").arg(idleSecs);
        event.metadata["idleSecs"] = idleSecs;
        event.metadata["idleStartTime"] = m_idleStartTime.toString(Qt::ISODateWithMs);

        spdlog::debug("User became idle ({} secs)", idleSecs);
        emit rawEventCaptured(event);
        stateChanged = true;

    } else if (m_isIdle && idleSecs < m_afkThresholdSecs) {
        // Transition back to active
        m_isIdle = false;

        RawEvent event;
        event.timestamp = now;
        event.type = EventType::UserActive;
        event.source = "InputActivityMonitor";
        event.description = "User became active";
        event.metadata["idleStartTime"] = m_idleStartTime.toString(Qt::ISODateWithMs);

        // Calculate idle duration
        int idleDuration = m_idleStartTime.secsTo(event.timestamp);
        event.metadata["idleDurationSecs"] = idleDuration;

        spdlog::debug("User became active (was idle for {} secs)", idleDuration);
        emit rawEventCaptured(event);
        stateChanged = true;
    }

    // Persist the current state just after local midnight. This gives each
    // date an explicit boundary even when the app runs for days without an
    // active/idle transition at midnight.
    if (dateChanged && !stateChanged)
        emitStateSnapshot(now, "date boundary");
    m_lastStateEmissionDate = localDate;
}

void InputActivityMonitor::emitStateSnapshot(const QDateTime &timestamp,
                                             const QString &reason)
{
    RawEvent event;
    event.timestamp = timestamp;
    event.type = m_isIdle ? EventType::UserIdle : EventType::UserActive;
    event.source = "InputActivityMonitor";
    event.description = m_isIdle
        ? QString("User idle state snapshot (%1)").arg(reason)
        : QString("User active state snapshot (%1)").arg(reason);
    event.metadata["stateSnapshot"] = true;
    event.metadata["reason"] = reason;
    if (m_isIdle && m_idleStartTime.isValid())
        event.metadata["idleStartTime"] = m_idleStartTime.toString(Qt::ISODateWithMs);
    emit rawEventCaptured(event);
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
