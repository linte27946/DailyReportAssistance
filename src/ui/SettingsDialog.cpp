#include "SettingsDialog.h"
#include "UiLanguage.h"
#include "storage/SettingsRepository.h"
#include "report/TemplateEngine.h"
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
                               QWidget *parent)
    : QWidget(parent)
    , m_settings(settings)
    , m_templateEngine(templateEngine)
{
    setupUi();
    loadSettings();
}

void SettingsDialog::setupUi()
{
    auto *mainLayout = new QVBoxLayout(this);
    auto *tabWidget = new QTabWidget(this);

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

    auto *dataGroup = new QGroupBox(generalTab);
    UiLanguage::bindText(dataGroup, "Data", "数据");
    auto *dataForm = new QFormLayout(dataGroup);
    m_dataRetentionSpin = new QSpinBox();
    m_dataRetentionSpin->setRange(7, 365);
    UiLanguage::bindSuffix(m_dataRetentionSpin, " days", " 天");
    addLocalizedRow(dataForm, "Data retention:", "数据保留时间：", m_dataRetentionSpin);

    auto *saveBtn = new QPushButton();
    UiLanguage::bindText(saveBtn, "Save settings", "保存设置");
    connect(saveBtn, &QPushButton::clicked, this, &SettingsDialog::saveSettings);

    generalLayout->addWidget(startupGroup);
    generalLayout->addWidget(dataGroup);
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
    UiLanguage::bindText(templateSaveBtn, "Save template", "保存模板");
    connect(templateSaveBtn, &QPushButton::clicked, this, &SettingsDialog::saveSettings);
    templateLayout->addWidget(templateSaveBtn);

    // Assemble tabs
    tabWidget->addTab(generalTab, "");
    tabWidget->addTab(monitorTab, "");
    tabWidget->addTab(llmTab, "");
    tabWidget->addTab(reportTab, "");
    tabWidget->addTab(templateTab, "");
    UiLanguage::bindTab(tabWidget, generalTab, "General", "常规");
    UiLanguage::bindTab(tabWidget, monitorTab, "Monitoring", "监控");
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
    m_dataRetentionSpin->setValue(m_settings->getInt("data_retention_days", 90));

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
    m_settings->setInt("data_retention_days", m_dataRetentionSpin->value());

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
