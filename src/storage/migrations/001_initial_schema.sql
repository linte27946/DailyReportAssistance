-- Migration 001: Initial schema for DailyReport

-- Schema version tracking
CREATE TABLE IF NOT EXISTS schema_version (
    version     INTEGER PRIMARY KEY,
    applied_at  TEXT NOT NULL DEFAULT (datetime('now')),
    description TEXT
);

-- Core activity events table
CREATE TABLE IF NOT EXISTS activity_events (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    timestamp       TEXT NOT NULL,          -- ISO-8601 UTC
    end_timestamp   TEXT,                   -- ISO-8601 UTC (for duration events)
    event_type      TEXT NOT NULL,          -- e.g., 'FileModified', 'GitCommit'
    category        TEXT NOT NULL,          -- e.g., 'Coding', 'Debugging'
    description     TEXT NOT NULL,          -- Human-readable summary
    application     TEXT,                   -- Process name (e.g., 'devenv.exe')
    window_title    TEXT,                   -- Active window title at event time
    file_path       TEXT,                   -- Full path if file-related
    file_extension  TEXT,                   -- Extracted extension
    url             TEXT,                   -- URL for browser events
    duration_secs   INTEGER DEFAULT 0,      -- Duration in seconds
    metadata_json   TEXT DEFAULT '{}',      -- Flexible JSON blob
    session_id      TEXT,                   -- UUID linking events to a login session
    created_at      TEXT NOT NULL DEFAULT (datetime('now'))
);

-- Indexes for common query patterns
CREATE INDEX IF NOT EXISTS idx_events_timestamp ON activity_events(timestamp);
CREATE INDEX IF NOT EXISTS idx_events_date ON activity_events(date(timestamp));
CREATE INDEX IF NOT EXISTS idx_events_category ON activity_events(category);
CREATE INDEX IF NOT EXISTS idx_events_session ON activity_events(session_id);
CREATE INDEX IF NOT EXISTS idx_events_type ON activity_events(event_type);

-- Key-value settings store
CREATE TABLE IF NOT EXISTS app_settings (
    key         TEXT PRIMARY KEY,
    value       TEXT NOT NULL,
    updated_at  TEXT NOT NULL DEFAULT (datetime('now'))
);

-- Generated reports
CREATE TABLE IF NOT EXISTS reports (
    id                  INTEGER PRIMARY KEY AUTOINCREMENT,
    report_type         TEXT NOT NULL,          -- 'daily', 'weekly', 'custom'
    report_date         TEXT NOT NULL,          -- Date the report covers (ISO-8601)
    title               TEXT NOT NULL,
    content_md          TEXT NOT NULL,          -- Markdown content from LLM
    llm_backend         TEXT,                   -- 'openai', 'anthropic', 'ollama'
    llm_model           TEXT,                   -- e.g., 'gpt-4o', 'claude-sonnet-4-20250514'
    generation_time_secs REAL DEFAULT 0,
    token_count         INTEGER DEFAULT 0,
    created_at          TEXT NOT NULL DEFAULT (datetime('now'))
);

CREATE INDEX IF NOT EXISTS idx_reports_date ON reports(report_date);
CREATE INDEX IF NOT EXISTS idx_reports_type ON reports(report_type);

-- Report templates
CREATE TABLE IF NOT EXISTS report_templates (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    name        TEXT NOT NULL UNIQUE,
    description TEXT,
    content_md  TEXT NOT NULL,
    is_default  INTEGER DEFAULT 0,
    created_at  TEXT NOT NULL DEFAULT (datetime('now'))
);

-- Monitored project directories
CREATE TABLE IF NOT EXISTS monitored_paths (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    path        TEXT NOT NULL UNIQUE,
    label       TEXT,
    is_active   INTEGER DEFAULT 1,
    added_at    TEXT NOT NULL DEFAULT (datetime('now'))
);

-- Insert default prompts/settings
INSERT OR IGNORE INTO app_settings (key, value) VALUES
    ('auto_start', 'true'),
    ('start_minimized', 'true'),
    ('afk_threshold_secs', '300'),
    ('data_retention_days', '90'),
    ('monitoring_enabled', 'true'),
    ('git_tracking_enabled', 'true'),
    ('browser_tracking_enabled', 'true'),
    ('build_tracking_enabled', 'true'),
    ('daily_report_time', '17:30'),
    ('weekly_report_day', '5'),
    ('weekly_report_time', '17:00'),
    ('language', 'zh-CN'),
    ('classification_rules', '{}'),
    ('llm_backend', ''),
    ('llm_config', '{}');
