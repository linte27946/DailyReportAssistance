#include "MarkdownRenderer.h"
#include <QRegularExpression>
#include <QStringList>

namespace {
// Forward-declared helper for inline formatting in simpleMarkdownToHtml
QString processInlineFormatting(const QString &text);
}
{
    return simpleMarkdownToHtml(markdown);
}

QString MarkdownRenderer::toPlainText(const QString &markdown)
{
    QString text = markdown;
    text.replace(QRegularExpression("\\*\\*(.+?)\\*\\*"), "\\1");
    text.replace(QRegularExpression("\\*(.+?)\\*"), "\\1");
    text.replace(QRegularExpression("`(.+?)`"), "\\1");
    text.replace(QRegularExpression("```[\\s\\S]*?```"), "[code]");
    text.replace(QRegularExpression("\\[([^\\]]+)\\]\\([^\\)]+\\)"), "\\1");
    text.replace(QRegularExpression("^#{1,6}\\s+", QRegularExpression::MultilineOption), "");
    text.replace(QRegularExpression("^\\s*[-*+]\\s+", QRegularExpression::MultilineOption), "• ");
    return text.trimmed();
}

QString MarkdownRenderer::escapeHtml(const QString &text)
{
    QString result = text;
    result.replace("&", "&amp;");
    result.replace("<", "&lt;");
    result.replace(">", "&gt;");
    return result;
}

QString MarkdownRenderer::simpleMarkdownToHtml(const QString &md)
{
    QString html = "<div style='font-family: -apple-system, sans-serif; line-height: 1.6;'>\n";
    QStringList lines = md.split('\n');
    bool inCodeBlock = false;
    bool inTable = false;

    for (const auto &line : lines) {
        // Code block fencing
        if (line.trimmed().startsWith("```")) {
            if (inCodeBlock) {
                html += "</code></pre>\n";
                inCodeBlock = false;
            } else {
                html += "<pre style='background:#f5f5f5;padding:10px;border-radius:4px;'><code>";
                inCodeBlock = true;
            }
            continue;
        }

        if (inCodeBlock) {
            html += escapeHtml(line) + "\n";
            continue;
        }

        // Table separator line
        QRegularExpression sepRx("^\\|\\s*[-:]+[-|\\s:]*\\|$");
        if (sepRx.match(line.trimmed()).hasMatch()) continue;

        // Table row
        if (line.trimmed().startsWith('|') && line.trimmed().endsWith('|')) {
            if (!inTable) {
                html += "<table style='border-collapse:collapse;width:100%;margin:10px 0;'>\n";
                inTable = true;
            }
            html += "<tr>";
            QStringList cells = line.mid(1, line.length() - 2).split('|');
            for (const auto &cell : cells) {
                html += "<td style='border:1px solid #ddd;padding:6px 10px;'>"
                        + escapeHtml(cell.trimmed()) + "</td>";
            }
            html += "</tr>\n";
            continue;
        }
        if (inTable && !line.trimmed().startsWith('|')) {
            html += "</table>\n";
            inTable = false;
        }

        // Headings
        QRegularExpression hRx("^(#{1,6})\\s+(.+)$");
        auto hm = hRx.match(line);
        if (hm.hasMatch()) {
            int lvl = hm.captured(1).length();
            html += QString("<h%1>%2</h%1>\n").arg(lvl).arg(escapeHtml(hm.captured(2)));
            continue;
        }

        // Horizontal rule
        if (line.trimmed() == "---" || line.trimmed() == "***") {
            html += "<hr>\n";
            continue;
        }

        // Empty line
        if (line.trimmed().isEmpty()) {
            html += "<br>\n";
            continue;
        }

        // Bold & italic inline
        QString processed = line;
        processed.replace(QRegularExpression("\\*\\*(.+?)\\*\\*"), "<strong>\\1</strong>");
        processed.replace(QRegularExpression("\\*(.+?)\\*"), "<em>\\1</em>");
        processed.replace(QRegularExpression("`([^`]+)`"), "<code>\\1</code>");
        processed.replace(QRegularExpression("\\[([^\\]]+)\\]\\(([^\\)]+)\\)"),
                          "<a href='\\2'>\\1</a>");

        html += "<p>" + processed + "</p>\n";
    }

    if (inCodeBlock) html += "</code></pre>\n";
    if (inTable) html += "</table>\n";
    html += "</div>";

    return html;
}
