#include "EventFilter.h"
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <spdlog/spdlog.h>

EventFilter::EventFilter(QObject *parent)
    : QObject(parent)
{
    initDefaults();
}

void EventFilter::initDefaults()
{
    m_ignoredPathPatterns = {
        "*.tmp", "*.temp", "*.log", "*.cache",
        "*.o", "*.obj", "*.pch", "*.gch", "*.ilk", "*.pdb",
        "*.class", "*.pyc", "*.pyo", "__pycache__",
        "*.min.js", "*.min.css", "*.map",
        "*.lock", "package-lock.json", "yarn.lock",
        "desktop.ini", "thumbs.db",
        "*.suo", "*.user", "*.userosscache", "*.sln.docstates",
    };

    m_ignoredProcesses = {
        "explorer.exe", "searchhost.exe", "sihost.exe",
        "taskhostw.exe", "svchost.exe", "dwm.exe",
        "ctfmon.exe", "textinputhost.exe", "systemsettings.exe",
        "applicationframehost.exe", "backgroundtaskhost.exe",
        "shellexperiencehost.exe", "runtimebroker.exe",
        "securityhealthsystray.exe", "securityhealthservice.exe",
    };
}

QList<RawEvent> EventFilter::filter(const QList<RawEvent> &events)
{
    QList<RawEvent> filtered;

    for (const auto &e : events) {
        // Ignore specific processes
        if (isIgnoredProcess(e.processName))
            continue;

        // Ignore specific paths
        if (!e.filePath.isEmpty() && isIgnoredPath(e.filePath))
            continue;

        // Deduplicate near-identical events
        if (isDuplicate(e))
            continue;

        // Apply minimum duration filter (for duration events)
        int duration = e.metadata["durationSecs"].toInt(0);
        if (duration > 0 && duration < m_minDurationSecs)
            continue;

        recordEvent(e);
        filtered.append(e);
    }

    int dropped = events.size() - filtered.size();
    if (dropped > 0) {
        spdlog::debug("EventFilter: dropped {} of {} events", dropped, events.size());
    }

    return filtered;
}

void EventFilter::loadConfig(const QJsonObject &config)
{
    if (config.contains("dedupWindowMs"))
        m_dedupWindowMs = config["dedupWindowMs"].toInt();
    if (config.contains("minDurationSecs"))
        m_minDurationSecs = config["minDurationSecs"].toInt();

    if (config.contains("ignoredPaths")) {
        m_ignoredPathPatterns.clear();
        for (const auto &v : config["ignoredPaths"].toArray())
            m_ignoredPathPatterns.append(v.toString());
    }

    if (config.contains("ignoredProcesses")) {
        m_ignoredProcesses.clear();
        for (const auto &v : config["ignoredProcesses"].toArray())
            m_ignoredProcesses.append(v.toString());
    }
}

void EventFilter::addIgnoredPath(const QString &pattern)
{
    m_ignoredPathPatterns.append(pattern);
}

void EventFilter::addIgnoredProcess(const QString &processName)
{
    m_ignoredProcesses.append(processName.toLower());
}

bool EventFilter::isIgnoredPath(const QString &path) const
{
    QString normalized = QDir::toNativeSeparators(path);
    for (const auto &pattern : m_ignoredPathPatterns) {
        QRegularExpression regex(QRegularExpression::wildcardToRegularExpression(pattern),
                                  QRegularExpression::CaseInsensitiveOption);
        if (regex.match(QFileInfo(path).fileName()).hasMatch())
            return true;
        if (normalized.contains(pattern, Qt::CaseInsensitive))
            return true;
    }
    return false;
}

bool EventFilter::isIgnoredProcess(const QString &processName) const
{
    if (processName.isEmpty()) return false;
    return m_ignoredProcesses.contains(processName.toLower());
}

bool EventFilter::isDuplicate(const RawEvent &event)
{
    // Build a dedup key based on file path + event type
    QString key;
    if (!event.filePath.isEmpty()) {
        key = event.filePath + "|" + eventTypeToString(event.type);
    } else if (!event.url.isEmpty()) {
        key = event.url + "|UrlVisited";
    } else {
        // For non-file/URL events, use process + type + description
        key = event.processName + "|" + eventTypeToString(event.type) + "|"
              + event.description.left(80);
    }

    QDateTime now = QDateTime::currentDateTimeUtc();

    if (m_dedupMap.contains(key)) {
        int msSince = m_dedupMap[key].msecsTo(now);
        if (msSince < m_dedupWindowMs) {
            return true;  // Duplicate within the window
        }
    }

    m_dedupMap[key] = now;
    return false;
}

void EventFilter::recordEvent(const RawEvent &event)
{
    // Periodically clean old entries from the dedup map
    if (m_dedupMap.size() > 10000) {
        QDateTime cutoff = QDateTime::currentDateTimeUtc().addSecs(-60);
        QMutableMapIterator<QString, QDateTime> it(m_dedupMap);
        while (it.hasNext()) {
            it.next();
            if (it.value() < cutoff)
                it.remove();
        }
    }
}
