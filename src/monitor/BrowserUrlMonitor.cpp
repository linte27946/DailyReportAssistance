#include "BrowserUrlMonitor.h"
#include "WindowFocusMonitor.h"
#include <QRegularExpression>
#include <QUrl>
#include <spdlog/spdlog.h>

namespace {

bool domainMatches(const QString &host, const QString &domain)
{
    return host == domain || host.endsWith("." + domain);
}

bool containsAny(const QString &text, const QStringList &values)
{
    for (const QString &value : values) {
        if (text.contains(value, Qt::CaseInsensitive)) return true;
    }
    return false;
}

} // namespace

#ifdef _WIN32
#include <psapi.h>
#endif

#ifdef _WIN32
BrowserUrlMonitor *BrowserUrlMonitor::s_instance_for_automation = nullptr;
#endif

const QSet<QString> &BrowserUrlMonitor::browserProcesses()
{
    static const QSet<QString> procs = {
        "chrome.exe", "msedge.exe", "firefox.exe",
        "brave.exe", "opera.exe", "iexplore.exe",
        "chrome", "google-chrome", "chromium", "chromium-browser",
        "firefox", "brave", "microsoft-edge", "opera"
    };
    return procs;
}

BrowserUrlMonitor::BrowserUrlMonitor(QObject *parent)
    : IMonitor(parent)
{
    // Default documentation URL patterns
    m_docUrlPatterns = {
        "docs.microsoft.com", "learn.microsoft.com",
        "developer.mozilla.org", "mdn.dev",
        "stackoverflow.com", "stackexchange.com",
        "github.com", "gitlab.com",
        "cppreference.com", "en.cppreference.com",
        "w3schools.com",
        "medium.com", "dev.to",
        "reddit.com/r/programming", "reddit.com/r/cpp",
        "arxiv.org",
        "npmjs.com", "pypi.org", "crates.io",
        "documentation", "developer guide", "api reference", "tutorial", "MDN",
    };

    m_pollTimer = new QTimer(this);
    m_pollTimer->setInterval(m_pollIntervalMs);
    connect(m_pollTimer, &QTimer::timeout, this, &BrowserUrlMonitor::pollBrowserUrl);
}

BrowserUrlMonitor::~BrowserUrlMonitor()
{
    stop();
#ifdef _WIN32
    cleanupAutomation();
#endif
}

bool BrowserUrlMonitor::start()
{
    spdlog::info("BrowserUrlMonitor starting...");
#ifdef _WIN32
    s_instance_for_automation = this;
    initAutomation();
#endif
    m_pollTimer->start();
    setRunning(true);
    pollBrowserUrl();
    return true;
}

void BrowserUrlMonitor::stop()
{
    m_pollTimer->stop();
#ifdef _WIN32
    cleanupAutomation();
    s_instance_for_automation = nullptr;
#endif
    setRunning(false);
    spdlog::info("BrowserUrlMonitor stopped.");
}

void BrowserUrlMonitor::setDocUrlPatterns(const QSet<QString> &patterns)
{
    QMutexLocker lock(&m_mutex);
    m_docUrlPatterns = patterns;
}

void BrowserUrlMonitor::addDocUrlPattern(const QString &pattern)
{
    QMutexLocker lock(&m_mutex);
    m_docUrlPatterns.insert(pattern);
}

