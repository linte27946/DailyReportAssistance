#include "ProcessMonitor.h"
#include <QCoreApplication>
#include <spdlog/spdlog.h>

#ifdef _WIN32
#include <tlhelp32.h>
#include <psapi.h>
#endif

QSet<QString> ProcessMonitor::defaultTrackedProcesses()
{
    return {
        // IDEs and Editors
        "devenv.exe",      // Visual Studio
        "code.exe",        // VS Code
        "code-insiders.exe",
        "idea64.exe",      // IntelliJ IDEA
        "pycharm64.exe",
        "webstorm64.exe",
        "clion64.exe",
        "rider64.exe",
        "notepad++.exe",
        "sublime_text.exe",
        "vim.exe", "gvim.exe",
        // Compilers / Build tools
        "msbuild.exe",
        "cl.exe",          // MSVC compiler
        "link.exe",        // MSVC linker
        "gcc.exe", "g++.exe",
        "clang.exe", "clang++.exe",
        "rustc.exe",
        "cargo.exe",
        "go.exe",
        "javac.exe", "java.exe",
        "dotnet.exe",
        "node.exe",
        "tsc.exe",
        // Version Control
        "git.exe",
        "tgit.exe",        // TortoiseGit
        // Terminals
        "cmd.exe",
        "powershell.exe",
        "pwsh.exe",
        "wt.exe",          // Windows Terminal
        "conhost.exe",
        // Debugging / Profiling
        "windbg.exe",
        "perfview.exe",
        "processhacker.exe",
        // Browsers (for URL tracking)
        "chrome.exe",
        "msedge.exe",
        "firefox.exe",
        // Communication
        "teams.exe",
        "slack.exe",
        "outlook.exe",
    };
}

ProcessMonitor::ProcessMonitor(QObject *parent)
    : IMonitor(parent)
{
    m_trackedProcesses = defaultTrackedProcesses();
    m_pollTimer = new QTimer(this);
    m_pollTimer->setInterval(kPollIntervalMs);
    connect(m_pollTimer, &QTimer::timeout, this, &ProcessMonitor::pollProcesses);
}

ProcessMonitor::~ProcessMonitor()
{
    stop();
#ifdef _WIN32
    cleanupWmi();
#endif
}

bool ProcessMonitor::start()
{
    spdlog::info("ProcessMonitor starting (polling every {}ms)...", kPollIntervalMs);
#ifdef _WIN32
    if (!initWmi()) {
        spdlog::warn("ProcessMonitor: WMI init failed, falling back to Toolhelp32 snapshot.");
    }
#endif

    // Initial scan of running processes
    pollProcesses();

    m_pollTimer->start();
    setRunning(true);
    return true;
}

void ProcessMonitor::stop()
{
    m_pollTimer->stop();
    setRunning(false);
    spdlog::info("ProcessMonitor stopped.");
}

void ProcessMonitor::addTrackedProcess(const QString &processName)
{
    m_trackedProcesses.insert(processName.toLower());
}

void ProcessMonitor::setTrackedProcesses(const QSet<QString> &processes)
{
    m_trackedProcesses = processes;
}

