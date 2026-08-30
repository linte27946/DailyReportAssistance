-- Migration 003: optional personal/entertainment browsing classification.

INSERT OR IGNORE INTO app_settings (key, value) VALUES
    ('distraction_tracking_enabled', 'false');
