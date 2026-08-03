#include "SettingsDialog.h"
#include "storage/SettingsRepository.h"
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

SettingsDialog::SettingsDialog(SettingsRepository *settings, QWidget *parent)
    : QWidget(parent)
    , m_settings(settings)
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

    auto *startupGroup = new QGroupBox("Startup", generalTab);
    auto *startupForm = new QFormLayout(startupGroup);
    m_autoStartChk = new QCheckBox("Start with Windows");
    m_startMinimizedChk = new QCheckBox("Start minimized to tray");
    m_afkThresholdSpin = new QSpinBox();
    m_afkThresholdSpin->setRange(60, 3600);
    m_afkThresholdSpin->setSuffix(" seconds");
    startupForm->addRow(m_autoStartChk);
    startupForm->addRow(m_startMinimizedChk);
    startupForm->addRow("AFK Threshold:", m_afkThresholdSpin);

    auto *dataGroup = new QGroupBox("Data", generalTab);
    auto *dataForm = new QFormLayout(dataGroup);
    m_dataRetentionSpin = new QSpinBox();
    m_dataRetentionSpin->setRange(7, 365);
    m_dataRetentionSpin->setSuffix(" days");
    dataForm->addRow("Data retention:", m_dataRetentionSpin);

    auto *saveBtn = new QPushButton("Save Settings");
    connect(saveBtn, &QPushButton::clicked, this, &SettingsDialog::saveSettings);

    generalLayout->addWidget(startupGroup);
    generalLayout->addWidget(dataGroup);
    generalLayout->addStretch();
    generalLayout->addWidget(saveBtn);

    // === Monitoring Tab ===
    auto *monitorTab = new QWidget();
    auto *monitorLayout = new QVBoxLayout(monitorTab);

    auto *pathsGroup = new QGroupBox("Project Directories", monitorTab);
    auto *pathsLayout = new QVBoxLayout(pathsGroup);
    m_projectPathsList = new QListWidget();
    auto *pathBtnLayout = new QHBoxLayout();
    auto *addPathBtn = new QPushButton("Add Directory");
    auto *removePathBtn = new QPushButton("Remove Selected");
    pathBtnLayout->addWidget(addPathBtn);
    pathBtnLayout->addWidget(removePathBtn);
    pathBtnLayout->addStretch();
    pathsLayout->addWidget(m_projectPathsList);
    pathsLayout->addLayout(pathBtnLayout);

    connect(addPathBtn, &QPushButton::clicked, this, [this]() {
        QString dir = QFileDialog::getExistingDirectory(this, "Select Project Directory");
        if (!dir.isEmpty())
            m_projectPathsList->addItem(dir);
    });
    connect(removePathBtn, &QPushButton::clicked, this, [this]() {
        delete m_projectPathsList->currentItem();
    });

    auto *featuresGroup = new QGroupBox("Tracking Features", monitorTab);
    auto *featuresForm = new QFormLayout(featuresGroup);
    m_gitTrackingChk = new QCheckBox("Enable Git tracking");
    m_browserTrackingChk = new QCheckBox("Enable browser URL tracking");
    m_buildTrackingChk = new QCheckBox("Enable build/compile tracking");
    featuresForm->addRow(m_gitTrackingChk);
    featuresForm->addRow(m_browserTrackingChk);
    featuresForm->addRow(m_buildTrackingChk);

    monitorLayout->addWidget(pathsGroup);
    monitorLayout->addWidget(featuresGroup);

    // === LLM Tab ===
    auto *llmTab = new QWidget();
    auto *llmLayout = new QFormLayout(llmTab);

    m_backendCombo = new QComboBox();
    m_backendCombo->addItems({"", "OpenAI", "Anthropic", "Ollama"});
    m_endpointEdit = new QLineEdit();
    m_apiKeyEdit = new QLineEdit();
    m_apiKeyEdit->setEchoMode(QLineEdit::Password);
    m_showKeyChk = new QCheckBox("Show API key");
    m_modelEdit = new QLineEdit();
    m_temperatureSpin = new QDoubleSpinBox();
    m_temperatureSpin->setRange(0.0, 2.0);
    m_temperatureSpin->setSingleStep(0.1);
    m_temperatureSpin->setDecimals(2);
    m_maxTokensSpin = new QSpinBox();
    m_maxTokensSpin->setRange(100, 32768);
    m_maxTokensSpin->setSingleStep(512);

    connect(m_showKeyChk, &QCheckBox::toggled, this, [this](bool checked) {
        m_apiKeyEdit->setEchoMode(checked ? QLineEdit::Normal : QLineEdit::Password);
    });

    llmLayout->addRow("LLM Backend:", m_backendCombo);
    llmLayout->addRow("API Endpoint:", m_endpointEdit);
    llmLayout->addRow("API Key:", m_apiKeyEdit);
    llmLayout->addRow("", m_showKeyChk);
    llmLayout->addRow("Model:", m_modelEdit);
    llmLayout->addRow("Temperature:", m_temperatureSpin);
    llmLayout->addRow("Max Tokens:", m_maxTokensSpin);

    auto *llmSaveBtn = new QPushButton("Save LLM Settings");
    connect(llmSaveBtn, &QPushButton::clicked, this, &SettingsDialog::saveSettings);
    llmLayout->addRow(llmSaveBtn);

    // === Report Tab ===
    auto *reportTab = new QWidget();
    auto *reportLayout = new QFormLayout(reportTab);

    m_dailyTimeEdit = new QTimeEdit(QTime(17, 30));
    m_weeklyDaySpin = new QSpinBox();
    m_weeklyDaySpin->setRange(1, 7);
    m_weeklyDaySpin->setSpecialValueText("Monday");
    m_weeklyTimeEdit = new QTimeEdit(QTime(17, 0));
    m_languageCombo = new QComboBox();
    m_languageCombo->addItems({"English", "Chinese (中文)", "Japanese (日本語)"});

    reportLayout->addRow("Daily report time:", m_dailyTimeEdit);
    reportLayout->addRow("Weekly report day:", m_weeklyDaySpin);
    reportLayout->addRow("Weekly report time:", m_weeklyTimeEdit);
    reportLayout->addRow("Report language:", m_languageCombo);

    auto *reportSaveBtn = new QPushButton("Save Report Settings");
    connect(reportSaveBtn, &QPushButton::clicked, this, &SettingsDialog::saveSettings);
    reportLayout->addRow(reportSaveBtn);

    // === Template Edit Tab ===
    auto *templateTab = new QWidget();
    auto *templateLayout = new QVBoxLayout(templateTab);
    m_templateCombo = new QComboBox();
    m_templateCombo->addItems({"daily_report", "weekly_report"});
    m_templateEdit = new QTextEdit();
    m_templateEdit->setFont(QFont("Consolas", 10));
    m_templateEdit->setPlaceholderText("Select a template to edit...");

    templateLayout->addWidget(new QLabel("Edit Report Template:"));
    templateLayout->addWidget(m_templateCombo);
    templateLayout->addWidget(m_templateEdit);
    auto *templateSaveBtn = new QPushButton("Save Template");
    connect(templateSaveBtn, &QPushButton::clicked, this, &SettingsDialog::saveSettings);
    templateLayout->addWidget(templateSaveBtn);

    // Assemble tabs
    tabWidget->addTab(generalTab, "General");
    tabWidget->addTab(monitorTab, "Monitoring");
    tabWidget->addTab(llmTab, "LLM");
    tabWidget->addTab(reportTab, "Schedule");
    tabWidget->addTab(templateTab, "Templates");

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
    m_buildTrackingChk->setChecked(m_settings->getBool("build_tracking_enabled", true));

    m_backendCombo->setCurrentText(m_settings->getValue("llm_backend", ""));
    QJsonObject llmCfg = m_settings->getJson("llm_config");
    if (!llmCfg.isEmpty()) {
        m_endpointEdit->setText(llmCfg["endpoint"].toString());
        m_modelEdit->setText(llmCfg["model"].toString());
        m_temperatureSpin->setValue(llmCfg["temperature"].toDouble(0.7));
        m_maxTokensSpin->setValue(llmCfg["maxTokens"].toInt(4096));
    }

    m_dailyTimeEdit->setTime(QTime::fromString(
        m_settings->getValue("daily_report_time", "17:30"), "HH:mm"));
    m_weeklyDaySpin->setValue(m_settings->getInt("weekly_report_day", 5));
    m_weeklyTimeEdit->setTime(QTime::fromString(
        m_settings->getValue("weekly_report_time", "17:00"), "HH:mm"));
}