void BrowserUrlMonitor::pollBrowserUrl()
{
    auto info = WindowFocusMonitor::getForegroundWindowInfo();

    if (!isBrowserWindow(info.processName)) return;

    QString rawUrl;
#ifdef _WIN32
    rawUrl = getBrowserUrl(info.hwnd);
#endif
    const QString url = sanitizeUrl(rawUrl, m_captureFullUrl);
    QString pageTitle = info.title.trimmed();
    pageTitle.remove(QRegularExpression(
        R"(\s+[-—]\s+(Google Chrome|Microsoft Edge|Mozilla Firefox|Brave|Opera)\s*$)",
        QRegularExpression::CaseInsensitiveOption));
    const QString pageKey = url + "|" + pageTitle;
    if (pageKey == "|" || pageKey == m_currentPageKey) return;

    const QString entertainmentKind = distractionKind(url, pageTitle);
    if (!entertainmentKind.isEmpty() && !m_trackDistractions) {
        // Keep the generic foreground observation used for wall-clock timing,
        // but do not create a URL-level entertainment record without consent.
        m_currentPageKey = pageKey;
        return;
    }

    m_currentPageKey = pageKey;

    // Deduplicate: skip URLs we've recently seen
    {
        QMutexLocker lock(&m_mutex);
        if (m_recentPages.contains(pageKey)) return;
        m_recentPages.insert(pageKey);
        // Keep the recent set bounded
        if (m_recentPages.size() > 500) {
            m_recentPages.clear();
            m_recentPages.insert(pageKey);
        }
    }

    RawEvent event;
    event.timestamp = QDateTime::currentDateTimeUtc();
    event.type = EventType::UrlVisited;
    event.source = "BrowserUrlMonitor";
    event.processName = info.processName;
    event.windowTitle = info.title;
    event.url = url;
    const QString host = QUrl(url).host();
    if (!pageTitle.isEmpty() && !host.isEmpty())
        event.description = QString("Browser page: %1 (%2)").arg(pageTitle, host);
    else if (!pageTitle.isEmpty())
        event.description = QString("Browser page: %1").arg(pageTitle);
    else
        event.description = QString("Browsing: %1").arg(url);
    event.metadata["url"] = url;
    event.metadata["domain"] = host;
    event.metadata["pageTitle"] = pageTitle;
    event.metadata["processName"] = info.processName;
    event.metadata["queryCaptured"] = m_captureFullUrl;
    if (!entertainmentKind.isEmpty()) {
        event.metadata["isDistraction"] = true;
        event.metadata["distractionKind"] = entertainmentKind;
        event.description = QString("Entertainment page: %1%2")
            .arg(pageTitle.isEmpty() ? host : pageTitle,
                 host.isEmpty() ? QString() : QString(" (%1)").arg(host));
    }

    // Check if it's a documentation URL
    {
        QMutexLocker lock(&m_mutex);
        for (const auto &pattern : m_docUrlPatterns) {
            if (url.contains(pattern, Qt::CaseInsensitive)
                || pageTitle.contains(pattern, Qt::CaseInsensitive)) {
                event.metadata["isDocumentation"] = true;
                break;
            }
        }
    }

    emit rawEventCaptured(event);
}

QString BrowserUrlMonitor::sanitizeUrl(const QString &url, bool includeQuery)
{
    QUrl parsed = QUrl::fromUserInput(url.trimmed());
    if (!parsed.isValid()) return {};
    const QString scheme = parsed.scheme().toLower();
    if (scheme != "http" && scheme != "https") return {};

    parsed.setUserInfo({});
    parsed.setFragment({});
    if (!includeQuery) parsed.setQuery({});
    return parsed.toString(QUrl::FullyEncoded);
}

QString BrowserUrlMonitor::distractionKind(const QString &url,
                                           const QString &pageTitle)
{
    const QUrl parsed(url);
    const QString host = parsed.host().toLower();
    const QString path = parsed.path().toLower();
    const QString combined = pageTitle + " " + path;

    const QStringList liveDomains = {
        "douyu.com", "huya.com", "twitch.tv", "live.bilibili.com",
        "live.douyin.com", "cc.163.com", "yy.com"
    };
    for (const QString &domain : liveDomains) {
        if (domainMatches(host, domain)) return "live_stream";
    }
    if (containsAny(combined,
                    {"游戏直播", "直播间", "game stream", "gaming live",
                     "live stream", "正在直播"})) {
        return "live_stream";
    }

    const QStringList videoDomains = {
        "iqiyi.com", "v.qq.com", "youku.com", "mgtv.com", "netflix.com",
        "disneyplus.com", "primevideo.com"
    };
    for (const QString &domain : videoDomains) {
        if (domainMatches(host, domain)) return "video";
    }

    const QStringList gamingDomains = {
        "steampowered.com", "steamcommunity.com", "epicgames.com",
        "ign.com", "gamespot.com", "gamersky.com", "3dmgame.com"
    };
    for (const QString &domain : gamingDomains) {
        if (domainMatches(host, domain)) return "gaming";
    }
    if ((domainMatches(host, "bilibili.com")
         || domainMatches(host, "youtube.com"))
        && containsAny(combined, {"游戏", "gameplay", "gaming", "电竞"})) {
        return "gaming";
    }

    return {};
}

