#pragma once

#include <QString>
#include <QByteArray>
#include <spdlog/spdlog.h>

#ifdef _WIN32
#include <windows.h>
#endif

/// Ensures only one instance of the application runs at a time.
/// Uses a named Windows mutex as the primary guard.
class SingleInstance {
public:
    explicit SingleInstance(const QString &name)
        : m_mutexName(name)
    {
    }

    ~SingleInstance()
    {
        release();
    }

    /// Try to acquire the single-instance lock. Returns false if another instance is running.
    bool tryLock()
    {
#ifdef _WIN32
        std::wstring wideName = m_mutexName.toStdWString();
        m_mutexHandle = CreateMutexW(nullptr, TRUE, wideName.c_str());
        DWORD error = GetLastError();

        if (m_mutexHandle == nullptr) {
            spdlog::error("Failed to create mutex: {}", error);
            return false;
        }

        if (error == ERROR_ALREADY_EXISTS) {
            spdlog::info("Another instance is already running (mutex exists).");
            CloseHandle(m_mutexHandle);
            m_mutexHandle = nullptr;
            return false;
        }

        m_locked = true;
        return true;
#else
        // Non-Windows: always succeed (for cross-platform compilation only)
        m_locked = true;
        return true;
#endif
    }

    /// Release the single-instance lock.
    void release()
    {
#ifdef _WIN32
        if (m_mutexHandle) {
            ReleaseMutex(m_mutexHandle);
            CloseHandle(m_mutexHandle);
            m_mutexHandle = nullptr;
        }
#endif
        m_locked = false;
    }

    /// Notify the existing instance to bring its window to the foreground.
    void notifyExistingInstance()
    {
#ifdef _WIN32
        // Find the existing window by class name or title
        HWND hwnd = FindWindowW(nullptr, L"DailyReport");
        if (hwnd) {
            spdlog::info("Found existing DailyReport window, bringing to foreground.");
            // Restore if minimized
            if (IsIconic(hwnd)) {
                ShowWindow(hwnd, SW_RESTORE);
            }
            SetForegroundWindow(hwnd);
        }
#endif
    }

    bool isLocked() const { return m_locked; }

private:
    QString m_mutexName;
#ifdef _WIN32
    HANDLE m_mutexHandle = nullptr;
#endif
    bool m_locked = false;
};
