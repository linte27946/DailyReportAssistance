#pragma once

#include "IMonitor.h"
#include <QMap>
#include <QSet>
#include <QTimer>

#ifdef _WIN32
#include <windows.h>
#endif

/// Detects build/compilation activity by monitoring process creation.
/// Listens for known compiler/ build tool processes and tracks their durations.
class BuildMonitor : public IMonitor {
    Q_OBJECT

public:
    explicit BuildMonitor(QObject *parent = nullptr);
    ~BuildMonitor() override;

    bool start() override;
    void stop() override;
    QString name() const override { return "BuildMonitor"; }

    /// Feed a ProcessMonitor event into this monitor for build detection.
    void onProcessEvent(const RawEvent &event);

private:
    /// Known build/compiler process names.
    static const QSet<QString> &buildProcesses();

#ifdef _WIN32
    struct ActiveBuild {
        DWORD pid = 0;
        QString processName;
        QDateTime startTime;
        QString workingDir;
    };

    QMap<DWORD, ActiveBuild> m_activeBuilds;
#endif

    // Track build attempts per session
    QDateTime m_lastBuildEndTime;
    int m_buildCountToday = 0;
    int m_buildFailuresToday = 0;
};
