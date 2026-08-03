#pragma once

#include "IMonitor.h"
#include <QTimer>
#include <QSet>
#include <QMutex>

#ifdef _WIN32
#include <windows.h>
#include <UIAutomation.h>
#endif

/// Monitors browser URL bar content using UI Automation.
/// Detects which URLs the user is browsing in Chrome, Edge, Firefox.
class BrowserUrlMonitor : public IMonitor {
    Q_OBJECT

public:
    explicit BrowserUrlMonitor(QObject *parent = nullptr);
    ~BrowserUrlMonitor() override;

    bool start() override;
    void stop() override;
    QString name() const override { return "BrowserUrlMonitor"; }

    /// Set the polling interval in milliseconds. Default: 2000.
    void setPollInterval(int ms) { m_pollIntervalMs = ms; }

    /// Set URL patterns that are considered "documentation" (for classification).
    void setDocUrlPatterns(const QSet<QString> &patterns);
    void addDocUrlPattern(const QString &pattern);

private:
    void pollBrowserUrl();

    /// Try to get the URL from a browser window using UI Automation.
    static QString getBrowserUrl(HWND hwnd);

    /// Check if a window belongs to a known browser.
    static bool isBrowserWindow(const QString &processName);

#ifdef _WIN32
    IUIAutomation *m_automation = nullptr;
    void initAutomation();
    void cleanupAutomation();
#endif

    QTimer *m_pollTimer = nullptr;
    QSet<QString> m_docUrlPatterns;
    QSet<QString> m_recentUrls;        // Dedup recent URLs
    QString m_currentUrl;
    QMutex m_mutex;
    int m_pollIntervalMs = 3000;

    // Known browser process names
    static const QSet<QString> &browserProcesses();

#ifdef _WIN32
    static BrowserUrlMonitor *s_instance_for_automation;
#endif
};
