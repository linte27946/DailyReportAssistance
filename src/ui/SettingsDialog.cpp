#include "SettingsDialog.h"
#include "UiLanguage.h"
#include "DialogUtils.h"
#include "storage/SettingsRepository.h"
#include "report/TemplateEngine.h"
#include "app/DataRetentionService.h"
#include "monitor/WeComMeetingMonitor.h"
#include "util/CryptoUtils.h"
#include "util/WinUtils.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QPushButton>
#include <QLabel>
#include <QJsonArray>
#include <QJsonDocument>
#include <QDateTime>
#include <QDir>
#include <QFrame>
#include <QScrollArea>
#include <QTimer>
#include <QProcess>
#include <QRegularExpression>
#include <QStyle>
#include <QStandardPaths>
#include <spdlog/spdlog.h>

namespace {

void addLocalizedRow(QFormLayout *layout,
                     const QString &english, const QString &chinese,
                     QWidget *field)
{
    auto *label = new QLabel();
    UiLanguage::bindText(label, english, chinese);
    layout->addRow(label, field);
}

QScrollArea *makeSettingsPage(QWidget *content)
{
    auto *scrollArea = new QScrollArea();
    scrollArea->setObjectName("settingsScrollArea");
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setWidget(content);
    return scrollArea;
}

} // namespace

SettingsDialog::SettingsDialog(SettingsRepository *settings,
                               TemplateEngine *templateEngine,
                               DataRetentionService *retentionService,
                               WeComMeetingMonitor *weComMeetingMonitor,
                               QWidget *parent)
    : QWidget(parent)
    , m_settings(settings)
    , m_templateEngine(templateEngine)
    , m_retentionService(retentionService)
    , m_weComMeetingMonitor(weComMeetingMonitor)
{
    setupUi();

    m_weComAuthProcess = new QProcess(this);
    m_weComAuthTimeout = new QTimer(this);
    m_weComAuthTimeout->setSingleShot(true);
    connect(m_weComAuthProcess,
            qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this, &SettingsDialog::finishWeComAuthorizationCheck);
    connect(m_weComAuthProcess, &QProcess::errorOccurred, this,
            [this](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart || !m_weComAuthChecking) return;
        m_weComAuthChecking = false;
        m_weComAuthorized = false;
        m_weComAuthTimeout->stop();
        setWeComStatus(
            "error",
            QString("Authorization check could not start: %1")
                .arg(m_weComAuthProcess->errorString()),
            QString("无法启动授权检测：%1")
                .arg(m_weComAuthProcess->errorString()));
        updateWeComActionState();
    });
    connect(m_weComAuthTimeout, &QTimer::timeout, this, [this]() {
        if (!m_weComAuthChecking) return;
        m_weComAuthChecking = false;
        m_weComAuthorized = false;
        m_weComAuthProcess->kill();
        setWeComStatus(
            "error",
            "Authorization check timed out. Check the CLI and network, then try again.",
            "授权检测超时，请检查 CLI 和网络后重试。");
        updateWeComActionState();
    });

    loadSettings();

    if (m_weComMeetingMonitor) {
        connect(m_weComMeetingMonitor, &WeComMeetingMonitor::syncStarted,
                this, [this]() {
            m_weComSyncing = true;
            UiLanguage::bindText(m_weComSyncStatusLabel,
                "Synchronizing actual attendance…", "正在同步实际参会记录……");
            updateWeComActionState();
        });
        connect(m_weComMeetingMonitor, &WeComMeetingMonitor::syncFinished,
                this, [this](int count) {
            m_weComSyncing = false;
            UiLanguage::bindText(m_weComSyncStatusLabel,
                QString("Sync complete · %1 actual attendance records found").arg(count),
                QString("同步完成 · 找到 %1 条实际参会记录").arg(count));
            updateWeComActionState();
        });
        connect(m_weComMeetingMonitor, &WeComMeetingMonitor::syncFailed,
                this, [this](const QString &message) {
            m_weComSyncing = false;
            UiLanguage::bindText(m_weComSyncStatusLabel,
                QString("Sync unavailable: %1").arg(message),
                QString("暂时无法同步：%1").arg(message));
            updateWeComActionState();
            QTimer::singleShot(0, this,
                               &SettingsDialog::checkWeComAuthorization);
        });
    }

    if (m_retentionService) {
        connect(m_retentionService, &DataRetentionService::cleanupFinished,
                this,
                [this](int activityDeleted, int reportsDeleted,
                       const QDate &activityCutoff, const QDate &reportCutoff,
                       const QDateTime &completedAt, bool userInitiated) {
            if (activityDeleted < 0 || reportsDeleted < 0) {
                m_cleanupStatusLabel->setText(UiLanguage::text(
                    "The last cleanup failed. Check the application log.",
                    "上次清理失败，请检查应用日志。"));
                if (userInitiated) {
                    DialogUtils::warning(
                        this, UiLanguage::text("Cleanup failed", "清理失败"),
                        UiLanguage::text(
                            "Some expired data could not be removed. Check the log for details.",
                            "部分过期数据未能删除，请检查日志了解详情。"));
                }
                return;
            }

            m_cleanupStatusLabel->setText(UiLanguage::text(
                QString("Last cleanup: %1 · %2 activities and %3 reports removed")
                    .arg(completedAt.toString("yyyy-MM-dd HH:mm"))
                    .arg(activityDeleted).arg(reportsDeleted),
                QString("上次清理：%1 · 已删除 %2 条活动和 %3 份报告")
                    .arg(completedAt.toString("yyyy-MM-dd HH:mm"))
                    .arg(activityDeleted).arg(reportsDeleted)));

            if (userInitiated) {
                DialogUtils::information(
                    this, UiLanguage::text("Cleanup complete", "清理完成"),
                    UiLanguage::text(
                        QString("Removed %1 expired activities (before %2) and "
                                "%3 expired reports (before %4).")
                            .arg(activityDeleted)
                            .arg(activityCutoff.toString(Qt::ISODate))
                            .arg(reportsDeleted)
                            .arg(reportCutoff.toString(Qt::ISODate)),
                        QString("已删除 %1 条过期活动（早于 %2）和 %3 份过期报告（早于 %4）。")
                            .arg(activityDeleted)
                            .arg(activityCutoff.toString(Qt::ISODate))
                            .arg(reportsDeleted)
                            .arg(reportCutoff.toString(Qt::ISODate))));
            }
        });
    }
}

