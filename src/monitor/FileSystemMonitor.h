#pragma once

#include "IMonitor.h"
#include <QSet>
#include <QPair>
#include <QTimer>
#include <QMutex>
#include <QHash>

#ifdef _WIN32
#include <windows.h>
#endif

/// Watches configured project directories for source-file changes.
/// A portable snapshot poller is used so the same behavior works on Linux,
/// Windows and macOS without platform-specific watcher limits.
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

    void setPollInterval(int milliseconds);

private:
    struct FileState {
        QDateTime lastModified;
        qint64 size = 0;

        bool operator==(const FileState &other) const
        {
            return lastModified == other.lastModified && size == other.size;
        }
    };

    void pollFiles();
    void scanDirectory(const QString &path, QHash<QString, FileState> &snapshot) const;
    void emitFileEvent(EventType type, const QString &filePath, const FileState *state = nullptr);
    bool isTrackedFile(const QString &filePath) const;
    bool isExcludedPath(const QString &filePath) const;

    QList<QString> m_watchPaths;
    QSet<QString> m_trackedExtensions;
    QSet<QString> m_excludedPaths;
    QSet<QString> m_excludedPrefixes;
    QHash<QString, FileState> m_snapshot;
    QTimer *m_pollTimer = nullptr;
    int m_pollIntervalMs = 2000;

    // Deduplication: avoid duplicate events for the same file within a short window
    QMap<QString, QDateTime> m_recentEvents;
    QMutex m_recentMutex;
    static constexpr int kDedupWindowMs = 2000;

};