bool BrowserUrlMonitor::isBrowserWindow(const QString &processName)
{
    return browserProcesses().contains(processName.toLower());
}

#ifdef _WIN32

void BrowserUrlMonitor::initAutomation()
{
    HRESULT hr = CoCreateInstance(CLSID_CUIAutomation, nullptr,
                                   CLSCTX_INPROC_SERVER,
                                   IID_IUIAutomation,
                                   (void**)&m_automation);
    if (FAILED(hr)) {
        spdlog::warn("BrowserUrlMonitor: UI Automation init failed (0x{:08X}). "
                     "Browser URL tracking disabled.", (unsigned)hr);
        m_automation = nullptr;
    }
}

void BrowserUrlMonitor::cleanupAutomation()
{
    if (m_automation) {
        m_automation->Release();
        m_automation = nullptr;
    }
}

QString BrowserUrlMonitor::getBrowserUrl(HWND hwnd)
{
    // Try to read the browser address bar via UI Automation
    // Different browsers expose the address bar differently
    if (!s_instance_for_automation) return {};

    IUIAutomation *automation = s_instance_for_automation->m_automation;
    if (!automation) return {};

    IUIAutomationElement *element = nullptr;
    HRESULT hr = automation->ElementFromHandle(hwnd, &element);
    if (FAILED(hr) || !element) return {};

    // Create a condition to find the edit control (address bar)
    VARIANT editVar;
    editVar.vt = VT_I4;
    editVar.lVal = UIA_EditControlTypeId;

    IUIAutomationCondition *editCondition = nullptr;
    hr = automation->CreatePropertyCondition(UIA_ControlTypePropertyId, editVar, &editCondition);
    if (FAILED(hr) || !editCondition) {
        element->Release();
        return {};
    }

    // Search in the subtree for edit controls (address bar is usually the first one)
    IUIAutomationElementArray *editElements = nullptr;
    hr = element->FindAll(TreeScope_Subtree, editCondition, &editElements);
    editCondition->Release();

    if (FAILED(hr) || !editElements) {
        element->Release();
        return {};
    }

    int count = 0;
    editElements->get_Length(&count);

    QString url;
    for (int i = 0; i < count && url.isEmpty(); ++i) {
        IUIAutomationElement *editEl = nullptr;
        hr = editElements->GetElement(i, &editEl);
        if (FAILED(hr) || !editEl) continue;

        // Get the value pattern to read the text
        IUIAutomationValuePattern *valuePattern = nullptr;
        hr = editEl->GetCurrentPatternAs(UIA_ValuePatternId,
                                          IID_IUIAutomationValuePattern,
                                          (void**)&valuePattern);
        if (SUCCEEDED(hr) && valuePattern) {
            BSTR bstrValue = nullptr;
            hr = valuePattern->get_CurrentValue(&bstrValue);
            if (SUCCEEDED(hr) && bstrValue) {
                QString text = QString::fromWCharArray(bstrValue);
                // Basic URL detection: starts with http or contains common URL patterns
                if (text.startsWith("http://") || text.startsWith("https://") ||
                    text.startsWith("www.") || text.contains("://")) {
                    url = text;
                }
                SysFreeString(bstrValue);
            }
            valuePattern->Release();
        }
        editEl->Release();
    }

    editElements->Release();
    element->Release();
    return url;
}

#else

QString BrowserUrlMonitor::getBrowserUrl(void *)
{
    return {};
}

#endif // _WIN32