void SettingsDialog::setupUi()
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(18, 16, 18, 16);
    mainLayout->setSpacing(12);
    auto *tabWidget = new QTabWidget(this);
    tabWidget->setObjectName("settingsTabs");
    tabWidget->setTabPosition(QTabWidget::North);
    tabWidget->setDocumentMode(true);
    tabWidget->setUsesScrollButtons(true);

    // === General Tab ===
    auto *generalTab = new QWidget();
    auto *generalLayout = new QVBoxLayout(generalTab);
    generalLayout->setContentsMargins(20, 16, 20, 20);
    generalLayout->setSpacing(14);

    auto *startupGroup = new QGroupBox(generalTab);
    UiLanguage::bindText(startupGroup, "Startup", "启动");
    auto *startupForm = new QFormLayout(startupGroup);
    m_autoStartChk = new QCheckBox();
#ifndef _WIN32
    UiLanguage::bindText(m_autoStartChk,
                         "Start automatically after login", "登录后自动启动");
#else
    UiLanguage::bindText(m_autoStartChk, "Start with Windows", "随 Windows 启动");
#endif
    m_startMinimizedChk = new QCheckBox();
    UiLanguage::bindText(m_startMinimizedChk,
                         "Start minimized to tray", "启动后最小化到托盘");
    m_afkThresholdSpin = new QSpinBox();
    m_afkThresholdSpin->setRange(60, 3600);
    UiLanguage::bindSuffix(m_afkThresholdSpin, " seconds", " 秒");
    startupForm->addRow(m_autoStartChk);
    startupForm->addRow(m_startMinimizedChk);
    addLocalizedRow(startupForm, "AFK threshold:", "离开状态阈值：", m_afkThresholdSpin);

    generalLayout->addWidget(startupGroup);
    generalLayout->addStretch();

    // === Monitoring Tab ===
    auto *monitorTab = new QWidget();
    auto *monitorLayout = new QVBoxLayout(monitorTab);
    monitorLayout->setContentsMargins(20, 16, 20, 20);
    monitorLayout->setSpacing(14);

    auto *pathsGroup = new QGroupBox(monitorTab);
    UiLanguage::bindText(pathsGroup, "Project directories", "项目目录");
    auto *pathsLayout = new QVBoxLayout(pathsGroup);
    m_projectPathsList = new QListWidget();
    auto *pathBtnLayout = new QHBoxLayout();
    auto *addPathBtn = new QPushButton();
    auto *removePathBtn = new QPushButton();
    UiLanguage::bindText(addPathBtn, "Add directory", "添加目录");
    UiLanguage::bindText(removePathBtn, "Remove selected", "移除所选目录");
    pathBtnLayout->addWidget(addPathBtn);
    pathBtnLayout->addWidget(removePathBtn);
    pathBtnLayout->addStretch();
    pathsLayout->addWidget(m_projectPathsList);
    pathsLayout->addLayout(pathBtnLayout);

    connect(addPathBtn, &QPushButton::clicked, this, [this]() {
        const QString dir = DialogUtils::selectDirectory(
            this, UiLanguage::text("Select project directory", "选择项目目录"));
        if (!dir.isEmpty())
            m_projectPathsList->addItem(dir);
    });
    connect(removePathBtn, &QPushButton::clicked, this, [this]() {
        delete m_projectPathsList->currentItem();
    });

    auto *featuresGroup = new QGroupBox(monitorTab);
    UiLanguage::bindText(featuresGroup, "Tracking features", "监控功能");
    auto *featuresForm = new QFormLayout(featuresGroup);
    m_gitTrackingChk = new QCheckBox();
    m_browserTrackingChk = new QCheckBox();
    m_browserFullUrlChk = new QCheckBox();
    m_distractionTrackingChk = new QCheckBox();
    m_buildTrackingChk = new QCheckBox();
    m_editorTrackingChk = new QCheckBox();
    m_documentTrackingChk = new QCheckBox();
    UiLanguage::bindText(m_gitTrackingChk, "Enable Git tracking", "启用 Git 监控");
    UiLanguage::bindText(m_browserTrackingChk,
                         "Track browser pages", "记录浏览器页面");
    UiLanguage::bindText(
        m_browserFullUrlChk,
        "Include URL query strings (may contain private data)",
        "保留网址查询参数（可能包含隐私数据）");
    m_distractionTrackingChk->setObjectName("privacyOption");
    UiLanguage::bindText(
        m_distractionTrackingChk,
        "Record entertainment and distraction browsing",
        "记录娱乐与摸鱼浏览");
    UiLanguage::bindTooltip(
        m_distractionTrackingChk,
        "Recognizes common live-streaming, gaming, and entertainment pages. "
        "Only the page title and privacy-filtered URL are stored.",
        "识别常见直播、游戏和娱乐网站，仅保存页面标题和经过隐私过滤的网址。");
    UiLanguage::bindText(m_buildTrackingChk,
                         "Enable build/compile tracking", "启用构建与编译监控");
    UiLanguage::bindText(m_editorTrackingChk,
                         "Track editor file and project context", "记录编辑器文件与项目上下文");
    UiLanguage::bindText(m_documentTrackingChk,
                         "Track opened PDF and Office document names", "记录打开的 PDF 与 Office 文档名称");
    featuresForm->addRow(m_gitTrackingChk);
    featuresForm->addRow(m_browserTrackingChk);
    featuresForm->addRow(m_browserFullUrlChk);
    featuresForm->addRow(m_distractionTrackingChk);
    featuresForm->addRow(m_buildTrackingChk);
    featuresForm->addRow(m_editorTrackingChk);
    featuresForm->addRow(m_documentTrackingChk);
    auto *monitorRestartHint = new QLabel();
    monitorRestartHint->setObjectName("fieldHint");
    monitorRestartHint->setWordWrap(true);
    UiLanguage::bindText(
        monitorRestartHint,
        "Tracking feature changes take effect after DailyReport is restarted.",
        "监控功能的修改会在重启 DailyReport 后生效。");
    featuresForm->addRow(monitorRestartHint);

    auto updateBrowserOptions = [this](bool browserEnabled) {
        m_browserFullUrlChk->setEnabled(browserEnabled);
        m_distractionTrackingChk->setEnabled(browserEnabled);
    };
    connect(m_browserTrackingChk, &QCheckBox::toggled,
            this, updateBrowserOptions);

    monitorLayout->addWidget(pathsGroup);
    monitorLayout->addWidget(featuresGroup);
    monitorLayout->addStretch();

    // === Integrations Tab ===
    auto *integrationsTab = new QWidget();
    auto *integrationsLayout = new QVBoxLayout(integrationsTab);
    integrationsLayout->setContentsMargins(20, 16, 20, 20);
    integrationsLayout->setSpacing(14);

    auto *integrationIntro = new QLabel(integrationsTab);
    integrationIntro->setObjectName("settingsHint");
    integrationIntro->setWordWrap(true);
    UiLanguage::bindText(
        integrationIntro,
        "Meeting data is read through the official WeCom CLI. Reservations are ignored; "
        "only records with your actual enter and quit times can become meeting activity.",
        "会议数据通过企业微信官方 CLI 只读获取。程序会忽略单纯的预约，只有包含本人实际入会和离会时间的记录才可能计为会议活动。");

    auto *weComGroup = new QGroupBox(integrationsTab);
    UiLanguage::bindText(weComGroup, "WeCom meetings", "企业微信会议");
    auto *weComForm = new QFormLayout(weComGroup);
    m_weComMeetingChk = new QCheckBox(weComGroup);
    UiLanguage::bindText(m_weComMeetingChk,
                         "Import actual meeting attendance", "导入实际参会记录");
    m_weComCliPathEdit = new QLineEdit(weComGroup);
    UiLanguage::bindPlaceholder(m_weComCliPathEdit,
                                "wecom-cli or an absolute path",
                                "wecom-cli 或程序的绝对路径");
    m_weComSyncIntervalSpin = new QSpinBox(weComGroup);
    m_weComSyncIntervalSpin->setRange(5, 1440);
    UiLanguage::bindSuffix(m_weComSyncIntervalSpin, " minutes", " 分钟");
    m_weComIdleThresholdSpin = new QSpinBox(weComGroup);
    m_weComIdleThresholdSpin->setRange(1, 99);
    m_weComIdleThresholdSpin->setSuffix("%");
    m_weComIdleThresholdSpin->setToolTip(UiLanguage::text(
        "Meeting time is added only when idle time is strictly greater than this percentage.",
        "只有空闲占比严格大于该值时才补记会议时间。"));
    weComForm->addRow(m_weComMeetingChk);
    addLocalizedRow(weComForm, "CLI executable:", "CLI 程序：", m_weComCliPathEdit);
    addLocalizedRow(weComForm, "Automatic sync interval:", "自动同步间隔：",
                    m_weComSyncIntervalSpin);
    addLocalizedRow(weComForm, "Required idle ratio (strictly greater):",
                    "所需空闲占比（严格大于）：", m_weComIdleThresholdSpin);

    auto *priorityHint = new QLabel(weComGroup);
    priorityHint->setObjectName("fieldHint");
    priorityHint->setWordWrap(true);
    UiLanguage::bindText(
        priorityHint,
        "Low priority: coding, documents, browser and other detected activity always remain unchanged. "
        "Only qualifying idle fragments are relabeled as meeting time.",
        "低优先级：编码、文档、浏览器等已检测活动始终保持不变，只会将达到条件的空闲片段补记为会议。");
    weComForm->addRow(QString(), priorityHint);

    auto *setupGroup = new QGroupBox(integrationsTab);
    UiLanguage::bindText(setupGroup, "Connection status", "连接状态");
    auto *setupLayout = new QVBoxLayout(setupGroup);
    setupLayout->setSpacing(10);
    auto *installHint = new QLabel(setupGroup);
    installHint->setWordWrap(true);
    UiLanguage::bindText(
        installHint,
        "First-time setup requires Node.js 18+ and the commands below. Authorization is separate from the desktop client's login. Return here and recheck after scanning:",
        "首次使用需要 Node.js 18+ 并执行以下命令。该授权与企业微信桌面客户端登录相互独立；扫码完成后请回到这里重新检测：");
    auto *commands = new QLabel(
        QStringLiteral("npm install -g @wecom/cli\n"
                       "wecom-cli auth init\n"
                       "wecom-cli auth show"), setupGroup);
    commands->setObjectName("integrationCommand");
    commands->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_weComStatusLabel = new QLabel(setupGroup);
    m_weComStatusLabel->setObjectName("integrationStatus");
    m_weComStatusLabel->setWordWrap(true);
    m_weComAuthCheckButton = new QPushButton(setupGroup);
    m_weComAuthCheckButton->setObjectName("secondaryButton");
    m_weComAuthCheckButton->setIcon(
        style()->standardIcon(QStyle::SP_DialogApplyButton));
    UiLanguage::bindText(m_weComAuthCheckButton,
                         "Recheck authorization", "重新检测授权");
    m_weComSyncButton = new QPushButton(setupGroup);
    m_weComSyncButton->setObjectName("secondaryButton");
    m_weComSyncButton->setIcon(style()->standardIcon(QStyle::SP_BrowserReload));
    UiLanguage::bindText(m_weComSyncButton, "Sync meetings now", "立即同步会议");
    m_weComSyncStatusLabel = new QLabel(setupGroup);
    m_weComSyncStatusLabel->setObjectName("fieldHint");
    m_weComSyncStatusLabel->setWordWrap(true);
    UiLanguage::bindText(m_weComSyncStatusLabel,
                         "No manual sync has run in this session.",
                         "本次运行尚未手动同步。");
    auto *syncActions = new QHBoxLayout();
    syncActions->addWidget(m_weComAuthCheckButton);
    syncActions->addWidget(m_weComSyncButton);
    syncActions->addStretch();
    setupLayout->addWidget(installHint);
    setupLayout->addWidget(commands);
    setupLayout->addWidget(m_weComStatusLabel);
    setupLayout->addLayout(syncActions);
    setupLayout->addWidget(m_weComSyncStatusLabel);

    connect(m_weComMeetingChk, &QCheckBox::toggled,
            this, [this]() { updateWeComActionState(); });
    connect(m_weComAuthCheckButton, &QPushButton::clicked,
            this, &SettingsDialog::checkWeComAuthorization);
    connect(m_weComCliPathEdit, &QLineEdit::editingFinished,
            this, &SettingsDialog::checkWeComAuthorization);
    connect(m_weComSyncButton, &QPushButton::clicked, this, [this]() {
        if (!m_weComMeetingMonitor || !m_weComAuthorized) return;
        const QString path = m_weComCliPathEdit->text();
        const int interval = m_weComSyncIntervalSpin->value();
        const int threshold = m_weComIdleThresholdSpin->value();
        QMetaObject::invokeMethod(m_weComMeetingMonitor,
            [monitor = m_weComMeetingMonitor, path, interval, threshold]() {
                monitor->setCliPath(path);
                monitor->setSyncIntervalMinutes(interval);
                monitor->setIdleThresholdPercent(threshold);
                monitor->setEnabled(true);
                monitor->syncNow();
            }, Qt::QueuedConnection);
    });

    integrationsLayout->addWidget(integrationIntro);
    integrationsLayout->addWidget(weComGroup);
    integrationsLayout->addWidget(setupGroup);
    integrationsLayout->addStretch();

    // === Data Retention Tab ===
    auto *dataTab = new QWidget();
    auto *dataLayout = new QVBoxLayout(dataTab);
    dataLayout->setContentsMargins(20, 16, 20, 20);
    dataLayout->setSpacing(14);

    auto *dataIntro = new QLabel(dataTab);
    dataIntro->setObjectName("settingsHint");
    dataIntro->setWordWrap(true);
    UiLanguage::bindText(
        dataIntro,
        "DailyReport keeps data locally. Expired records are removed at startup "
        "and automatically once every 24 hours.",
        "DailyReport 将数据保存在本机。程序会在启动时清理，并每 24 小时自动清理一次过期记录。");
    dataLayout->addWidget(dataIntro);

    auto *activityGroup = new QGroupBox(dataTab);
    UiLanguage::bindText(activityGroup, "Activity timeline", "活动时间线");
    auto *activityLayout = new QFormLayout(activityGroup);
    m_activityRetentionSpin = new QSpinBox(activityGroup);
    m_activityRetentionSpin->setRange(1, 120);
    UiLanguage::bindSuffix(m_activityRetentionSpin, " months", " 个月");
    addLocalizedRow(activityLayout, "Keep activity data for:",
                    "活动数据保留：", m_activityRetentionSpin);
    auto *activityHint = new QLabel(activityGroup);
    activityHint->setObjectName("fieldHint");
    activityHint->setWordWrap(true);
    UiLanguage::bindText(
        activityHint,
        "Includes editor, browser, document, Git, build, process, and window events.",
        "包括编辑器、浏览器、文档、Git、构建、进程和窗口事件。");
    activityLayout->addRow(QString(), activityHint);

    auto *reportGroup = new QGroupBox(dataTab);
    UiLanguage::bindText(reportGroup, "Report history", "历史报告");
    auto *reportRetentionLayout = new QFormLayout(reportGroup);
    m_reportRetentionSpin = new QSpinBox(reportGroup);
    m_reportRetentionSpin->setRange(1, 120);
    UiLanguage::bindSuffix(m_reportRetentionSpin, " months", " 个月");
    addLocalizedRow(reportRetentionLayout, "Keep generated reports for:",
                    "生成的报告保留：", m_reportRetentionSpin);
    auto *reportHint = new QLabel(reportGroup);
    reportHint->setObjectName("fieldHint");
    reportHint->setWordWrap(true);
    UiLanguage::bindText(
        reportHint,
        "Exported Markdown, HTML, and AI packages are not deleted because they are outside the app database.",
        "已导出的 Markdown、HTML 和 AI 总结包不在应用数据库中，因此不会被删除。");
    reportRetentionLayout->addRow(QString(), reportHint);

    auto *cleanupGroup = new QGroupBox(dataTab);
    UiLanguage::bindText(cleanupGroup, "Cleanup status", "清理状态");
    auto *cleanupLayout = new QVBoxLayout(cleanupGroup);
    m_cleanupStatusLabel = new QLabel(cleanupGroup);
    m_cleanupStatusLabel->setObjectName("cleanupStatus");
    m_cleanupStatusLabel->setWordWrap(true);
    const QDateTime lastCleanup = QDateTime::fromString(
        m_settings->getValue("last_data_cleanup_at"), Qt::ISODateWithMs);
    if (lastCleanup.isValid()) {
        UiLanguage::bindText(
            m_cleanupStatusLabel,
            QString("Last automatic cleanup: %1")
                .arg(lastCleanup.toLocalTime().toString("yyyy-MM-dd HH:mm")),
            QString("上次自动清理：%1")
                .arg(lastCleanup.toLocalTime().toString("yyyy-MM-dd HH:mm")));
    } else {
        UiLanguage::bindText(m_cleanupStatusLabel,
                             "No cleanup has run yet.", "尚未执行过清理。");
    }
    auto *cleanupActions = new QHBoxLayout();
    auto *cleanupNowBtn = new QPushButton();
    cleanupNowBtn->setObjectName("dangerButton");
    UiLanguage::bindText(cleanupNowBtn,
                         "Clean expired data now", "立即清理过期数据");
    connect(cleanupNowBtn, &QPushButton::clicked, this, [this]() {
        const bool confirmed = DialogUtils::confirm(
            this,
            UiLanguage::text("Clean expired data", "清理过期数据"),
            UiLanguage::text(
                "Expired activity records and reports will be permanently deleted "
                "using the retention periods shown above. Continue?",
                "将按照上方保留时间永久删除过期的活动记录和历史报告。是否继续？"),
            UiLanguage::text("Clean expired data", "清理过期数据"), true);
        if (!confirmed) return;

        saveRetentionSettings();
        if (m_retentionService) {
            m_retentionService->reloadSettings();
            m_retentionService->runCleanupNow();
        }
    });
    cleanupActions->addWidget(cleanupNowBtn);
    cleanupActions->addStretch();
    cleanupLayout->addWidget(m_cleanupStatusLabel);
    cleanupLayout->addLayout(cleanupActions);

    dataLayout->addWidget(activityGroup);
    dataLayout->addWidget(reportGroup);
    dataLayout->addWidget(cleanupGroup);
    dataLayout->addStretch();

    // === LLM Tab ===
    auto *llmTab = new QWidget();
    auto *llmLayout = new QFormLayout(llmTab);
    llmLayout->setContentsMargins(20, 22, 20, 20);
    llmLayout->setHorizontalSpacing(16);
    llmLayout->setVerticalSpacing(13);

    m_backendCombo = new QComboBox();
    m_backendCombo->addItems({"", "OpenAI", "Anthropic", "DeepSeek", "Ollama"});
    m_endpointEdit = new QLineEdit();
    m_apiKeyEdit = new QLineEdit();
    m_apiKeyEdit->setEchoMode(QLineEdit::Password);
    m_showKeyChk = new QCheckBox();
    UiLanguage::bindText(m_showKeyChk, "Show API key", "显示 API 密钥");
    m_modelEdit = new QLineEdit();
    m_temperatureSpin = new QDoubleSpinBox();
    m_temperatureSpin->setRange(0.0, 2.0);
    m_temperatureSpin->setSingleStep(0.1);
    m_temperatureSpin->setDecimals(2);
    m_maxTokensSpin = new QSpinBox();
    m_maxTokensSpin->setRange(100, 32768);
    m_maxTokensSpin->setSingleStep(512);

    connect(m_backendCombo, &QComboBox::activated, this, [this](int) {
        const QString backend = m_backendCombo->currentText();
        if (backend == "DeepSeek") {
            m_endpointEdit->setText("https://api.deepseek.com/chat/completions");
            m_modelEdit->setText("deepseek-v4-flash");
        } else if (backend == "OpenAI") {
            m_endpointEdit->setText("https://api.openai.com/v1/chat/completions");
            m_modelEdit->setText("gpt-4o");
        } else if (backend == "Anthropic") {
            m_endpointEdit->setText("https://api.anthropic.com/v1/messages");
            m_modelEdit->setText("claude-sonnet-4-20250514");
        } else if (backend == "Ollama") {
            m_endpointEdit->setText("http://localhost:11434/api/generate");
            m_modelEdit->setText("llama3");
        }
    });

    connect(m_showKeyChk, &QCheckBox::toggled, this, [this](bool checked) {
        m_apiKeyEdit->setEchoMode(checked ? QLineEdit::Normal : QLineEdit::Password);
    });

    addLocalizedRow(llmLayout, "AI provider:", "AI 服务：", m_backendCombo);
    addLocalizedRow(llmLayout, "API endpoint:", "API 地址：", m_endpointEdit);
    addLocalizedRow(llmLayout, "API key:", "API 密钥：", m_apiKeyEdit);
    llmLayout->addRow("", m_showKeyChk);
    addLocalizedRow(llmLayout, "Model:", "模型：", m_modelEdit);
    addLocalizedRow(llmLayout, "Temperature:", "随机性：", m_temperatureSpin);
    addLocalizedRow(llmLayout, "Maximum tokens:", "最大 Token 数：", m_maxTokensSpin);

    // === Report Tab ===
    auto *reportTab = new QWidget();
    auto *reportLayout = new QFormLayout(reportTab);
    reportLayout->setContentsMargins(20, 22, 20, 20);
    reportLayout->setHorizontalSpacing(16);
    reportLayout->setVerticalSpacing(13);

    m_dailyTimeEdit = new QTimeEdit(QTime(17, 30));
    m_weeklyDayCombo = new QComboBox();
    const QStringList days = {"Monday", "Tuesday", "Wednesday", "Thursday",
                              "Friday", "Saturday", "Sunday"};
    const QStringList daysZh = {"星期一", "星期二", "星期三", "星期四",
                                "星期五", "星期六", "星期日"};
    for (int i = 0; i < days.size(); ++i) {
        m_weeklyDayCombo->addItem("", i + 1);
        UiLanguage::bindComboItem(m_weeklyDayCombo, i, days.at(i), daysZh.at(i));
    }
    m_weeklyTimeEdit = new QTimeEdit(QTime(17, 0));
    m_languageCombo = new QComboBox();
    m_languageCombo->addItem("", "en");
    m_languageCombo->addItem("", "zh-CN");
    UiLanguage::bindComboItem(m_languageCombo, 0, "English", "English");
    UiLanguage::bindComboItem(m_languageCombo, 1, "Simplified Chinese", "简体中文");

    addLocalizedRow(reportLayout, "Daily report time:", "日报生成时间：", m_dailyTimeEdit);
    addLocalizedRow(reportLayout, "Weekly report day:", "周报生成日期：", m_weeklyDayCombo);
    addLocalizedRow(reportLayout, "Weekly report time:", "周报生成时间：", m_weeklyTimeEdit);
    addLocalizedRow(reportLayout, "Interface and report language:",
                    "界面与报告语言：", m_languageCombo);

    // === Template Edit Tab ===
    auto *templateTab = new QWidget();
    auto *templateLayout = new QVBoxLayout(templateTab);
    templateLayout->setContentsMargins(20, 18, 20, 20);
    templateLayout->setSpacing(12);
    m_templateCombo = new QComboBox();
    m_templateCombo->addItems({"daily_report", "weekly_report"});
    m_templateEdit = new QTextEdit();
    m_templateEdit->setFont(QFont("Consolas", 10));
    UiLanguage::bindPlaceholder(m_templateEdit,
                                "Select a template to edit...", "选择要编辑的报告模板……");
    connect(m_templateCombo, &QComboBox::currentTextChanged, this,
            [this](const QString &name) {
                if (m_templateEngine)
                    m_templateEdit->setPlainText(m_templateEngine->templateContent(name));
            });

    auto *templateLabel = new QLabel();
    UiLanguage::bindText(templateLabel, "Edit report template:", "编辑报告模板：");
    templateLayout->addWidget(templateLabel);
    templateLayout->addWidget(m_templateCombo);
    templateLayout->addWidget(m_templateEdit);
    // Assemble tabs
    auto *generalPage = makeSettingsPage(generalTab);
    auto *monitorPage = makeSettingsPage(monitorTab);
    auto *integrationsPage = makeSettingsPage(integrationsTab);
    auto *dataPage = makeSettingsPage(dataTab);
    auto *llmPage = makeSettingsPage(llmTab);
    auto *reportPage = makeSettingsPage(reportTab);
    auto *templatePage = makeSettingsPage(templateTab);
    tabWidget->addTab(generalPage, "");
    tabWidget->addTab(monitorPage, "");
    tabWidget->addTab(integrationsPage, "");
    tabWidget->addTab(dataPage, "");
    tabWidget->addTab(llmPage, "");
    tabWidget->addTab(reportPage, "");
    tabWidget->addTab(templatePage, "");
    UiLanguage::bindTab(tabWidget, generalPage, "General", "常规");
    UiLanguage::bindTab(tabWidget, monitorPage, "Monitoring", "监控");
    UiLanguage::bindTab(tabWidget, integrationsPage, "Integrations", "集成");
    UiLanguage::bindTab(tabWidget, dataPage, "Data & privacy", "数据与隐私");
    UiLanguage::bindTab(tabWidget, llmPage, "AI provider", "AI 服务");
    UiLanguage::bindTab(tabWidget, reportPage, "Schedule & language", "计划与语言");
    UiLanguage::bindTab(tabWidget, templatePage, "Templates", "模板");

    auto *footer = new QFrame(this);
    footer->setObjectName("settingsFooter");
    auto *footerLayout = new QHBoxLayout(footer);
    footerLayout->setContentsMargins(12, 9, 10, 9);
    footerLayout->setSpacing(9);
    m_saveStatusLabel = new QLabel(footer);
    m_saveStatusLabel->setObjectName("settingsSaveStatus");
    footerLayout->addWidget(m_saveStatusLabel, 1);

    auto *reloadBtn = new QPushButton(footer);
    reloadBtn->setIcon(style()->standardIcon(QStyle::SP_BrowserReload));
    UiLanguage::bindText(reloadBtn, "Reload saved values", "重新载入已保存配置");
    connect(reloadBtn, &QPushButton::clicked, this, [this]() {
        loadSettings();
        m_saveStatusLabel->setText(UiLanguage::text(
            "Saved values restored.", "已恢复为保存的配置。"));
    });
    footerLayout->addWidget(reloadBtn);

    auto *saveAllBtn = new QPushButton(footer);
    saveAllBtn->setObjectName("primaryButton");
    saveAllBtn->setIcon(style()->standardIcon(QStyle::SP_DialogSaveButton));
    UiLanguage::bindText(saveAllBtn, "Save all changes", "保存全部修改");
    connect(saveAllBtn, &QPushButton::clicked,
            this, &SettingsDialog::saveSettings);
    footerLayout->addWidget(saveAllBtn);

    mainLayout->addWidget(tabWidget, 1);
    mainLayout->addWidget(footer);
}

