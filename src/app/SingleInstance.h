#pragma once

#include <QString>
#include <QByteArray>
#include <QLockFile>
#include <QDir>
#include <QStandardPaths>
#include <memory>
#include <spdlog/spdlog.h>

#ifdef _WIN32
#include <windows.h>
#endif

/// Ensures only one instance of the application runs at a time.
/// Uses a named Windows mutex or a QLockFile on Unix-like systems.
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
        QString runtimeDir = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
        if (runtimeDir.isEmpty())
            runtimeDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
        QDir().mkpath(runtimeDir);
        m_lockFile = std::make_unique<QLockFile>(
            runtimeDir + "/" + m_mutexName + ".lock");
        m_lockFile->setStaleLockTime(30000);
        m_locked = m_lockFile->tryLock(0);
        if (!m_locked)
            spdlog::info("Another instance is already running (lock file exists).");
        return m_locked;
#endif
#ifndef _WIN32
        if (m_lockFile) {
            m_lockFile->unlock();
            m_lockFile.reset();
        }
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
        HWND hwnd = FindWindowW(nullptr, L"DailyReport");
        if (hwnd) {
            spdlog::info("Found existing DailyReport window, bringing to foreground.");
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
#else
    std::unique_ptr<QLockFile> m_lockFile;
#endif
    bool m_locked = false;
};
