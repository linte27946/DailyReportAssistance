#include "FileSystemMonitor.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <spdlog/spdlog.h>

FileSystemMonitor::FileSystemMonitor(QObject *parent)
    : IMonitor(parent)
{
    // Default tracked extensions for common programming files
    m_trackedExtensions = {
        ".cpp", ".h", ".hpp", ".cc", ".cxx", ".c",    // C/C++
        ".cs", ".xaml", ".csproj", ".sln",             // C#/.NET
        ".py", ".pyw",                                 // Python
        ".js", ".ts", ".jsx", ".tsx", ".mjs",          // JavaScript/TypeScript
        ".java", ".kt", ".groovy",                     // Java/Kotlin
        ".rs", ".go", ".rb", ".php", ".swift",        // Other languages
        ".html", ".css", ".scss", ".less",             // Web
        ".json", ".xml", ".yaml", ".yml", ".toml",     // Config
        ".md", ".txt", ".rst",                         // Documentation
        ".cmake", ".cmakelists", ".mk", ".make",       // Build
        ".sql", ".sh", ".ps1", ".bat", ".dockerfile",  // Scripts
    };

    // Default excluded directories/prefixes
    m_excludedPrefixes = {
        "node_modules", ".git", ".svn", ".hg",
        "__pycache__", ".pytest_cache", ".tox",
        "bin", "obj", "build", "out", "target",
        ".vs", ".vscode", ".idea",
        "dist", "coverage", ".nyc_output",
        "vendor", "bower_components",
    };

    // Only explicitly monitored roots are scanned, so broad OS path fragments
    // (for example /tmp) must not be excluded here. Generated project folders
    // are handled by m_excludedPrefixes above.
    m_excludedPaths.clear();

    m_pollTimer = new QTimer(this);
    m_pollTimer->setInterval(m_pollIntervalMs);
    connect(m_pollTimer, &QTimer::timeout, this, &FileSystemMonitor::pollFiles);
}

FileSystemMonitor::~FileSystemMonitor()
{
    stop();
}

bool FileSystemMonitor::start()
{
    if (m_watchPaths.isEmpty()) {
        spdlog::info("FileSystemMonitor: No watch paths configured, monitoring skipped.");
        setRunning(false);
        return true;
    }

    spdlog::info("FileSystemMonitor starting with {} watch paths.", m_watchPaths.size());
    m_snapshot.clear();
    for (const auto &path : m_watchPaths)
        scanDirectory(path, m_snapshot);
    m_pollTimer->setInterval(m_pollIntervalMs);
    m_pollTimer->start();
    setRunning(true);
    return true;
}

void FileSystemMonitor::stop()
{
    m_pollTimer->stop();
    m_snapshot.clear();
    setRunning(false);
    spdlog::info("FileSystemMonitor stopped.");
}

void FileSystemMonitor::addWatchPath(const QString &path)
{
    QFileInfo fi(path);
    if (!fi.exists() || !fi.isDir()) {
        spdlog::warn("FileSystemMonitor: Path does not exist or is not a directory: {}",
                     path.toStdString());
        return;
    }

    QString canonical = fi.canonicalFilePath();
    if (!m_watchPaths.contains(canonical)) {
        m_watchPaths.append(canonical);
        spdlog::info("FileSystemMonitor: Added watch path: {}", canonical.toStdString());
    }
}

void FileSystemMonitor::addWatchPaths(const QStringList &paths)
{
    for (const auto &p : paths)
        addWatchPath(p);
}

void FileSystemMonitor::removeWatchPath(const QString &path)
{
    QFileInfo fi(path);
    QString canonical = fi.canonicalFilePath();
    m_watchPaths.removeAll(canonical);
}

void FileSystemMonitor::setTrackedExtensions(const QSet<QString> &extensions)
{
    m_trackedExtensions = extensions;
}