void SettingsDialog::setWeComStatus(const char *state,
                                    const QString &english,
                                    const QString &chinese)
{
    if (!m_weComStatusLabel) return;
    m_weComStatusLabel->setProperty("state", state);
    UiLanguage::bindText(m_weComStatusLabel, english, chinese);
    style()->unpolish(m_weComStatusLabel);
    style()->polish(m_weComStatusLabel);
}

void SettingsDialog::updateWeComActionState()
{
    if (!m_weComMeetingChk) return;
    const bool enabled = m_weComMeetingChk->isChecked();
    const bool authProcessIdle = !m_weComAuthProcess
        || m_weComAuthProcess->state() == QProcess::NotRunning;

    // The path and authorization check remain available before the integration
    // is enabled, so setup can be completed in a predictable order.
    m_weComCliPathEdit->setEnabled(!m_weComAuthChecking);
    m_weComSyncIntervalSpin->setEnabled(enabled);
    m_weComIdleThresholdSpin->setEnabled(enabled);
    m_weComAuthCheckButton->setEnabled(
        !m_weComAuthChecking && !m_weComSyncing && authProcessIdle);
    m_weComSyncButton->setEnabled(
        enabled && m_weComMeetingMonitor && m_weComAuthorized
        && !m_weComAuthChecking && !m_weComSyncing);
}

