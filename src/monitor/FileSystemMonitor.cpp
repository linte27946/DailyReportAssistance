#include "FileSystemMonitor.h"
#include <QDir>
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

    m_excludedPaths = {
        "AppData", "Temp", "Cache", "logs",
    };
}

FileSystemMonitor::~FileSystemMonitor()
{
    stop();
}

bool FileSystemMonitor::start()
{
    if (m_watchPaths.isEmpty()) {
        spdlog::info("FileSystemMonitor: No watch paths configured, monitoring skipped.");
        setRunning(true);
        return true;
    }

    spdlog::info("FileSystemMonitor starting with {} watch paths.", m_watchPaths.size());
    setRunning(true);
    return true;
}

void FileSystemMonitor::stop()
{
    m_stopRequested = true;
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

#ifdef _WIN32

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
    // Check excluded prefixes (directory names)
    QString normalized = QDir::toNativeSeparators(filePath).toLower();
    for (const auto &prefix : m_excludedPrefixes) {
        if (normalized.contains("\\" + prefix + "\\") ||
            normalized.contains("/" + prefix + "/") ||
            normalized.contains("\\" + prefix) ||
            normalized.contains("/" + prefix))
            return true;
    }

    // Check excluded path fragments
    for (const auto &exPath : m_excludedPaths) {
        if (normalized.contains(exPath.toLower()))
            return true;
    }

    return false;
}

QString FileSystemMonitor::actionToString(DWORD action)
{
    switch (action) {
    case FILE_ACTION_ADDED:            return "added";
    case FILE_ACTION_REMOVED:          return "removed";
    case FILE_ACTION_MODIFIED:         return "modified";
    case FILE_ACTION_RENAMED_OLD_NAME: return "renamed_from";
    case FILE_ACTION_RENAMED_NEW_NAME: return "renamed_to";
    default:                           return "unknown";
    }
}

#endif // _WIN32
