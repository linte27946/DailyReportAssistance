#include "ActivityClassifier.h"
#include <QFileInfo>
#include <QDir>
#include <QRegularExpression>
#include <spdlog/spdlog.h>

ActivityClassifier::ActivityClassifier(QObject *parent)
    : QObject(parent)
{
    initDefaultRules();
}

void ActivityClassifier::initDefaultRules()
{
    // Priority-ordered default rules (higher priority = evaluated first)
    m_rules = {
        // Build processes
        {"msbuild.exe", "", "", EventType::ProcessStarted, EventCategory::Building, 50},
        {"cl.exe", "", "", EventType::ProcessStarted, EventCategory::Building, 50},
        {"gcc.exe", "", "", EventType::ProcessStarted, EventCategory::Building, 50},
        {"g++.exe", "", "", EventType::ProcessStarted, EventCategory::Building, 50},
        {"clang.exe", "", "", EventType::ProcessStarted, EventCategory::Building, 50},
        {"rustc.exe", "", "", EventType::ProcessStarted, EventCategory::Building, 50},
        {"cargo.exe", "", "", EventType::ProcessStarted, EventCategory::Building, 50},
        {"dotnet.exe", "", "", EventType::ProcessStarted, EventCategory::Building, 45},
        {"cmake.exe", "", "", EventType::ProcessStarted, EventCategory::Building, 45},
        {"ninja.exe", "", "", EventType::ProcessStarted, EventCategory::Building, 45},
        {"make.exe", "", "", EventType::ProcessStarted, EventCategory::Building, 45},
        {"npm.cmd", "", "", EventType::ProcessStarted, EventCategory::Building, 40},
        {"node.exe", "", "", EventType::ProcessStarted, EventCategory::Building, 35},
        {"tsc.exe", "", "", EventType::ProcessStarted, EventCategory::Building, 40},

        // Git / VCS
        {"git.exe", "", "", EventType::ProcessStarted, EventCategory::VersionControl, 50},

        // IDEs → Coding
        {"devenv.exe", "", "", EventType::WindowFocusChanged, EventCategory::Coding, 40},
        {"code.exe", "", "", EventType::WindowFocusChanged, EventCategory::Coding, 40},
        {"idea64.exe", "", "", EventType::WindowFocusChanged, EventCategory::Coding, 40},
        {"pycharm64.exe", "", "", EventType::WindowFocusChanged, EventCategory::Coding, 40},
        {"notepad++.exe", "", "", EventType::WindowFocusChanged, EventCategory::Coding, 35},
        {"sublime_text.exe", "", "", EventType::WindowFocusChanged, EventCategory::Coding, 35},
        {"vim.exe", "", "", EventType::WindowFocusChanged, EventCategory::Coding, 35},

        // Debugging tools
        {"windbg.exe", "", "", EventType::WindowFocusChanged, EventCategory::Debugging, 50},
        {"devenv.exe", "", "", EventType::ProcessStarted, EventCategory::Debugging, 30},

        // Terminal → Coding (likely running dev commands)
        {"cmd.exe", "", "", EventType::WindowFocusChanged, EventCategory::Coding, 20},
        {"powershell.exe", "", "", EventType::WindowFocusChanged, EventCategory::Coding, 20},
        {"wt.exe", "", "", EventType::WindowFocusChanged, EventCategory::Coding, 20},

        // Browsers
        {"chrome.exe", "", "*docs.microsoft.com*", EventType::UrlVisited, EventCategory::Documentation, 60},
        {"chrome.exe", "", "*learn.microsoft.com*", EventType::UrlVisited, EventCategory::Documentation, 60},
        {"chrome.exe", "", "*developer.mozilla.org*", EventType::UrlVisited, EventCategory::Documentation, 60},
        {"chrome.exe", "", "*stackoverflow.com*", EventType::UrlVisited, EventCategory::Documentation, 55},
        {"chrome.exe", "", "*github.com*", EventType::UrlVisited, EventCategory::CodeReview, 50},
        {"msedge.exe", "", "*docs.microsoft.com*", EventType::UrlVisited, EventCategory::Documentation, 60},
        {"msedge.exe", "", "*github.com*", EventType::UrlVisited, EventCategory::CodeReview, 50},

        // Communication
        {"teams.exe", "", "", EventType::WindowFocusChanged, EventCategory::Communication, 45},
        {"slack.exe", "", "", EventType::WindowFocusChanged, EventCategory::Communication, 45},
        {"outlook.exe", "", "", EventType::WindowFocusChanged, EventCategory::Communication, 40},

        // File modifications → Coding
        {"", ".cpp", "", EventType::FileModified, EventCategory::Coding, 30},
        {"", ".h", "", EventType::FileModified, EventCategory::Coding, 30},
        {"", ".hpp", "", EventType::FileModified, EventCategory::Coding, 30},
        {"", ".c", "", EventType::FileModified, EventCategory::Coding, 30},
        {"", ".cs", "", EventType::FileModified, EventCategory::Coding, 30},
        {"", ".py", "", EventType::FileModified, EventCategory::Coding, 30},
        {"", ".js", "", EventType::FileModified, EventCategory::Coding, 30},
        {"", ".ts", "", EventType::FileModified, EventCategory::Coding, 30},
        {"", ".java", "", EventType::FileModified, EventCategory::Coding, 30},
        {"", ".rs", "", EventType::FileModified, EventCategory::Coding, 30},
        {"", ".go", "", EventType::FileModified, EventCategory::Coding, 30},
        {"", ".rb", "", EventType::FileModified, EventCategory::Coding, 25},
        {"", ".swift", "", EventType::FileModified, EventCategory::Coding, 30},
        {"", ".kt", "", EventType::FileModified, EventCategory::Coding, 30},
        {"", ".html", "", EventType::FileModified, EventCategory::Coding, 25},
        {"", ".css", "", EventType::FileModified, EventCategory::Coding, 25},
        {"", ".scss", "", EventType::FileModified, EventCategory::Coding, 25},

        // Config files → Coding
        {"", ".json", "", EventType::FileModified, EventCategory::Coding, 20},
        {"", ".xml", "", EventType::FileModified, EventCategory::Coding, 20},
        {"", ".yaml", "", EventType::FileModified, EventCategory::Coding, 20},
        {"", ".yml", "", EventType::FileModified, EventCategory::Coding, 20},
        {"", ".toml", "", EventType::FileModified, EventCategory::Coding, 20},

        // Docs → Documentation
        {"", ".md", "", EventType::FileModified, EventCategory::Documentation, 20},
        {"", ".rst", "", EventType::FileModified, EventCategory::Documentation, 20},
        {"", ".txt", "", EventType::FileModified, EventCategory::Documentation, 15},

        // Git events
        {"", "", "", EventType::GitCommit, EventCategory::VersionControl, 100},
        {"", "", "", EventType::GitPush, EventCategory::VersionControl, 100},
        {"", "", "", EventType::GitPull, EventCategory::VersionControl, 100},

        // Build events
        {"", "", "", EventType::BuildStarted, EventCategory::Building, 100},
        {"", "", "", EventType::BuildCompleted, EventCategory::Building, 100},

        // Idle events
        {"", "", "", EventType::UserIdle, EventCategory::Idle, 100},
        {"", "", "", EventType::UserActive, EventCategory::Coding, 5},

        // Default: Browsing for browser windows
        {"chrome.exe", "", "", EventType::WindowFocusChanged, EventCategory::Browsing, 10},
        {"msedge.exe", "", "", EventType::WindowFocusChanged, EventCategory::Browsing, 10},
        {"firefox.exe", "", "", EventType::WindowFocusChanged, EventCategory::Browsing, 10},
    };
    m_rulesSorted = false;
}