void SettingsDialog::checkWeComAuthorization()
{
    if (!m_weComAuthProcess || m_weComAuthChecking
        || m_weComAuthProcess->state() != QProcess::NotRunning) {
        return;
    }

    m_weComAuthorized = false;
    const QString resolvedCli = WeComMeetingMonitor::resolveCliPath(
        m_weComCliPathEdit->text());
    if (resolvedCli.isEmpty()) {
        setWeComStatus(
            "error",
            "Not configured · wecom-cli is not installed or the configured path is invalid. Install it, then recheck.",
            "尚未配置 · 未安装 wecom-cli，或填写的路径无效。请完成安装后重新检测。");
        updateWeComActionState();
        return;
    }

    m_weComAuthChecking = true;
    setWeComStatus(
        "checking",
        "Checking authorization… This normally takes only a few seconds.",
        "正在检测授权状态……通常只需要几秒钟。");
    updateWeComActionState();

    QString program = resolvedCli;
    QStringList arguments{QStringLiteral("auth"), QStringLiteral("show")};
#ifdef Q_OS_WIN
    if (program.endsWith(".cmd", Qt::CaseInsensitive)
        || program.endsWith(".bat", Qt::CaseInsensitive)) {
        arguments.prepend(program);
        arguments.prepend(QStringLiteral("/c"));
        arguments.prepend(QStringLiteral("/d"));
        program = qEnvironmentVariable("COMSPEC", QStringLiteral("cmd.exe"));
    }
#endif

    m_weComAuthProcess->setProgram(program);
    m_weComAuthProcess->setArguments(arguments);
    m_weComAuthProcess->start();
    m_weComAuthTimeout->start(15000);
}