void ProcessMonitor::pollProcesses()
{
#ifdef _WIN32
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) {
        spdlog::error("ProcessMonitor: CreateToolhelp32Snapshot failed: {}", GetLastError());
        return;
    }

    PROCESSENTRY32W pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32W);

    if (!Process32FirstW(hSnapshot, &pe32)) {
        CloseHandle(hSnapshot);
        return;
    }

    QSet<DWORD> currentPids;

    do {
        QString exeName = QString::fromWCharArray(pe32.szExeFile).toLower();
        if (!m_trackedProcesses.contains(exeName)) continue;

        DWORD pid = pe32.th32ProcessID;
        currentPids.insert(pid);

        // New process detected
        if (!m_knownProcessIds.contains(pid)) {
            m_knownProcessIds.insert(pid);
            m_processStartTimes[pid] = QDateTime::currentDateTimeUtc();

            RawEvent event;
            event.timestamp = QDateTime::currentDateTimeUtc();
            event.type = EventType::ProcessStarted;
            event.source = "ProcessMonitor";
            event.processName = exeName;
            event.description = QString("Process started: %1 (PID: %2)")
                                    .arg(exeName).arg(pid);
            event.metadata["pid"] = (qint64)pid;
            event.metadata["exePath"] = getProcessPath(pid);

            emit rawEventCaptured(event);
        }
    } while (Process32NextW(hSnapshot, &pe32));

    CloseHandle(hSnapshot);

    // Detect terminated processes
    QList<DWORD> terminated;
    for (DWORD knownPid : m_knownProcessIds) {
        if (!currentPids.contains(knownPid)) {
            terminated.append(knownPid);
        }
    }

    for (DWORD pid : terminated) {
        m_knownProcessIds.remove(pid);

        RawEvent event;
        event.timestamp = QDateTime::currentDateTimeUtc();
        event.type = EventType::ProcessEnded;
        event.source = "ProcessMonitor";
        event.description = QString("Process ended (PID: %1)").arg(pid);

        if (m_processStartTimes.contains(pid)) {
            int duration = m_processStartTimes[pid].secsTo(event.timestamp);
            event.metadata["durationSecs"] = duration;
            m_processStartTimes.remove(pid);
        }

        emit rawEventCaptured(event);
    }
#else
    Q_UNUSED(this);
#endif
}

#ifdef _WIN32

QString ProcessMonitor::getProcessPath(DWORD processId)
{
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId);
    if (!hProcess) return {};
    WCHAR path[MAX_PATH] = {0};
    DWORD size = MAX_PATH;
    if (QueryFullProcessImageNameW(hProcess, 0, path, &size)) {
        CloseHandle(hProcess);
        return QString::fromWCharArray(path);
    }
    CloseHandle(hProcess);
    return {};
}

QString ProcessMonitor::getProcessCommandLine(DWORD processId)
{
    // Requires PROCESS_CREATE_PROCESS access; limited version for basic use
    Q_UNUSED(processId);
    return {};
}

bool ProcessMonitor::initWmi()
{
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) return false;

    hr = CoInitializeSecurity(nullptr, -1, nullptr, nullptr,
                              RPC_C_AUTHN_LEVEL_DEFAULT,
                              RPC_C_IMP_LEVEL_IMPERSONATE,
                              nullptr, EOAC_NONE, nullptr);
    if (FAILED(hr) && hr != RPC_E_TOO_LATE) return false;

    hr = CoCreateInstance(CLSID_WbemLocator, nullptr, CLSCTX_INPROC_SERVER,
                          IID_IWbemLocator, (void**)&m_pLoc);
    if (FAILED(hr)) return false;

    hr = m_pLoc->ConnectServer(
        _bstr_t(L"ROOT\\CIMV2"), nullptr, nullptr, nullptr,
        0, nullptr, nullptr, &m_pSvc);
    if (FAILED(hr)) {
        m_pLoc->Release();
        m_pLoc = nullptr;
        return false;
    }

    // Set security levels on the proxy
    CoSetProxyBlanket(m_pSvc, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE,
                      nullptr, RPC_C_AUTHN_LEVEL_CALL,
                      RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE);

    return true;
}

void ProcessMonitor::cleanupWmi()
{
    if (m_pSvc) { m_pSvc->Release(); m_pSvc = nullptr; }
    if (m_pLoc) { m_pLoc->Release(); m_pLoc = nullptr; }
}

#endif // _WIN32

QStringList ProcessMonitor::getRunningTrackedProcesses()
{
    QStringList result;
#ifdef _WIN32
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) return result;

    PROCESSENTRY32W pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32W);

    if (Process32FirstW(hSnapshot, &pe32)) {
        do {
            QString exeName = QString::fromWCharArray(pe32.szExeFile).toLower();
            if (m_trackedProcesses.contains(exeName))
                result.append(exeName);
        } while (Process32NextW(hSnapshot, &pe32));
    }
    CloseHandle(hSnapshot);
#else
    Q_UNUSED(this);
#endif
    return result;
}