void ActivityClassifier::ensureRulesSorted()
{
    if (!m_rulesSorted) {
        std::sort(m_rules.begin(), m_rules.end(),
                  [](const ClassificationRule &a, const ClassificationRule &b) {
                      return a.priority > b.priority;
                  });
        m_rulesSorted = true;
    }
}

void ActivityClassifier::addRule(const ClassificationRule &rule)
{
    m_rules.append(rule);
    m_rulesSorted = false;
}

void ActivityClassifier::resetToDefaults()
{
    m_rules.clear();
    initDefaultRules();
}

void ActivityClassifier::loadRules(const QJsonObject &config)
{
    if (config.contains("rules")) {
        QJsonArray rulesArr = config["rules"].toArray();
        for (const auto &v : rulesArr) {
            QJsonObject r = v.toObject();
            ClassificationRule rule;
            rule.processName = r["process"].toString();
            rule.extension = r["extension"].toString();
            rule.urlPattern = r["urlPattern"].toString();
            rule.category = static_cast<EventCategory>(r["category"].toInt());
            rule.priority = r["priority"].toInt();
            rule.description = r["description"].toString();
            m_rules.append(rule);
        }
        m_rulesSorted = false;
    }
}

ActivityEvent ActivityClassifier::classify(const RawEvent &raw)
{
    ActivityEvent event;
    event.timestamp = raw.timestamp;
    event.type = raw.type;
    event.description = raw.description;
    event.application = raw.processName;
    event.windowTitle = raw.windowTitle;
    event.filePath = raw.filePath;
    event.url = raw.url;
    event.metadata = raw.metadata;

    if (!raw.filePath.isEmpty()) {
        QFileInfo fi(raw.filePath);
        event.fileExtension = fi.suffix().isEmpty() ? "" : "." + fi.suffix().toLower();
    }

    event.durationSecs = raw.metadata["durationSecs"].toInt(0);
    event.category = classifyInternal(raw);

    return event;
}

QList<ActivityEvent> ActivityClassifier::classifyBatch(const QList<RawEvent> &rawEvents)
{
    QList<ActivityEvent> results;
    results.reserve(rawEvents.size());
    for (const auto &raw : rawEvents) {
        results.append(classify(raw));
    }
    return results;
}

EventCategory ActivityClassifier::classifyInternal(const RawEvent &raw)
{
    ensureRulesSorted();

    QFileInfo fi(raw.filePath);
    QString ext = fi.suffix().isEmpty() ? "" : "." + fi.suffix().toLower();

    for (const auto &rule : m_rules) {
        // Check process name match (case-insensitive)
        if (!rule.processName.isEmpty()) {
            if (!raw.processName.isEmpty() &&
                !QRegularExpression(QRegularExpression::wildcardToRegularExpression(rule.processName),
                         QRegularExpression::CaseInsensitiveOption).match(raw.processName).hasMatch())
                continue;
        }

        // Check extension match
        if (!rule.extension.isEmpty()) {
            if (ext != rule.extension.toLower())
                continue;
        }

        // Check URL pattern match
        if (!rule.urlPattern.isEmpty()) {
            if (raw.url.isEmpty())
                continue;
            QRegularExpression urlRegex(QRegularExpression::wildcardToRegularExpression(rule.urlPattern), QRegularExpression::CaseInsensitiveOption);
            if (!urlRegex.match(raw.url).hasMatch() && !raw.url.contains(rule.urlPattern, Qt::CaseInsensitive))
                continue;
        }

        // Check event type match
        if (rule.eventType != EventType::Unknown) {
            if (raw.type != rule.eventType)
                continue;
        }

        // All specified conditions matched — this is our classification
        return rule.category;
    }

    // No rule matched — return a reasonable default
    return EventCategory::Other;
}
