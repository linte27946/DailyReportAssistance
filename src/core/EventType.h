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
