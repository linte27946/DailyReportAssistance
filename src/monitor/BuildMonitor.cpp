#include "BuildMonitor.h"
#include <spdlog/spdlog.h>

const QSet<QString> &BuildMonitor::buildProcesses()
{
    static const QSet<QString> procs = {
        // MSVC
        "msbuild.exe", "cl.exe", "link.exe", "lib.exe", "nmake.exe",
        "vctip.exe",
        // GCC / Clang
        "gcc.exe", "g++.exe", "clang.exe", "clang++.exe", "make.exe",
        "mingw32-make.exe", "cmake.exe", "ninja.exe",
        // Rust
        "rustc.exe", "cargo.exe",
        // Go
        "go.exe",
        // .NET
        "dotnet.exe", "csc.exe", "vbc.exe",
        // Java
        "javac.exe", "mvn.exe", "gradle.exe", "ant.bat",
        // Node / Web
        "node.exe", "tsc.exe", "webpack.cmd", "vite.cmd",
        "npm.cmd", "yarn.cmd", "pnpm.cmd",
        // Python
        "python.exe", "pip.exe",
        // Unity/Unreal
        "unity.exe", "UnrealBuildTool.exe",
        // Other
        "bazel.exe", "scons.exe", "meson",
    };
    return procs;
}

BuildMonitor::BuildMonitor(QObject *parent)
    : IMonitor(parent)
{
}

BuildMonitor::~BuildMonitor()
{
    stop();
}

bool BuildMonitor::start()
{
    spdlog::info("BuildMonitor starting...");
    m_buildCountToday = 0;
    m_buildFailuresToday = 0;
    setRunning(true);
    return true;
}

void BuildMonitor::stop()
{
    // End any active builds
#ifdef _WIN32
    QDateTime now = QDateTime::currentDateTimeUtc();
    for (auto it = m_activeBuilds.begin(); it != m_activeBuilds.end(); ++it) {
        const auto &build = it.value();
        int durationSecs = static_cast<int>(build.startTime.secsTo(now));

        RawEvent event;
        event.timestamp = now;
        event.type = EventType::BuildCompleted;
        event.source = "BuildMonitor";
        event.processName = build.processName;
        event.description = QString("Build process ended (monitor stopping): %1 (PID: %2)")
                                .arg(build.processName).arg(build.pid);
        event.durationSecs = durationSecs;
        event.metadata["pid"] = (qint64)build.pid;
        event.metadata["durationSecs"] = durationSecs;
        event.metadata["stoppedBySystem"] = true;
        emit rawEventCaptured(event);
    }
    m_activeBuilds.clear();
#endif
    setRunning(false);
    spdlog::info("BuildMonitor stopped.");
}

void BuildMonitor::onProcessEvent(const RawEvent &event)
{
    if (!isRunning()) return;
    if (event.source == "BuildMonitor") return;  // Don't process our own events

    QString procName = event.processName.toLower();
    if (!buildProcesses().contains(procName)) return;

#ifdef _WIN32
    bool ok = false;
    DWORD pid = static_cast<DWORD>(event.metadata["pid"].toInt(&ok));
    if (!ok || pid == 0) return;

    if (event.type == EventType::ProcessStarted) {
        // New build process started
        ActiveBuild build;
        build.pid = pid;
        build.processName = procName;
        build.startTime = event.timestamp;

        QString exePath = event.metadata["exePath"].toString();
        if (!exePath.isEmpty()) {
            QFileInfo fi(exePath);
            build.workingDir = fi.absolutePath();
        }

        m_activeBuilds[pid] = build;
        m_buildCountToday++;

        RawEvent buildEvent;
        buildEvent.timestamp = event.timestamp;
        buildEvent.type = EventType::BuildStarted;
        buildEvent.source = "BuildMonitor";
        buildEvent.processName = procName;
        buildEvent.description = QString("Build started: %1 (PID: %2)").arg(procName).arg(pid);
        buildEvent.metadata["pid"] = (qint64)pid;
        buildEvent.metadata["workingDir"] = build.workingDir;
        buildEvent.metadata["buildCountToday"] = m_buildCountToday;

        spdlog::debug("Build started: {} (PID: {})", procName.toStdString(), pid);
        emit rawEventCaptured(buildEvent);

    } else if (event.type == EventType::ProcessEnded) {
        // Build process ended
        if (!m_activeBuilds.contains(pid)) return;

        const auto &build = m_activeBuilds[pid];
        int durationSecs = static_cast<int>(build.startTime.secsTo(event.timestamp));

        // Determine build success/failure from metadata
        int exitCode = event.metadata["exitCode"].toInt(0);
        bool success = (exitCode == 0);

        RawEvent buildEvent;
        buildEvent.timestamp = event.timestamp;
        buildEvent.type = EventType::BuildCompleted;
        buildEvent.source = "BuildMonitor";
        buildEvent.processName = build.processName;
        buildEvent.durationSecs = durationSecs;

        if (success) {
            buildEvent.description = QString("Build completed successfully: %1 (%2s)")
                                         .arg(build.processName).arg(durationSecs);
        } else {
            buildEvent.description = QString("Build failed: %1 (%2s, exit code: %3)")
                                         .arg(build.processName).arg(durationSecs).arg(exitCode);
            m_buildFailuresToday++;
        }

        buildEvent.metadata["pid"] = (qint64)pid;
        buildEvent.metadata["durationSecs"] = durationSecs;
        buildEvent.metadata["success"] = success;
        buildEvent.metadata["exitCode"] = exitCode;
        buildEvent.metadata["buildCountToday"] = m_buildCountToday;
        buildEvent.metadata["buildFailuresToday"] = m_buildFailuresToday;

        spdlog::debug("Build ended: {} (PID: {}, duration: {}s, success: {})",
                      build.processName.toStdString(), pid, durationSecs, success);
        emit rawEventCaptured(buildEvent);

        m_activeBuilds.remove(pid);
        m_lastBuildEndTime = event.timestamp;
    }
#else
    Q_UNUSED(event);
#endif
}
