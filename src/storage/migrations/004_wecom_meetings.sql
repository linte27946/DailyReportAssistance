-- Migration 004: read-only WeCom meeting attendance integration.

ALTER TABLE activity_events ADD COLUMN external_id TEXT;

CREATE UNIQUE INDEX IF NOT EXISTS idx_events_external_id
    ON activity_events(external_id)
    WHERE external_id IS NOT NULL AND external_id <> '';

INSERT OR IGNORE INTO app_settings (key, value) VALUES
    ('wecom_meeting_enabled', 'false'),
    ('wecom_cli_path', 'wecom-cli'),
    ('wecom_meeting_sync_minutes', '30'),
    ('wecom_meeting_idle_threshold_percent', '30');
