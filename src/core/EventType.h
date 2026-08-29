#pragma once

#include <QString>
#include <cstdint>

/// Categories of developer activity for high-level grouping.
enum class EventCategory : uint8_t {
    Unknown = 0,
    Coding,         // Writing/editing source code
    CodeReview,     // Reviewing code (PRs, diffs)
    Debugging,      // Running debugger, stepping through code
    Building,       // Compiling, linking, building
    Testing,        // Running tests
    Documentation,  // Reading docs, browsing references
    Communication,  // Email, Slack, Teams, etc.
    VersionControl, // Git commit, push, pull, merge
    Browsing,       // General web browsing
    Idle,           // No activity detected
    Other
};

/// Fine-grained event types produced by monitors.
enum class EventType : uint8_t {
    // File events
    FileCreated,
    FileModified,
    FileDeleted,
    FileRenamed,

    // Process events
    ProcessStarted,
    ProcessEnded,

    // Window events
    WindowFocusChanged,
    EditorContextChanged,
    DocumentViewed,

    // Input events
    UserActive,
    UserIdle,

    // Browser events
    UrlVisited,

    // Git events
    GitCommit,
    GitPush,
    GitPull,
    GitBranchSwitch,
    GitMerge,

    // Build events
    BuildStarted,
    BuildCompleted,

    // Other
    SessionStarted,
    SessionEnded,
    Unknown
};

/// Convert EventCategory to human-readable string.
inline QString eventCategoryToString(EventCategory cat)
{
    switch (cat) {
    case EventCategory::Coding:         return "Coding";
    case EventCategory::CodeReview:     return "Code Review";
    case EventCategory::Debugging:      return "Debugging";
    case EventCategory::Building:       return "Building";
    case EventCategory::Testing:        return "Testing";
    case EventCategory::Documentation:  return "Documentation";
    case EventCategory::Communication:  return "Communication";
    case EventCategory::VersionControl: return "Version Control";
    case EventCategory::Browsing:       return "Browsing";
    case EventCategory::Idle:           return "Idle";
    case EventCategory::Other:          return "Other";
    default:                            return "Unknown";
    }
}

/// Convert EventType to human-readable string.
inline QString eventTypeToString(EventType type)
{
    switch (type) {
    case EventType::FileCreated:        return "FileCreated";
    case EventType::FileModified:       return "FileModified";
    case EventType::FileDeleted:        return "FileDeleted";
    case EventType::FileRenamed:        return "FileRenamed";
    case EventType::ProcessStarted:     return "ProcessStarted";
    case EventType::ProcessEnded:       return "ProcessEnded";
    case EventType::WindowFocusChanged: return "WindowFocusChanged";
    case EventType::EditorContextChanged:return "EditorContextChanged";
    case EventType::DocumentViewed:     return "DocumentViewed";
    case EventType::UserActive:         return "UserActive";
    case EventType::UserIdle:           return "UserIdle";
    case EventType::UrlVisited:         return "UrlVisited";
    case EventType::GitCommit:          return "GitCommit";
    case EventType::GitPush:            return "GitPush";
    case EventType::GitPull:            return "GitPull";
    case EventType::GitBranchSwitch:    return "GitBranchSwitch";
    case EventType::GitMerge:           return "GitMerge";
    case EventType::BuildStarted:       return "BuildStarted";
    case EventType::BuildCompleted:     return "BuildCompleted";
    case EventType::SessionStarted:     return "SessionStarted";
    case EventType::SessionEnded:       return "SessionEnded";
    default:                            return "Unknown";
    }
}

/// Parse a persisted EventCategory value. Unknown strings remain Unknown so a
/// database row cannot silently turn into an unrelated activity category.
inline EventCategory eventCategoryFromString(const QString &value)
{
    if (value == "Coding") return EventCategory::Coding;
    if (value == "Code Review") return EventCategory::CodeReview;
    if (value == "Debugging") return EventCategory::Debugging;
    if (value == "Building") return EventCategory::Building;
    if (value == "Testing") return EventCategory::Testing;
    if (value == "Documentation") return EventCategory::Documentation;
    if (value == "Communication") return EventCategory::Communication;
    if (value == "Version Control") return EventCategory::VersionControl;
    if (value == "Browsing") return EventCategory::Browsing;
    if (value == "Idle") return EventCategory::Idle;
    if (value == "Other") return EventCategory::Other;
    return EventCategory::Unknown;
}

/// Parse a persisted EventType value.
inline EventType eventTypeFromString(const QString &value)
{
    if (value == "FileCreated") return EventType::FileCreated;
    if (value == "FileModified") return EventType::FileModified;
    if (value == "FileDeleted") return EventType::FileDeleted;
    if (value == "FileRenamed") return EventType::FileRenamed;
    if (value == "ProcessStarted") return EventType::ProcessStarted;
    if (value == "ProcessEnded") return EventType::ProcessEnded;
    if (value == "WindowFocusChanged") return EventType::WindowFocusChanged;
    if (value == "EditorContextChanged") return EventType::EditorContextChanged;
    if (value == "DocumentViewed") return EventType::DocumentViewed;
    if (value == "UserActive") return EventType::UserActive;
    if (value == "UserIdle") return EventType::UserIdle;
    if (value == "UrlVisited") return EventType::UrlVisited;
    if (value == "GitCommit") return EventType::GitCommit;
    if (value == "GitPush") return EventType::GitPush;
    if (value == "GitPull") return EventType::GitPull;
    if (value == "GitBranchSwitch") return EventType::GitBranchSwitch;
    if (value == "GitMerge") return EventType::GitMerge;
    if (value == "BuildStarted") return EventType::BuildStarted;
    if (value == "BuildCompleted") return EventType::BuildCompleted;
    if (value == "SessionStarted") return EventType::SessionStarted;
    if (value == "SessionEnded") return EventType::SessionEnded;
    return EventType::Unknown;
}
