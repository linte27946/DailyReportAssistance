#pragma once

#include <QString>
#include <QSettings>
#include <spdlog/spdlog.h>

/// Windows-specific utility functions.
namespace WinUtils {

/// Enable or disable auto-start with Windows via the registry Run key.
inline bool setAutoStart(bool enable, const QString &appName, const QString &appPath)
{
    QSettings reg(
        "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run",
        QSettings::NativeFormat);

    if (enable) {
        reg.setValue(appName, QString("\"%1\"").arg(QDir::toNativeSeparators(appPath)));
        spdlog::info("Auto-start enabled for: {}", appName.toStdString());
    } else {
        reg.remove(appName);
        spdlog::info("Auto-start disabled for: {}", appName.toStdString());
    }

    reg.sync();
    return reg.status() == QSettings::NoError;
}

/// Check if auto-start is currently enabled.
inline bool isAutoStartEnabled(const QString &appName)
{
    QSettings reg(
        "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run",
        QSettings::NativeFormat);
    return reg.contains(appName);
}

/// Get the current executable path.
inline QString applicationFilePath()
{
    return QCoreApplication::applicationFilePath();
}

/// Check if the application is running as administrator.
inline bool isRunningAsAdmin()
{
    // Simplified check using the Windows API
    BOOL isElevated = FALSE;
    HANDLE hToken = nullptr;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
        TOKEN_ELEVATION elevation;
        DWORD size = sizeof(TOKEN_ELEVATION);
        if (GetTokenInformation(hToken, TokenElevation, &elevation, size, &size)) {
            isElevated = elevation.TokenIsElevated;
        }
        CloseHandle(hToken);
    }
    return isElevated != FALSE;
}

/// Get the path to the user's documents folder.
inline QString documentsPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
}

/// Get the friendly name of the Windows version.
inline QString windowsVersionString()
{
    // Use QSysInfo for basic version info
    return QSysInfo::prettyProductName();
}

} // namespace WinUtils
