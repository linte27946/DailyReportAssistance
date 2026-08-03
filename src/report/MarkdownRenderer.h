#pragma once

#include <QString>

/// Converts Markdown to HTML for rich report preview.
/// Uses a lightweight approach: either cmark-gfm if available, or a simple Qt-based conversion.
class MarkdownRenderer {
public:
    /// Render markdown text to HTML.
    static QString toHtml(const QString &markdown);

    /// Render markdown text to plain text (strip formatting).
    static QString toPlainText(const QString &markdown);

private:
    /// Simple built-in markdown → HTML converter for headings, bold, italic, code, lists, tables.
    static QString simpleMarkdownToHtml(const QString &md);

    /// Escape HTML entities in text.
    static QString escapeHtml(const QString &text);
};
