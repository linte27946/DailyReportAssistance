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
#ifdef _WIN32
        HWND hwnd = nullptr;
#else
        void *hwnd = nullptr;
#endif
        QString title;
        QString processName;
        QString processPath;
#ifdef _WIN32
        DWORD processId = 0;
#else
        qint64 processId = 0;
#endif
    };

    static WindowInfo getForegroundWindowInfo();

private:
#ifdef _WIN32
    static QString getWindowTitle(HWND hwnd);
    static QString getWindowProcessName(HWND hwnd, DWORD *outPid = nullptr);
    static QString getWindowProcessPath(HWND hwnd);
#else
    static QString getWindowTitle(void *hwnd);
    static QString getWindowProcessName(void *hwnd, qint64 *outPid = nullptr);
    static QString getWindowProcessPath(void *hwnd);
#endif

#ifdef _WIN32
    static void CALLBACK winEventHookProc(HWINEVENTHOOK hWinEventHook,
                                          DWORD event, HWND hwnd,
                                          LONG idObject, LONG idChild,
                                          DWORD dwEventThread, DWORD dwmsEventTime);

    HWINEVENTHOOK m_hook = nullptr;
    static WindowFocusMonitor *s_instance;
#endif
};