void FileSystemMonitor::setExcludedPaths(const QSet<QString> &paths)
{
    m_excludedPaths = paths;
}

void FileSystemMonitor::setPollInterval(int milliseconds)
{
    m_pollIntervalMs = qMax(250, milliseconds);
    if (m_pollTimer->isActive())
        m_pollTimer->setInterval(m_pollIntervalMs);
}

bool FileSystemMonitor::isTrackedFile(const QString &filePath) const
{
    if (m_trackedExtensions.isEmpty()) return true;

    QFileInfo fi(filePath);
    QString ext = fi.suffix().toLower();
    if (ext.isEmpty()) return true; // Track files without extensions
    return m_trackedExtensions.contains("." + ext);
}

bool FileSystemMonitor::isExcludedPath(const QString &filePath) const
{
    const QString normalized = QDir::fromNativeSeparators(filePath).toLower();
    for (const auto &prefix : m_excludedPrefixes) {
        const QString component = "/" + prefix.toLower() + "/";
        if (normalized.contains(component) || normalized.endsWith("/" + prefix.toLower()))
            return true;
    }

    // Check excluded path fragments
    for (const auto &exPath : m_excludedPaths) {
        if (normalized.contains(exPath.toLower()))
            return true;
    }

    return false;
}

void FileSystemMonitor::scanDirectory(const QString &path,
                                      QHash<QString, FileState> &snapshot) const
{
    QDir dir(path);
    const QFileInfoList entries = dir.entryInfoList(
        QDir::NoDotAndDotDot | QDir::AllDirs | QDir::Files | QDir::NoSymLinks,
        QDir::Name | QDir::DirsFirst);

    for (const auto &entry : entries) {
        const QString absolutePath = entry.absoluteFilePath();
        if (isExcludedPath(absolutePath)) continue;

        if (entry.isDir()) {
            scanDirectory(absolutePath, snapshot);
        } else if (entry.isFile() && isTrackedFile(absolutePath)) {
            snapshot.insert(absolutePath,
                            FileState{entry.lastModified().toUTC(), entry.size()});
        }
    }
}

void FileSystemMonitor::pollFiles()
{
    QHash<QString, FileState> current;
    for (const auto &path : m_watchPaths)
        scanDirectory(path, current);

    for (auto it = current.cbegin(); it != current.cend(); ++it) {
        const auto previous = m_snapshot.constFind(it.key());
        if (previous == m_snapshot.cend()) {
            emitFileEvent(EventType::FileCreated, it.key(), &it.value());
        } else if (!(previous.value() == it.value())) {
            emitFileEvent(EventType::FileModified, it.key(), &it.value());
        }
    }

    for (auto it = m_snapshot.cbegin(); it != m_snapshot.cend(); ++it) {
        if (!current.contains(it.key()))
            emitFileEvent(EventType::FileDeleted, it.key());
    }

    m_snapshot.swap(current);
}

void FileSystemMonitor::emitFileEvent(EventType type,
                                      const QString &filePath,
                                      const FileState *state)
{
    const QDateTime now = QDateTime::currentDateTimeUtc();
    const QString dedupKey = eventTypeToString(type) + ":" + filePath;
    {
        QMutexLocker lock(&m_recentMutex);
        const QDateTime previous = m_recentEvents.value(dedupKey);
        if (previous.isValid() && previous.msecsTo(now) < kDedupWindowMs)
            return;
        m_recentEvents[dedupKey] = now;
        if (m_recentEvents.size() > 5000)
            m_recentEvents.clear();
    }

    RawEvent event;
    event.timestamp = now;
    event.type = type;
    event.source = "FileSystemMonitor";
    event.filePath = filePath;
    event.description = QString("%1: %2").arg(eventTypeToString(type), filePath);
    if (state) {
        event.metadata["size"] = state->size;
        event.metadata["lastModified"] = state->lastModified.toString(Qt::ISODateWithMs);
    }
    emit rawEventCaptured(event);
}