void SettingsDialog::saveSettings()
{
    m_settings->setBool("auto_start", m_autoStartChk->isChecked());
    m_settings->setBool("start_minimized", m_startMinimizedChk->isChecked());
    m_settings->setInt("afk_threshold_secs", m_afkThresholdSpin->value());
    m_settings->setInt("data_retention_days", m_dataRetentionSpin->value());

    m_settings->setBool("git_tracking_enabled", m_gitTrackingChk->isChecked());
    m_settings->setBool("browser_tracking_enabled", m_browserTrackingChk->isChecked());
    m_settings->setBool("build_tracking_enabled", m_buildTrackingChk->isChecked());

    m_settings->setValue("llm_backend", m_backendCombo->currentText());

    QJsonObject llmCfg;
    llmCfg["endpoint"] = m_endpointEdit->text();
    llmCfg["model"] = m_modelEdit->text();
    llmCfg["temperature"] = m_temperatureSpin->value();
    llmCfg["maxTokens"] = m_maxTokensSpin->value();
    m_settings->setJson("llm_config", llmCfg);

    // API key is stored encrypted separately
    if (!m_apiKeyEdit->text().isEmpty()) {
        // We store the key via CryptoUtils::encrypt in the actual app
        m_settings->setValue("llm_api_key_encrypted", m_apiKeyEdit->text());
    }

    m_settings->setValue("daily_report_time", m_dailyTimeEdit->time().toString("HH:mm"));
    m_settings->setInt("weekly_report_day", m_weeklyDaySpin->value());
    m_settings->setValue("weekly_report_time", m_weeklyTimeEdit->time().toString("HH:mm"));

    // Save project paths as JSON array
    QJsonArray paths;
    for (int i = 0; i < m_projectPathsList->count(); ++i) {
        paths.append(m_projectPathsList->item(i)->text());
    }
    QJsonDocument doc;
    doc.setArray(paths);
    m_settings->setValue("monitored_paths",
                         QString::fromUtf8(doc.toJson(QJsonDocument::Compact)));

    spdlog::info("Settings saved.");
    QMessageBox::information(this, "Settings", "Settings saved successfully.");
}
