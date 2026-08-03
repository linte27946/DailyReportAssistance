#pragma once

#include <QString>
#include <QMap>
#include <QVariant>
#include <QRegularExpression>

/// Lightweight template engine that substitutes {{placeholders}} with values.
/// Supports simple variable replacement — no loops or conditionals.
class PromptTemplate {
public:
    PromptTemplate() = default;

    /// Load a template from a raw markdown string.
    explicit PromptTemplate(const QString &content)
        : m_content(content)
    {
    }

    /// Load template content.
    void setContent(const QString &content) { m_content = content; }
    QString content() const { return m_content; }

    /// Render the template with the given context variables.
    /// Replaces all {{variable}} placeholders with their values.
    QString render(const QMap<QString, QString> &context) const
    {
        QString result = m_content;

        QRegularExpression regex("\\{\\{\\s*(\\w+)\\s*\\}\\}");
        QRegularExpressionMatchIterator it = regex.globalMatch(result);

        // Collect matches first to avoid infinite loops from replacement
        QMap<int, QPair<int, QString>> replacements; // pos → (length, replacement)
        while (it.hasNext()) {
            QRegularExpressionMatch match = it.next();
            QString varName = match.captured(1);
            QString replacement = context.value(varName, QString("{{%1}}").arg(varName));
            replacements[match.capturedStart()] = {match.capturedLength(), replacement};
        }

        // Apply replacements in reverse order to preserve positions
        QList<int> positions = replacements.keys();
        std::sort(positions.begin(), positions.end(), std::greater<int>());
        for (int pos : positions) {
            const auto &[len, repl] = replacements[pos];
            result.replace(pos, len, repl);
        }

        return result;
    }

    /// Render with a QVariantMap for convenience.
    QString renderVariant(const QVariantMap &context) const
    {
        QMap<QString, QString> strContext;
        for (auto it = context.begin(); it != context.end(); ++it) {
            strContext[it.key()] = it.value().toString();
        }
        return render(strContext);
    }

private:
    QString m_content;
};
