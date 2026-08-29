#include "SettingsDialog.h"
#include "UiLanguage.h"
#include "storage/SettingsRepository.h"
#include "report/TemplateEngine.h"
#include "app/DataRetentionService.h"
#include "util/CryptoUtils.h"
#include "util/WinUtils.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QPushButton>
#include <QFileDialog>
#include <QLabel>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMessageBox>
#include <QDateTime>
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

} // namespace

SettingsDialog::SettingsDialog(SettingsRepository *settings,
                               TemplateEngine *templateEngine,
                               DataRetentionService *retentionService,
                               QWidget *parent)
    : QWidget(parent)
    , m_settings(settings)
    , m_templateEngine(templateEngine)
    , m_retentionService(retentionService)
{
    setupUi();
    loadSettings();

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
                    QMessageBox::warning(
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
                QMessageBox::information(
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
    mainLayout->setContentsMargins(20, 18, 20, 20);
    auto *tabWidget = new QTabWidget(this);
    tabWidget->setObjectName("settingsTabs");
    tabWidget->setDocumentMode(true);
    tabWidget->setUsesScrollButtons(true);

    // === General Tab ===
    auto *generalTab = new QWidget();
    auto *generalLayout = new QVBoxLayout(generalTab);

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

    auto *saveBtn = new QPushButton();
    saveBtn->setObjectName("primaryButton");
    UiLanguage::bindText(saveBtn, "Save settings", "保存设置");
    connect(saveBtn, &QPushButton::clicked, this, &SettingsDialog::saveSettings);

    generalLayout->addWidget(startupGroup);
    generalLayout->addStretch();
    generalLayout->addWidget(saveBtn);

    // === Monitoring Tab ===
    auto *monitorTab = new QWidget();
    auto *monitorLayout = new QVBoxLayout(monitorTab);

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
        QString dir = QFileDialog::getExistingDirectory(
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
    UiLanguage::bindText(m_buildTrackingChk,
                         "Enable build/compile tracking", "启用构建与编译监控");
    UiLanguage::bindText(m_editorTrackingChk,
                         "Track editor file and project context", "记录编辑器文件与项目上下文");
    UiLanguage::bindText(m_documentTrackingChk,
                         "Track opened PDF and Office document names", "记录打开的 PDF 与 Office 文档名称");
    featuresForm->addRow(m_gitTrackingChk);
    featuresForm->addRow(m_browserTrackingChk);
    featuresForm->addRow(m_browserFullUrlChk);
    featuresForm->addRow(m_buildTrackingChk);
    featuresForm->addRow(m_editorTrackingChk);
    featuresForm->addRow(m_documentTrackingChk);

    monitorLayout->addWidget(pathsGroup);
    monitorLayout->addWidget(featuresGroup);
    monitorLayout->addStretch();
    auto *monitorSaveBtn = new QPushButton();
    monitorSaveBtn->setObjectName("primaryButton");
    UiLanguage::bindText(monitorSaveBtn, "Save monitoring settings", "保存监控设置");
    connect(monitorSaveBtn, &QPushButton::clicked,
            this, &SettingsDialog::saveSettings);
    monitorLayout->addWidget(monitorSaveBtn);

    // === Data Retention Tab ===
    auto *dataTab = new QWidget();
    auto *dataLayout = new QVBoxLayout(dataTab);

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
    auto *retentionSaveBtn = new QPushButton();
    retentionSaveBtn->setObjectName("primaryButton");
    UiLanguage::bindText(retentionSaveBtn,
                         "Save retention settings", "保存保留时间");
    connect(retentionSaveBtn, &QPushButton::clicked,
            this, &SettingsDialog::saveSettings);
    auto *cleanupNowBtn = new QPushButton();
    cleanupNowBtn->setObjectName("dangerButton");
    UiLanguage::bindText(cleanupNowBtn,
                         "Clean expired data now", "立即清理过期数据");
    connect(cleanupNowBtn, &QPushButton::clicked, this, [this]() {
        const auto answer = QMessageBox::question(
            this,
            UiLanguage::text("Clean expired data", "清理过期数据"),
            UiLanguage::text(
                "Expired activity records and reports will be permanently deleted "
                "using the retention periods shown above. Continue?",
                "将按照上方保留时间永久删除过期的活动记录和历史报告。是否继续？"));
        if (answer != QMessageBox::Yes) return;

        saveRetentionSettings();
        if (m_retentionService) {
            m_retentionService->reloadSettings();
            m_retentionService->runCleanupNow();
        }
    });
    cleanupActions->addWidget(retentionSaveBtn);
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

    auto *llmSaveBtn = new QPushButton();
    llmSaveBtn->setObjectName("primaryButton");
    UiLanguage::bindText(llmSaveBtn, "Save AI settings", "保存 AI 设置");
    connect(llmSaveBtn, &QPushButton::clicked, this, &SettingsDialog::saveSettings);
    llmLayout->addRow(llmSaveBtn);

    // === Report Tab ===
    auto *reportTab = new QWidget();
    auto *reportLayout = new QFormLayout(reportTab);

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

    auto *reportSaveBtn = new QPushButton();
    reportSaveBtn->setObjectName("primaryButton");
    UiLanguage::bindText(reportSaveBtn, "Save report settings", "保存报告设置");
    connect(reportSaveBtn, &QPushButton::clicked, this, &SettingsDialog::saveSettings);
    reportLayout->addRow(reportSaveBtn);

    // === Template Edit Tab ===
    auto *templateTab = new QWidget();
    auto *templateLayout = new QVBoxLayout(templateTab);
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
    auto *templateSaveBtn = new QPushButton();
    templateSaveBtn->setObjectName("primaryButton");
    UiLanguage::bindText(templateSaveBtn, "Save template", "保存模板");
    connect(templateSaveBtn, &QPushButton::clicked, this, &SettingsDialog::saveSettings);
    templateLayout->addWidget(templateSaveBtn);

    // Assemble tabs
    tabWidget->addTab(generalTab, "");
    tabWidget->addTab(monitorTab, "");
    tabWidget->addTab(dataTab, "");
    tabWidget->addTab(llmTab, "");
    tabWidget->addTab(reportTab, "");
    tabWidget->addTab(templateTab, "");
    UiLanguage::bindTab(tabWidget, generalTab, "General", "常规");
    UiLanguage::bindTab(tabWidget, monitorTab, "Monitoring", "监控");
    UiLanguage::bindTab(tabWidget, dataTab, "Data & privacy", "数据与隐私");
    UiLanguage::bindTab(tabWidget, llmTab, "AI provider", "AI 服务");
    UiLanguage::bindTab(tabWidget, reportTab, "Schedule & language", "计划与语言");
    UiLanguage::bindTab(tabWidget, templateTab, "Templates", "模板");

    mainLayout->addWidget(tabWidget);
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
    m_buildTrackingChk->setChecked(m_settings->getBool("build_tracking_enabled", true));
    m_editorTrackingChk->setChecked(m_settings->getBool("editor_tracking_enabled", true));
    m_documentTrackingChk->setChecked(m_settings->getBool("document_tracking_enabled", true));

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
    m_settings->setBool("build_tracking_enabled", m_buildTrackingChk->isChecked());
    m_settings->setBool("editor_tracking_enabled", m_editorTrackingChk->isChecked());
    m_settings->setBool("document_tracking_enabled", m_documentTrackingChk->isChecked());

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
    QMessageBox::information(
        this,
        UiLanguage::text("Settings", "设置"),
        UiLanguage::text("Settings saved successfully.", "设置已保存。"));
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
