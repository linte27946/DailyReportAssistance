#pragma once

#include <QString>
#include <QSettings>
#include <QStandardPaths>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <spdlog/spdlog.h>

#ifdef _WIN32
#include <windows.h>
#endif

/// Windows-specific utility functions.
namespace WinUtils {

/// Enable or disable auto-start with Windows via the registry Run key.
inline bool setAutoStart(bool enable, const QString &appName, const QString &appPath)
{
#ifdef _WIN32
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
#else
    const QString autostartDir = QStandardPaths::writableLocation(
        QStandardPaths::ConfigLocation) + "/autostart";
    const QString desktopPath = autostartDir + "/dailyreport.desktop";
    if (!enable) {
        return !QFile::exists(desktopPath) || QFile::remove(desktopPath);
    }

    if (!QDir().mkpath(autostartDir)) return false;
    QFile file(desktopPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
        return false;
    QTextStream stream(&file);
    QString escapedPath = appPath;
    escapedPath.replace("\\", "\\\\").replace("\"", "\\\"");
    stream << "[Desktop Entry]\n"
           << "Type=Application\n"
           << "Name=" << appName << "\n"
           << "Exec=\"" << escapedPath << "\"\n"
           << "Terminal=false\n"
           << "X-GNOME-Autostart-enabled=true\n";
    file.close();
    QFile::setPermissions(desktopPath,
                          QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    return file.error() == QFileDevice::NoError;
#endif
}

/// Check if auto-start is currently enabled.
inline bool isAutoStartEnabled(const QString &appName)
{
#ifdef _WIN32
    QSettings reg(
        "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run",
        QSettings::NativeFormat);
    return reg.contains(appName);
#else
    Q_UNUSED(appName);
    return QFile::exists(QStandardPaths::writableLocation(
        QStandardPaths::ConfigLocation) + "/autostart/dailyreport.desktop");
#endif
}

/// Get the current executable path.
inline QString applicationFilePath()
{
    return QCoreApplication::applicationFilePath();
}

/// Check if the application is running as administrator.
inline bool isRunningAsAdmin()
{
#ifdef _WIN32
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
#else
    return false;
#endif
}

/// Get the path to the user's documents folder.
inline QString documentsPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
}

/// Get the friendly name of the Windows version.
inline QString windowsVersionString()
{
    return QSysInfo::prettyProductName();
}

} // namespace WinUtils
