#include "WindowFocusMonitor.h"
#include <spdlog/spdlog.h>

#ifdef _WIN32
#include <psapi.h>
#pragma comment(lib, "user32.lib")

WindowFocusMonitor *WindowFocusMonitor::s_instance = nullptr;

void CALLBACK WindowFocusMonitor::winEventHookProc(HWINEVENTHOOK /*hHook*/,
                                                    DWORD event, HWND hwnd,
                                                    LONG idObject, LONG /*idChild*/,
                                                    DWORD /*dwEventThread*/,
                                                    DWORD /*dwmsEventTime*/)
{
    // Only handle foreground window changes on window objects
    if (event != EVENT_SYSTEM_FOREGROUND || idObject != OBJID_WINDOW)
        return;

    if (!s_instance || !s_instance->isRunning())
        return;

    auto info = getForegroundWindowInfo();

    // Ignore empty windows
    if (info.title.isEmpty() && info.processName.isEmpty())
        return;

    RawEvent eventData;
    eventData.timestamp = QDateTime::currentDateTimeUtc();
    eventData.type = EventType::WindowFocusChanged;
    eventData.source = "WindowFocusMonitor";
    eventData.processName = info.processName;
    eventData.windowTitle = info.title;
    eventData.description = QString("Window focus: [%1] %2")
                                .arg(info.processName, info.title);
    eventData.metadata["processId"] = (qint64)info.processId;
    eventData.metadata["processPath"] = info.processPath;
    eventData.metadata["hwnd"] = (qint64)info.hwnd;

    emit s_instance->rawEventCaptured(eventData);
}
#endif

WindowFocusMonitor::WindowFocusMonitor(QObject *parent)
    : IMonitor(parent)
{
}

WindowFocusMonitor::~WindowFocusMonitor()
{
    stop();
}

bool WindowFocusMonitor::start()
{
    spdlog::info("WindowFocusMonitor starting...");
#ifdef _WIN32
    s_instance = this;

    m_hook = SetWinEventHook(
        EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND,
        nullptr,
        winEventHookProc,
        0, 0,
        WINEVENT_OUTOFCONTEXT);  // Delivers on a system thread; signals use queued connection

    if (!m_hook) {
        spdlog::error("WindowFocusMonitor: SetWinEventHook failed: {}", GetLastError());
        return false;
    }

    setRunning(true);

    // Emit current foreground window as initial state
    auto info = getForegroundWindowInfo();
    if (!info.processName.isEmpty() || !info.title.isEmpty()) {
        RawEvent event;
        event.timestamp = QDateTime::currentDateTimeUtc();
        event.type = EventType::WindowFocusChanged;
        event.source = "WindowFocusMonitor";
        event.processName = info.processName;
        event.windowTitle = info.title;
        event.description = QString("Initial window: [%1] %2")
                                .arg(info.processName, info.title);
        event.metadata["processId"] = (qint64)info.processId;
        event.metadata["processPath"] = info.processPath;
        emit rawEventCaptured(event);
    }

    spdlog::info("WindowFocusMonitor started.");
    return true;
#else
    setRunning(true);
    return true;
#endif
}

void WindowFocusMonitor::stop()
{
#ifdef _WIN32
    if (m_hook) {
        UnhookWinEvent(m_hook);
        m_hook = nullptr;
    }
    s_instance = nullptr;
#endif
    setRunning(false);
    spdlog::info("WindowFocusMonitor stopped.");
}

WindowFocusMonitor::WindowInfo WindowFocusMonitor::getForegroundWindowInfo()
{
    WindowInfo info;
#ifdef _WIN32
    info.hwnd = GetForegroundWindow();
    if (!info.hwnd) return info;

    info.title = getWindowTitle(info.hwnd);
    DWORD pid = 0;
    info.processName = getWindowProcessName(info.hwnd, &pid);
    info.processId = pid;
    info.processPath = getWindowProcessPath(info.hwnd);
#endif
    return info;
}

#ifdef _WIN32

QString WindowFocusMonitor::getWindowTitle(HWND hwnd)
{
    WCHAR title[512] = {0};
    int len = GetWindowTextW(hwnd, title, 512);
    if (len > 0)
        return QString::fromWCharArray(title, len);
    return {};
}

QString WindowFocusMonitor::getWindowProcessName(HWND hwnd, DWORD *outPid)
{
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (outPid) *outPid = pid;

    HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!hProcess) return {};

    WCHAR exeName[MAX_PATH] = {0};
    DWORD size = MAX_PATH;
    if (QueryFullProcessImageNameW(hProcess, 0, exeName, &size)) {
        CloseHandle(hProcess);
        QString fullPath = QString::fromWCharArray(exeName);
        // Extract just the file name
        int lastSlash = fullPath.lastIndexOf('\\');
        return lastSlash >= 0 ? fullPath.mid(lastSlash + 1).toLower() : fullPath.toLower();
    }

    CloseHandle(hProcess);
    return {};
}

QString WindowFocusMonitor::getWindowProcessPath(HWND hwnd)
{
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);

    HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!hProcess) return {};

    WCHAR exeName[MAX_PATH] = {0};
    DWORD size = MAX_PATH;
    if (QueryFullProcessImageNameW(hProcess, 0, exeName, &size)) {
        CloseHandle(hProcess);
        return QString::fromWCharArray(exeName);
    }

    CloseHandle(hProcess);
    return {};
}

#else // __linux__

QString WindowFocusMonitor::getWindowTitle(void *)
{
    return {};
}

QString WindowFocusMonitor::getWindowProcessName(void *, qint64 *)
{
    return {};
}

QString WindowFocusMonitor::getWindowProcessPath(void *)
{
    return {};
}

#endif // _WIN32
