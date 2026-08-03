#pragma once

#include <QObject>
#include <QList>
#include <QJsonObject>
#include <QJsonArray>
#include <QMap>
#include "core/Event.h"

/// Classification rule: maps a process/extension/pattern to an activity category.
struct ClassificationRule {
    QString processName;      // Match process name (wildcard supported)
    QString extension;        // Match file extension
    QString urlPattern;       // Match URL pattern
    EventType eventType = EventType::Unknown; // Match event type
    EventCategory category = EventCategory::Other;
    int priority = 0;        // Higher priority rules take precedence
    QString description;     // Optional override description template
};

/// Classifies raw events into high-level activity categories.
class ActivityClassifier : public QObject {
    Q_OBJECT

public:
    explicit ActivityClassifier(QObject *parent = nullptr);

    /// Classify a single raw event.
    ActivityEvent classify(const RawEvent &raw);

    /// Classify a batch of raw events.
    QList<ActivityEvent> classifyBatch(const QList<RawEvent> &rawEvents);

    /// Load classification rules from JSON.
    void loadRules(const QJsonObject &config);

    /// Add a custom classification rule.
    void addRule(const ClassificationRule &rule);

    /// Clear all custom rules (reverts to defaults).
    void resetToDefaults();

private:
    EventCategory classifyInternal(const RawEvent &raw);
    void initDefaultRules();

    QList<ClassificationRule> m_rules;
    bool m_rulesSorted = false;

    void ensureRulesSorted();
};
