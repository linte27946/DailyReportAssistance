#include "BrowserUrlMonitor.h"
#include "WindowFocusMonitor.h"
#include <spdlog/spdlog.h>

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
        "brave.exe", "opera.exe", "iexplore.exe"
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
#ifdef _WIN32
    auto info = WindowFocusMonitor::getForegroundWindowInfo();

    if (!isBrowserWindow(info.processName)) return;

    QString url = getBrowserUrl(info.hwnd);
    if (url.isEmpty() || url == m_currentUrl) return;

    m_currentUrl = url;

    // Deduplicate: skip URLs we've recently seen
    {
        QMutexLocker lock(&m_mutex);
        if (m_recentUrls.contains(url)) return;
        m_recentUrls.insert(url);
        // Keep the recent set bounded
        if (m_recentUrls.size() > 200) {
            m_recentUrls.clear();
            m_recentUrls.insert(url);
        }
    }

    RawEvent event;
    event.timestamp = QDateTime::currentDateTimeUtc();
    event.type = EventType::UrlVisited;
    event.source = "BrowserUrlMonitor";
    event.processName = info.processName;
    event.windowTitle = info.title;
    event.url = url;
    event.description = QString("Browsing: %1").arg(url);
    event.metadata["url"] = url;
    event.metadata["processName"] = info.processName;

    // Check if it's a documentation URL
    {
        QMutexLocker lock(&m_mutex);
        for (const auto &pattern : m_docUrlPatterns) {
            if (url.contains(pattern, Qt::CaseInsensitive)) {
                event.metadata["isDocumentation"] = true;
                break;
            }
        }
    }

    emit rawEventCaptured(event);
#else
    Q_UNUSED(this);
#endif
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
