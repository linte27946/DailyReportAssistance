#include "GitMonitor.h"
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <spdlog/spdlog.h>

GitMonitor::GitMonitor(QObject *parent)
    : IMonitor(parent)
{
    m_pollTimer = new QTimer(this);
    connect(m_pollTimer, &QTimer::timeout, this, &GitMonitor::scanRepositories);
}

GitMonitor::~GitMonitor()
{
    stop();
}

bool GitMonitor::start()
{
    spdlog::info("GitMonitor starting (polling every {}s)...", m_pollIntervalSecs);

    m_pollTimer->setInterval(m_pollIntervalSecs * 1000);

    // Do an initial scan with a wider window to catch recent activity
    scanRepositories();

    m_pollTimer->start();
    setRunning(true);
    return true;
}

void GitMonitor::stop()
{
    m_pollTimer->stop();
    setRunning(false);
    spdlog::info("GitMonitor stopped.");
}

void GitMonitor::addRepoPath(const QString &path)
{
    QFileInfo fi(path);
    if (fi.exists()) {
        QString canonical = fi.canonicalFilePath();
        if (!m_repoPaths.contains(canonical)) {
            m_repoPaths.append(canonical);
            spdlog::info("GitMonitor: Added repo path: {}", canonical.toStdString());
        }
    }
}

void GitMonitor::addRepoPaths(const QStringList &paths)
{
    for (const auto &p : paths)
        addRepoPath(p);
}

void GitMonitor::scanRepositories()
{
    QDateTime now = QDateTime::currentDateTimeUtc();
    int totalEvents = 0;

    for (const auto &repoPath : m_repoPaths) {
        const QDateTime startOfToday = QDateTime(
            QDate::currentDate(), QTime(0, 0), Qt::LocalTime).toUTC();
        QDateTime since = m_lastCheckTimes.value(repoPath, startOfToday);
        auto events = checkRepo(repoPath, since);
        for (const auto &e : events) {
            emit rawEventCaptured(e);
            totalEvents++;
        }
        m_lastCheckTimes[repoPath] = now;
    }

    if (totalEvents > 0) {
        spdlog::debug("GitMonitor: Found {} new git events.", totalEvents);
    }
}

QList<RawEvent> GitMonitor::checkRepo(const QString &repoPath, const QDateTime &since)
{
    QList<RawEvent> events;

    QDir dir(repoPath);
    if (!dir.exists(".git")) return events;

    QProcess git;
    git.setWorkingDirectory(repoPath);
    git.start("git", {
        "log", "--all",
        "--since=" + since.toString(Qt::ISODate),
        "--format=%H|%an|%ae|%ai|%s",
        "--name-only"
    });

    if (!git.waitForFinished(15000)) {
        git.kill();
        git.waitForFinished(1000);
        spdlog::warn("GitMonitor: git log timed out for {}", repoPath.toStdString());
        return events;
    }

    if (git.exitStatus() != QProcess::NormalExit || git.exitCode() != 0) {
        spdlog::warn("GitMonitor: git log failed for {}: {}",
                     repoPath.toStdString(),
                     QString::fromUtf8(git.readAllStandardError()).trimmed().toStdString());
        return events;
    }

    QString output = QString::fromUtf8(git.readAllStandardOutput());
    if (output.isEmpty()) return events;

    return parseGitLog(output, repoPath);
}

QList<RawEvent> GitMonitor::parseGitLog(const QString &output, const QString &repoPath)
{
    QList<RawEvent> events;
    QStringList lines = output.split('\n', Qt::SkipEmptyParts);

    QString currentHash, currentAuthor, currentEmail, currentTimestamp, currentMessage;
    QStringList currentFiles;

    for (const auto &line : lines) {
        // Detect commit header line: starts with a 40-char hex hash
        if (line.length() >= 41 && line[0].isLetterOrNumber()) {
            QStringList parts = line.split('|');
            if (parts.size() >= 5) {
                // Save previous commit
                if (!currentHash.isEmpty() && !m_knownCommits.contains(currentHash)) {
                    m_knownCommits.insert(currentHash);

                    // Emit one event per commit
                    RawEvent event;
                    event.timestamp = QDateTime::fromString(
                        currentTimestamp.trimmed(), Qt::ISODate).toUTC();
                    event.type = EventType::GitCommit;
                    event.source = "GitMonitor";
#ifdef _WIN32
                    event.processName = "git.exe";
#else
                    event.processName = "git";
#endif
                    event.description = QString("Git commit: %1").arg(currentMessage.trimmed());
                    event.metadata["hash"] = currentHash;
                    event.metadata["author"] = currentAuthor;
                    event.metadata["email"] = currentEmail;
                    event.metadata["message"] = currentMessage.trimmed();
                    event.metadata["repoPath"] = repoPath;

                    QJsonArray filesArr;
                    for (const auto &f : currentFiles) {
                        if (!f.trimmed().isEmpty()) filesArr.append(f.trimmed());
                    }
                    event.metadata["files"] = filesArr;
                    event.metadata["fileCount"] = filesArr.size();

                    events.append(event);

                    // Keep known commits set bounded
                    if (m_knownCommits.size() > 10000) {
                        m_knownCommits.clear();
                        m_knownCommits.insert(currentHash);
                    }
                }

                // Start new commit
                currentHash = parts[0].trimmed();
                currentAuthor = parts[1].trimmed();
                currentEmail = parts[2].trimmed();
                currentTimestamp = parts[3].trimmed();
                currentMessage = parts[4].trimmed();
                currentFiles.clear();
            }
        } else if (!line.trimmed().isEmpty() && !currentHash.isEmpty()) {
            // File path line (not starting with a hash pattern)
            currentFiles.append(line.trimmed());
        }
    }

    // Don't forget the last commit
    if (!currentHash.isEmpty() && !m_knownCommits.contains(currentHash)) {
        m_knownCommits.insert(currentHash);
        RawEvent event;
        event.timestamp = QDateTime::fromString(
            currentTimestamp.trimmed(), Qt::ISODate).toUTC();
        event.type = EventType::GitCommit;
        event.source = "GitMonitor";
#ifdef _WIN32
        event.processName = "git.exe";
#else
        event.processName = "git";
#endif
        event.description = QString("Git commit: %1").arg(currentMessage.trimmed());
        event.metadata["hash"] = currentHash;
        event.metadata["author"] = currentAuthor;
        event.metadata["email"] = currentEmail;
        event.metadata["message"] = currentMessage.trimmed();
        event.metadata["repoPath"] = repoPath;

        QJsonArray filesArr;
        for (const auto &f : currentFiles) {
            if (!f.trimmed().isEmpty()) filesArr.append(f.trimmed());
        }
        event.metadata["files"] = filesArr;
        event.metadata["fileCount"] = filesArr.size();

        events.append(event);
    }

    return events;
}

QStringList GitMonitor::findRepositories(const QStringList &watchPaths)
{
    QStringList repos;
    for (const auto &path : watchPaths) {
        QDir dir(path);
        QDirIterator it(path, QDir::Dirs | QDir::NoDotAndDotDot,
                        QDirIterator::Subdirectories);
        while (it.hasNext()) {
            it.next();
            if (it.fileName() == ".git") {
                // The parent of .git is the repo root
                repos.append(QFileInfo(it.filePath()).absolutePath());
                break;  // Don't recurse into git repos
            }
        }
    }
    return repos;
}
