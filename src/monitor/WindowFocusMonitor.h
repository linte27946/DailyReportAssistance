#pragma once

#include "IMonitor.h"
#include <QTimer>
#include <QThread>

#ifdef _WIN32
#include <windows.h>
#endif

/// Tracks the currently focused window using SetWinEventHook.
/// Captures window title, process name, and path on focus change.
class WindowFocusMonitor : public IMonitor {
    Q_OBJECT

public:
    explicit WindowFocusMonitor(QObject *parent = nullptr);
    ~WindowFocusMonitor() override;

    bool start() override;
    void stop() override;
    QString name() const override { return "WindowFocusMonitor"; }

    /// Get the current foreground window information.
    struct WindowInfo {
        HWND hwnd = nullptr;
        QString title;
        QString processName;
        QString processPath;
        DWORD processId = 0;
    };

    static WindowInfo getForegroundWindowInfo();

private:
    static QString getWindowTitle(HWND hwnd);
    static QString getWindowProcessName(HWND hwnd, DWORD *outPid = nullptr);
    static QString getWindowProcessPath(HWND hwnd);

#ifdef _WIN32
    static void CALLBACK winEventHookProc(HWINEVENTHOOK hWinEventHook,
                                          DWORD event, HWND hwnd,
                                          LONG idObject, LONG idChild,
                                          DWORD dwEventThread, DWORD dwmsEventTime);

    HWINEVENTHOOK m_hook = nullptr;
    // We use a static pointer to route callbacks to the instance
    static WindowFocusMonitor *s_instance;
#endif
};
