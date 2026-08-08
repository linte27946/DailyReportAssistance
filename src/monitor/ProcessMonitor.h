#pragma once

#include "IMonitor.h"
#include <QTimer>
#include <QSet>
#include <QMap>

#ifdef _WIN32
#include <windows.h>
#include <Wbemidl.h>
#include <comdef.h>
#endif

/// Tracks process creation and termination using WMI polling.
/// Filters for developer-relevant processes only.
class ProcessMonitor : public IMonitor {
    Q_OBJECT

public:
    explicit ProcessMonitor(QObject *parent = nullptr);
    ~ProcessMonitor() override;

    bool start() override;
    void stop() override;
    QString name() const override { return "ProcessMonitor"; }

    /// Add process names to track (e.g., "devenv.exe", "code.exe").
    void addTrackedProcess(const QString &processName);
    void setTrackedProcesses(const QSet<QString> &processes);

    /// Get currently running tracked processes.
    QStringList getRunningTrackedProcesses();

private:
    void pollProcesses();
#ifdef _WIN32
    static QString getProcessPath(DWORD processId);
    static QString getProcessCommandLine(DWORD processId);
#else
    static QString getProcessPath(qint64 processId);
    static QString getProcessCommandLine(qint64 processId);
#endif

#ifdef _WIN32
    bool initWmi();
    void cleanupWmi();
    IWbemServices *m_pSvc = nullptr;
    IWbemLocator *m_pLoc = nullptr;
    using PidType = DWORD;
#else
    using PidType = qint64;
#endif

    QTimer *m_pollTimer = nullptr;
    QSet<QString> m_trackedProcesses;
    QSet<PidType> m_knownProcessIds;  // Track PIDs we've already seen
    QMap<PidType, QDateTime> m_processStartTimes;

    // Default set of developer-relevant processes
    static QSet<QString> defaultTrackedProcesses();
    static constexpr int kPollIntervalMs = 5000;  // Poll every 5 seconds
};
