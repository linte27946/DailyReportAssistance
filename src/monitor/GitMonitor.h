#pragma once

#include "IMonitor.h"
#include <QTimer>
#include <QProcess>
#include <QSet>

/// Monitors Git activity by polling .git log and watching for changes
/// in monitored project directories.
class GitMonitor : public IMonitor {
    Q_OBJECT

public:
    explicit GitMonitor(QObject *parent = nullptr);
    ~GitMonitor() override;

    bool start() override;
    void stop() override;
    QString name() const override { return "GitMonitor"; }

    /// Add a git repository path to monitor.
    void addRepoPath(const QString &path);
    void addRepoPaths(const QStringList &paths);

    /// Set the polling interval in seconds. Default: 60.
    void setPollInterval(int seconds) { m_pollIntervalSecs = seconds; }

private:
    void scanRepositories();

    /// Check for new commits in a repository.
    QList<RawEvent> checkRepo(const QString &repoPath, const QDateTime &since);

    /// Get the list of git repositories from monitored directories.
    QStringList findRepositories(const QStringList &watchPaths);

    /// Parse git log output into events.
    static QList<RawEvent> parseGitLog(const QString &output, const QString &repoPath);

    QTimer *m_pollTimer = nullptr;
    QStringList m_repoPaths;
    QMap<QString, QDateTime> m_lastCheckTimes;  // Per-repo last check time
    int m_pollIntervalSecs = 60;
    QSet<QString> m_knownCommits;  // Dedup by commit hash
};
