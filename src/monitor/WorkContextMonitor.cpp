#include "WorkContextMonitor.h"

#include <QFileInfo>
#include <QRegularExpression>
#include <QSet>
#include <spdlog/spdlog.h>

namespace {

QString normalizedProcess(QString processName)
{
    processName = QFileInfo(processName.toLower()).fileName();
    if (processName.endsWith(".exe")) processName.chop(4);
    return processName;
}

const QSet<QString> &editorProcesses()
{
    static const QSet<QString> values = {
        "code", "code-insiders", "cursor", "vscodium", "codium",
        "webstorm64", "idea64", "pycharm64", "clion64", "rider64",
        "goland64", "phpstorm64", "devenv", "sublime_text", "notepad++",
        "zed", "atom", "vim", "nvim", "emacs"
    };
    return values;
}

const QSet<QString> &documentProcesses()
{
    static const QSet<QString> values = {
        "winword", "powerpnt", "excel", "acrord32", "acrobat",
        "sumatrapdf", "foxitpdfreader", "foxitreader", "okular", "evince",
        "libreoffice", "soffice", "wps", "wpsoffice", "et", "wpp"
    };
    return values;
}

const QRegularExpression &documentExpression()
{
    static const QRegularExpression expression(
        R"(([^\\/:*?"<>|\r\n]+\.(?:pdf|docx?|pptx?|xlsx?|odt|ods|odp|epub)))",
        QRegularExpression::CaseInsensitiveOption);
    return expression;
}

} // namespace

WorkContextMonitor::WorkContextMonitor(QObject *parent)
    : IMonitor(parent)
{
}

bool WorkContextMonitor::start()
{
    m_lastContextKey.clear();
    setRunning(true);
    spdlog::info("WorkContextMonitor started (metadata only; no file contents).");
    return true;
}

void WorkContextMonitor::stop()
{
    setRunning(false);
    m_lastContextKey.clear();
}

QString WorkContextMonitor::cleanTitlePart(QString value)
{
    value = value.trimmed();
    value.remove(QRegularExpression(R"(^[●•*]\s*)"));
    return value.trimmed();
}

bool WorkContextMonitor::isEditor(const QString &processName, const QString &title)
{
    if (editorProcesses().contains(normalizedProcess(processName))) return true;
    const QString lowered = title.toLower();
    return lowered.endsWith("visual studio code")
        || lowered.endsWith("cursor")
        || lowered.contains(" - jetbrains ");
}

bool WorkContextMonitor::isDocumentApplication(const QString &processName)
{
    return documentProcesses().contains(normalizedProcess(processName));
}

QString WorkContextMonitor::editorName(const QString &processName, const QString &title)
{
    const QString process = normalizedProcess(processName);
    if (process == "code" || process == "code-insiders") return "Visual Studio Code";
    if (process == "cursor") return "Cursor";
    if (process == "vscodium" || process == "codium") return "VSCodium";
    if (process.contains("webstorm")) return "WebStorm";
    if (process.contains("pycharm")) return "PyCharm";
    if (process.contains("clion")) return "CLion";
    if (process.contains("rider")) return "Rider";
    if (process.contains("goland")) return "GoLand";
    if (process.contains("idea")) return "IntelliJ IDEA";
    if (process == "devenv") return "Visual Studio";
    if (title.contains("Visual Studio Code", Qt::CaseInsensitive)) return "Visual Studio Code";
    return QFileInfo(processName).completeBaseName();
}

QString WorkContextMonitor::documentKind(const QString &documentName)
{
    const QString suffix = QFileInfo(documentName).suffix().toLower();
    if (suffix == "pdf" || suffix == "epub") return "reading";
    if (suffix == "doc" || suffix == "docx" || suffix == "odt") return "document";
    if (suffix == "ppt" || suffix == "pptx" || suffix == "odp") return "presentation";
    if (suffix == "xls" || suffix == "xlsx" || suffix == "ods") return "spreadsheet";
    return "reference";
}

void WorkContextMonitor::processWindowEvent(const RawEvent &event)
{
    if (!isRunning() || event.type != EventType::WindowFocusChanged) return;
    if (event.windowTitle.trimmed().isEmpty()) return;

    if (m_trackEditors && isEditor(event.processName, event.windowTitle)) {
        QStringList parts = event.windowTitle.split(
            QRegularExpression(R"(\s+[-—]\s+)"), Qt::SkipEmptyParts);
        for (QString &part : parts) part = cleanTitlePart(part);

        const QString editor = editorName(event.processName, event.windowTitle);
        if (!parts.isEmpty()
            && (parts.last().contains(editor, Qt::CaseInsensitive)
                || parts.last().contains("JetBrains", Qt::CaseInsensitive))) {
            parts.removeLast();
        }

        const QString fileName = parts.value(0);
        const QString workspace = parts.size() > 1 ? parts.last() : QString();
        if (fileName.isEmpty() || fileName.compare("Welcome", Qt::CaseInsensitive) == 0)
            return;

        const QString key = "editor|" + editor + "|" + fileName + "|" + workspace;
        if (key == m_lastContextKey) return;
        m_lastContextKey = key;

        RawEvent context;
        context.timestamp = event.timestamp;
        context.type = EventType::EditorContextChanged;
        context.source = name();
        context.processName = event.processName;
        context.windowTitle = event.windowTitle;
        context.description = workspace.isEmpty()
            ? QString("Editing %1 in %2").arg(fileName, editor)
            : QString("Editing %1 in project %2 with %3").arg(fileName, workspace, editor);
        context.metadata["editor"] = editor;
        context.metadata["fileName"] = fileName;
        context.metadata["workspace"] = workspace;
        context.metadata["contentCaptured"] = false;
        if (QFileInfo(fileName).isAbsolute()) context.filePath = fileName;
        emit rawEventCaptured(context);
        return;
    }

    if (!m_trackDocuments) return;
    const QRegularExpressionMatch documentMatch =
        documentExpression().match(event.windowTitle);
    if (!documentMatch.hasMatch() && !isDocumentApplication(event.processName)) return;

    QString documentName = documentMatch.hasMatch()
        ? cleanTitlePart(documentMatch.captured(1))
        : cleanTitlePart(event.windowTitle.split(
              QRegularExpression(R"(\s+[-—]\s+)"), Qt::SkipEmptyParts).value(0));
    if (documentName.isEmpty()) return;

    const QString key = "document|" + normalizedProcess(event.processName) + "|" + documentName;
    if (key == m_lastContextKey) return;
    m_lastContextKey = key;

    RawEvent context;
    context.timestamp = event.timestamp;
    context.type = EventType::DocumentViewed;
    context.source = name();
    context.processName = event.processName;
    context.windowTitle = event.windowTitle;
    context.description = QString("Reference document: %1").arg(documentName);
    context.metadata["documentName"] = documentName;
    context.metadata["documentKind"] = documentKind(documentName);
    context.metadata["contentCaptured"] = false;
    if (QFileInfo(documentName).isAbsolute()) context.filePath = documentName;
    emit rawEventCaptured(context);
}
