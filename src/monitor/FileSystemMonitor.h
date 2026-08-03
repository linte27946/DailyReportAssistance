#pragma once

#include "IMonitor.h"
#include <QSet>
#include <QPair>
#include <QTimer>
#include <QMutex>

#ifdef _WIN32
#include <windows.h>
#endif

/// Watches specified directories for file changes using ReadDirectoryChangesW.
/// Runs on a dedicated thread with IOCP for scalable I/O.
class FileSystemMonitor : public IMonitor {
    Q_OBJECT

public:
    explicit FileSystemMonitor(QObject *parent = nullptr);
    ~FileSystemMonitor() override;

    bool start() override;
    void stop() override;
    QString name() const override { return "FileSystemMonitor"; }

    /// Add a directory path to monitor (recursively monitors subdirectories).
    void addWatchPath(const QString &path);
    void addWatchPaths(const QStringList &paths);

    /// Remove a watched directory.
    void removeWatchPath(const QString &path);

    /// Set which file extensions to track (e.g., {".cpp", ".h", ".py"}).
    /// If empty, all file types are tracked.
    void setTrackedExtensions(const QSet<QString> &extensions);

    /// Set which directories or patterns to exclude.
    void setExcludedPaths(const QSet<QString> &paths);

private:
    void watchLoop();

#ifdef _WIN32
    struct WatchEntry {
        QString path;
        HANDLE dirHandle = INVALID_HANDLE_VALUE;
        OVERLAPPED overlapped;
        BYTE buffer[65536];
    };

    bool addDirectoryWatch(const QString &path, bool recursive);
    void processChanges(WatchEntry &entry, DWORD bytesTransferred);
    bool isTrackedFile(const QString &filePath) const;
    bool isExcludedPath(const QString &filePath) const;
    static QString actionToString(DWORD action);
#endif

    QList<QString> m_watchPaths;
    QSet<QString> m_trackedExtensions;
    QSet<QString> m_excludedPaths;
    QSet<QString> m_excludedPrefixes;

    // Deduplication: avoid duplicate events for the same file within a short window
    QMap<QString, QDateTime> m_recentEvents;
    QMutex m_recentMutex;
    static constexpr int kDedupWindowMs = 2000;

    volatile bool m_stopRequested = false;
};
