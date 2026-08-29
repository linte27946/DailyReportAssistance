-- Migration 002: independent retention periods for activities and reports.

-- Preserve a user's former day-based activity preference by rounding it up
-- to whole months. Fresh installations already receive the defaults below.
INSERT OR IGNORE INTO app_settings (key, value)
SELECT 'activity_retention_months',
       CAST(MAX(1, (CAST(value AS INTEGER) + 29) / 30) AS TEXT)
FROM app_settings
WHERE key = 'data_retention_days';

INSERT OR IGNORE INTO app_settings (key, value) VALUES
    ('activity_retention_months', '3'),
    ('report_retention_months', '3');