void SettingsDialog::finishWeComAuthorizationCheck(
    int exitCode, QProcess::ExitStatus exitStatus)
{
    if (!m_weComAuthChecking) {
        updateWeComActionState();
        return;
    }
    m_weComAuthChecking = false;
    m_weComAuthTimeout->stop();

    const QByteArray output = m_weComAuthProcess->readAllStandardOutput();
    QString standardError = QString::fromUtf8(
        m_weComAuthProcess->readAllStandardError()).trimmed();
    standardError.replace(QRegularExpression(QStringLiteral("[\\r\\n]+")),
                          QStringLiteral(" "));
    if (standardError.size() > 320)
        standardError = standardError.left(317) + QStringLiteral("…");

    if (exitStatus != QProcess::NormalExit || exitCode != 0) {
        m_weComAuthorized = false;
        const QString detail = standardError.isEmpty()
            ? QString("wecom-cli exited with code %1").arg(exitCode)
            : standardError;
        setWeComStatus(
            "error",
            QString("Authorization check failed · %1").arg(detail),
            QString("授权检测失败 · %1").arg(detail));
        updateWeComActionState();
        return;
    }

    bool authorized = false;
    QString botId;
    QString parseError;
    if (!WeComMeetingMonitor::parseAuthorizationResponse(
            output, &authorized, &botId, &parseError)) {
        m_weComAuthorized = false;
        setWeComStatus(
            "error",
            QString("Authorization status could not be recognized · %1")
                .arg(parseError),
            QString("无法识别授权状态 · %1").arg(parseError));
    } else if (!authorized) {
        m_weComAuthorized = false;
        setWeComStatus(
            "warning",
            "Not authorized · run 'wecom-cli auth init', finish scanning, then click Recheck authorization.",
            "尚未授权 · 请运行“wecom-cli auth init”并完成扫码，然后点击“重新检测授权”。");
    } else {
        m_weComAuthorized = true;
        setWeComStatus(
            "success",
            botId.isEmpty()
                ? QStringLiteral("Authorized · credentials are ready for meeting sync.")
                : QString("Authorized · Bot ID: %1").arg(botId),
            botId.isEmpty()
                ? QStringLiteral("授权成功 · 已可同步企业微信会议。")
                : QString("授权成功 · Bot ID：%1").arg(botId));
    }
    updateWeComActionState();
}

