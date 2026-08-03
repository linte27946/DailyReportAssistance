#pragma once

#include <QObject>
#include <QDate>
#include <QMap>
#include <QPair>
#include <QString>
#include <QVariantMap>
#include "core/Event.h"
#include "core/Timeline.h"
#include "llm/PromptTemplate.h"

/// Manages report templates: loading, editing, and rendering.
class TemplateEngine : public QObject {
    Q_OBJECT

public:
    explicit TemplateEngine(QObject *parent = nullptr);

    /// Register a template by name.
    void registerTemplate(const QString &name, const QString &content,
                          const QString &description = {});

    /// Load templates from the database.
    void loadFromDatabase();

    /// Save templates to the database.
    void saveToDatabase();

    /// Get all registered template names.
    QStringList templateNames() const;

    /// Get a template's content by name.
    QString templateContent(const QString &name) const;

    /// Get a template's description.
    QString templateDescription(const QString &name) const;

    /// Render a template with context variables.
    QString render(const QString &templateName, const QMap<QString, QString> &context) const;

    /// Render a template with a QVariantMap.
    QString renderVariant(const QString &templateName, const QVariantMap &context) const;

    /// Build the standard context variables from an ActivitySummary and timeline.
    static QMap<QString, QString> buildReportContext(
        const ActivitySummary &summary,
        const Timeline &timeline,
        const QDate &date);

    /// Build a context from a summary for weekly reports.
    static QMap<QString, QString> buildWeeklyContext(
        const QList<QPair<QDate, ActivitySummary>> &dailySummaries,
        const Timeline &weekTimeline,
        const QDate &weekStart);

    /// Get the default daily report template content.
    static QString defaultDailyTemplate();

    /// Get the default weekly report template content.
    static QString defaultWeeklyTemplate();

private:
    struct TemplateEntry {
        QString content;
        QString description;
    };
    QMap<QString, TemplateEntry> m_templates;
};
