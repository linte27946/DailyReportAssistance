#pragma once

#include <QObject>
#include <QList>
#include <QMap>
#include <QJsonObject>
#include <QJsonArray>
#include "core/Event.h"

/// Filters and deduplicates raw events before classification.
/// Removes noise (temp files, build artifacts) and near-duplicate events.
class EventFilter : public QObject {
    Q_OBJECT

public:
    explicit EventFilter(QObject *parent = nullptr);

    /// Apply filter rules to a batch of events.
    QList<RawEvent> filter(const QList<RawEvent> &events);

    /// Load filter configuration from JSON.
    void loadConfig(const QJsonObject &config);

    /// Set the deduplication window in milliseconds. Default: 2000ms.
    void setDedupWindowMs(int ms) { m_dedupWindowMs = ms; }

    /// Set the minimum event duration in seconds. Shorter events are dropped.
    void setMinDurationSecs(int secs) { m_minDurationSecs = secs; }

    /// Add an ignored file path pattern.
    void addIgnoredPath(const QString &pattern);
    void addIgnoredProcess(const QString &processName);

private:
    bool isIgnoredPath(const QString &path) const;
    bool isIgnoredProcess(const QString &processName) const;
    bool isDuplicate(const RawEvent &event);
    void recordEvent(const RawEvent &event);

    int m_dedupWindowMs = 2000;
    int m_minDurationSecs = 1;

    QStringList m_ignoredPathPatterns;
    QStringList m_ignoredProcesses;

    // Dedup map: (filePath + eventType) → last seen timestamp
    QMap<QString, QDateTime> m_dedupMap;

    // Default ignored patterns
    void initDefaults();
};
