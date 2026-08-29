#include "SetupWizard.h"
#include "storage/SettingsRepository.h"
#include "util/CryptoUtils.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QListWidget>
#include <QPushButton>
#include <QFileDialog>
#include <QCheckBox>
#include <QFormLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

SetupWizard::SetupWizard(SettingsRepository *settings, QWidget *parent)
    : QWizard(parent)
    , m_settings(settings)
{
    setWindowTitle("DailyReport - Setup Wizard");
    setMinimumSize(600, 450);

    addPage(createWelcomePage());
    addPage(createMonitoringPage());
    addPage(createLlmPage());
    addPage(createTemplatePage());
    addPage(createFinishPage());
}

QWizardPage *SetupWizard::createWelcomePage()
{
    auto *page = new QWizardPage(this);
    page->setTitle("Welcome to DailyReport");
    auto *layout = new QVBoxLayout(page);

    auto *label = new QLabel(
        "DailyReport will automatically track your development activities and "
        "generate natural-language daily and weekly reports using AI.\n\n"
        "This wizard will help you configure:\n"
        "• Project directories to monitor\n"
        "• Your preferred LLM backend\n"
        "• Report schedule preferences\n\n"
        "All data is stored locally on your machine. Your privacy is respected.",
        page);
    label->setWordWrap(true);
    layout->addWidget(label);
    return page;
}

QWizardPage *SetupWizard::createMonitoringPage()
{
    auto *page = new QWizardPage(this);
    page->setTitle("Select Project Directories");
    page->setSubTitle("Choose which directories to monitor for code changes.");

    auto *layout = new QVBoxLayout(page);
    m_projectPaths = new QListWidget(page);
    auto *btnLayout = new QHBoxLayout();
    auto *addBtn = new QPushButton("Add Directory...");
    auto *removeBtn = new QPushButton("Remove");
    btnLayout->addWidget(addBtn);
    btnLayout->addWidget(removeBtn);
    btnLayout->addStretch();

    connect(addBtn, &QPushButton::clicked, page, [this]() {
        QString dir = QFileDialog::getExistingDirectory(nullptr, "Select Project Directory");
        if (!dir.isEmpty()) m_projectPaths->addItem(dir);
    });
    connect(removeBtn, &QPushButton::clicked, page, [this]() {
        delete m_projectPaths->currentItem();
    });

    layout->addWidget(new QLabel("Project directories:", page));
    layout->addWidget(m_projectPaths);
    layout->addLayout(btnLayout);
    return page;
}

QWizardPage *SetupWizard::createLlmPage()
{
    auto *page = new QWizardPage(this);
    page->setTitle("Configure LLM Backend");
    page->setSubTitle("Choose the AI backend for generating reports.");

    auto *layout = new QFormLayout(page);
    m_backend = new QComboBox(page);
    m_backend->addItems({"OpenAI", "Anthropic", "Ollama (local)"});

    m_apiKey = new QLineEdit(page);
    m_apiKey->setEchoMode(QLineEdit::Password);
    m_apiKey->setPlaceholderText("sk-... or leave blank for Ollama");

    m_model = new QComboBox(page);
    m_model->setEditable(true);
    m_model->addItems({
        "gpt-4o", "gpt-4o-mini",
        "claude-sonnet-4-20250514", "claude-opus-4-20250514",
        "llama3", "codestral"
    });

    m_endpoint = new QLineEdit(page);
    m_endpoint->setPlaceholderText("Auto-detected based on backend");

    layout->addRow("Backend:", m_backend);
    layout->addRow("API Key:", m_apiKey);
    layout->addRow("Model:", m_model);
    layout->addRow("Custom Endpoint:", m_endpoint);

    return page;
}

QWizardPage *SetupWizard::createTemplatePage()
{
    auto *page = new QWizardPage(this);
    page->setTitle("Report Preferences");
    page->setSubTitle("Configure when and how reports are generated.");

    auto *layout = new QFormLayout(page);
    m_dailyTime = new QLineEdit("17:30");
    m_weeklyDay = new QComboBox();
    m_weeklyDay->addItems({"Monday", "Tuesday", "Wednesday", "Thursday", "Friday"});
    m_weeklyDay->setCurrentIndex(4); // Friday
    m_weeklyTime = new QLineEdit("17:00");
    m_language = new QComboBox();
    m_language->addItems({"English", "Chinese (中文)", "Japanese (日本語)"});
    m_language->setCurrentIndex(1);

    layout->addRow("Daily report time:", m_dailyTime);
    layout->addRow("Weekly report day:", m_weeklyDay);
    layout->addRow("Weekly report time:", m_weeklyTime);
    layout->addRow("Report language:", m_language);

    return page;
}

QWizardPage *SetupWizard::createFinishPage()
{
    auto *page = new QWizardPage(this);
    page->setTitle("Setup Complete");
    auto *layout = new QVBoxLayout(page);

    auto *label = new QLabel(
        "DailyReport is ready to start monitoring!\n\n"
        "The application will run in the system tray and automatically:\n"
        "• Track your coding activity across configured directories\n"
        "• Monitor Git commits in those projects\n"
        "• Generate daily reports at the scheduled time\n\n"
        "You can change these settings anytime from the system tray menu.",
        page);
    label->setWordWrap(true);
    layout->addWidget(label);
    return page;
}

void SetupWizard::accept()
{
    QJsonArray paths;
    for (int i = 0; i < m_projectPaths->count(); ++i)
        paths.append(m_projectPaths->item(i)->text());
    m_settings->setValue(
        "monitored_paths",
        QString::fromUtf8(QJsonDocument(paths).toJson(QJsonDocument::Compact)));

    const QString backend = m_backend->currentIndex() == 2
        ? "Ollama" : m_backend->currentText();
    m_settings->setValue("llm_backend", backend);

    QJsonObject llmConfig;
    llmConfig["endpoint"] = m_endpoint->text().trimmed();
    llmConfig["model"] = m_model->currentText().trimmed();
    llmConfig["temperature"] = 0.7;
    llmConfig["maxTokens"] = 4096;
    llmConfig["timeoutSecs"] = 120;
    m_settings->setJson("llm_config", llmConfig);
    if (!m_apiKey->text().isEmpty()) {
        m_settings->setValue(
            "llm_api_key_encrypted",
            QString::fromUtf8(CryptoUtils::encrypt(m_apiKey->text().toUtf8())));
    }

    const QTime daily = QTime::fromString(m_dailyTime->text(), "HH:mm");
    const QTime weekly = QTime::fromString(m_weeklyTime->text(), "HH:mm");
    m_settings->setValue("daily_report_time",
                         (daily.isValid() ? daily : QTime(17, 30)).toString("HH:mm"));
    m_settings->setInt("weekly_report_day", m_weeklyDay->currentIndex() + 1);
    m_settings->setValue("weekly_report_time",
                         (weekly.isValid() ? weekly : QTime(17, 0)).toString("HH:mm"));
    const QString language = m_language->currentIndex() == 0
        ? "en" : m_language->currentIndex() == 2 ? "ja-JP" : "zh-CN";
    m_settings->setValue("language", language);
    m_settings->setBool("setup_complete", true);

    QWizard::accept();
}