void SettingsDialog::loadSettings()
{
    m_autoStartChk->setChecked(m_settings->getBool("auto_start", true));
    m_startMinimizedChk->setChecked(m_settings->getBool("start_minimized", true));
    m_afkThresholdSpin->setValue(m_settings->getInt("afk_threshold_secs", 300));
    const int legacyMonths = qMax(
        1, (m_settings->getInt("data_retention_days", 90) + 29) / 30);
    m_activityRetentionSpin->setValue(
        m_settings->getInt("activity_retention_months", legacyMonths));
    m_reportRetentionSpin->setValue(
        m_settings->getInt("report_retention_months", 3));

    m_gitTrackingChk->setChecked(m_settings->getBool("git_tracking_enabled", true));
    m_browserTrackingChk->setChecked(m_settings->getBool("browser_tracking_enabled", true));
    m_browserFullUrlChk->setChecked(
        m_settings->getBool("browser_capture_full_url", false));
    m_distractionTrackingChk->setChecked(
        m_settings->getBool("distraction_tracking_enabled", false));
    m_browserFullUrlChk->setEnabled(m_browserTrackingChk->isChecked());
    m_distractionTrackingChk->setEnabled(m_browserTrackingChk->isChecked());
    m_buildTrackingChk->setChecked(m_settings->getBool("build_tracking_enabled", true));
    m_editorTrackingChk->setChecked(m_settings->getBool("editor_tracking_enabled", true));
    m_documentTrackingChk->setChecked(m_settings->getBool("document_tracking_enabled", true));

    m_weComMeetingChk->setChecked(
        m_settings->getBool("wecom_meeting_enabled", false));
    m_weComCliPathEdit->setText(
        m_settings->getValue("wecom_cli_path", "wecom-cli"));
    m_weComSyncIntervalSpin->setValue(
        m_settings->getInt("wecom_meeting_sync_minutes", 30));
    m_weComIdleThresholdSpin->setValue(
        m_settings->getInt("wecom_meeting_idle_threshold_percent", 30));
    const QString resolvedCli = WeComMeetingMonitor::resolveCliPath(
        m_weComCliPathEdit->text());
    m_weComAuthorized = false;
    if (resolvedCli.isEmpty()) {
        setWeComStatus(
            "error",
            "Not configured · wecom-cli is not installed or cannot be found.",
            "尚未配置 · 未安装 wecom-cli，或无法找到该程序。");
    } else {
        setWeComStatus(
            "checking",
            QString("CLI found · waiting to verify authorization: %1")
                .arg(QDir::toNativeSeparators(resolvedCli)),
            QString("已找到 CLI · 等待验证授权：%1")
                .arg(QDir::toNativeSeparators(resolvedCli)));
    }
    updateWeComActionState();
    QTimer::singleShot(0, this, &SettingsDialog::checkWeComAuthorization);

    m_projectPathsList->clear();
    const QJsonDocument pathsDoc = QJsonDocument::fromJson(
        m_settings->getValue("monitored_paths", "[]").toUtf8());
    for (const auto &path : pathsDoc.array()) {
        if (!path.toString().isEmpty())
            m_projectPathsList->addItem(path.toString());
    }

    m_backendCombo->setCurrentText(m_settings->getValue("llm_backend", ""));
    QJsonObject llmCfg = m_settings->getJson("llm_config");
    if (!llmCfg.isEmpty()) {
        m_endpointEdit->setText(llmCfg["endpoint"].toString());
        m_modelEdit->setText(llmCfg["model"].toString());
        m_temperatureSpin->setValue(llmCfg["temperature"].toDouble(0.7));
        m_maxTokensSpin->setValue(llmCfg["maxTokens"].toInt(4096));
    }
    const QByteArray encryptedKey = m_settings->getValue("llm_api_key_encrypted").toUtf8();
    if (!encryptedKey.isEmpty())
        m_apiKeyEdit->setText(QString::fromUtf8(CryptoUtils::decrypt(encryptedKey)));

    m_dailyTimeEdit->setTime(QTime::fromString(
        m_settings->getValue("daily_report_time", "17:30"), "HH:mm"));
    const int weeklyDay = qBound(1, m_settings->getInt("weekly_report_day", 5), 7);
    m_weeklyDayCombo->setCurrentIndex(weeklyDay - 1);
    m_weeklyTimeEdit->setTime(QTime::fromString(
        m_settings->getValue("weekly_report_time", "17:00"), "HH:mm"));
    const QString language = m_settings->getValue("language", "zh-CN");
    const int languageIndex = m_languageCombo->findData(
        language.startsWith("zh", Qt::CaseInsensitive) ? "zh-CN" : "en");
    m_languageCombo->setCurrentIndex(qMax(0, languageIndex));

    if (m_templateEngine)
        m_templateEdit->setPlainText(
            m_templateEngine->templateContent(m_templateCombo->currentText()));
}

void SettingsDialog::saveSettings()
{
    m_settings->setBool("auto_start", m_autoStartChk->isChecked());
    if (!WinUtils::setAutoStart(m_autoStartChk->isChecked(),
                                "DailyReport", WinUtils::applicationFilePath())) {
        spdlog::warn("Failed to update the automatic-start configuration.");
    }
    m_settings->setBool("start_minimized", m_startMinimizedChk->isChecked());
    m_settings->setInt("afk_threshold_secs", m_afkThresholdSpin->value());
    saveRetentionSettings();

    m_settings->setBool("git_tracking_enabled", m_gitTrackingChk->isChecked());
    m_settings->setBool("browser_tracking_enabled", m_browserTrackingChk->isChecked());
    m_settings->setBool("browser_capture_full_url", m_browserFullUrlChk->isChecked());
    m_settings->setBool("distraction_tracking_enabled",
                        m_distractionTrackingChk->isChecked());
    m_settings->setBool("build_tracking_enabled", m_buildTrackingChk->isChecked());
    m_settings->setBool("editor_tracking_enabled", m_editorTrackingChk->isChecked());
    m_settings->setBool("document_tracking_enabled", m_documentTrackingChk->isChecked());
    m_settings->setBool("wecom_meeting_enabled", m_weComMeetingChk->isChecked());
    m_settings->setValue("wecom_cli_path", m_weComCliPathEdit->text().trimmed());
    m_settings->setInt("wecom_meeting_sync_minutes",
                       m_weComSyncIntervalSpin->value());
    m_settings->setInt("wecom_meeting_idle_threshold_percent",
                       m_weComIdleThresholdSpin->value());

    if (m_weComMeetingMonitor) {
        const bool enabled = m_weComMeetingChk->isChecked();
        const QString path = m_weComCliPathEdit->text().trimmed();
        const int interval = m_weComSyncIntervalSpin->value();
        const int threshold = m_weComIdleThresholdSpin->value();
        QMetaObject::invokeMethod(m_weComMeetingMonitor,
            [monitor = m_weComMeetingMonitor, enabled, path, interval, threshold]() {
                monitor->setCliPath(path);
                monitor->setSyncIntervalMinutes(interval);
                monitor->setIdleThresholdPercent(threshold);
                monitor->setEnabled(enabled);
            }, Qt::QueuedConnection);
    }

    m_settings->setValue("llm_backend", m_backendCombo->currentText());

    QJsonObject llmCfg;
    llmCfg["endpoint"] = m_endpointEdit->text();
    llmCfg["model"] = m_modelEdit->text();
    llmCfg["temperature"] = m_temperatureSpin->value();
    llmCfg["maxTokens"] = m_maxTokensSpin->value();
    m_settings->setJson("llm_config", llmCfg);

    // API key is stored encrypted separately
    const QByteArray encrypted = CryptoUtils::encrypt(m_apiKeyEdit->text().toUtf8());
    m_settings->setValue("llm_api_key_encrypted", QString::fromUtf8(encrypted));

    m_settings->setValue("daily_report_time", m_dailyTimeEdit->time().toString("HH:mm"));
    m_settings->setInt("weekly_report_day", m_weeklyDayCombo->currentData().toInt());
    m_settings->setValue("weekly_report_time", m_weeklyTimeEdit->time().toString("HH:mm"));
    const QString language = m_languageCombo->currentData().toString();
    m_settings->setValue("language", language);

    // Save project paths as JSON array
    QJsonArray paths;
    for (int i = 0; i < m_projectPathsList->count(); ++i) {
        paths.append(m_projectPathsList->item(i)->text());
    }
    QJsonDocument doc;
    doc.setArray(paths);
    m_settings->setValue("monitored_paths",
                         QString::fromUtf8(doc.toJson(QJsonDocument::Compact)));

    if (m_templateEngine && !m_templateEdit->toPlainText().trimmed().isEmpty()) {
        const QString name = m_templateCombo->currentText();
        m_templateEngine->registerTemplate(
            name, m_templateEdit->toPlainText(),
            m_templateEngine->templateDescription(name));
        m_templateEngine->saveToDatabase();
    }

    spdlog::info("Settings saved.");
    UiLanguage::setLanguage(language);
    UiLanguage::applyAll();
    emit settingsSaved();
    m_saveStatusLabel->setText(UiLanguage::text(
        "All changes saved.", "全部修改已保存。"));
    QTimer::singleShot(4000, this, [this]() {
        m_saveStatusLabel->clear();
    });
}

void SettingsDialog::saveRetentionSettings()
{
    const int activityMonths = m_activityRetentionSpin->value();
    m_settings->setInt("activity_retention_months", activityMonths);
    m_settings->setInt("report_retention_months", m_reportRetentionSpin->value());

    // Retain the old setting for compatibility with older builds that may
    // still open this database.
    m_settings->setInt("data_retention_days", activityMonths * 30);
}
