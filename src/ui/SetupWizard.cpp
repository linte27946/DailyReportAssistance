#include "SetupWizard.h"
#include "storage/SettingsRepository.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QListWidget>
#include <QPushButton>
#include <QFileDialog>
#include <QCheckBox>
#include <QFormLayout>

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
    auto *list = new QListWidget(page);
    auto *btnLayout = new QHBoxLayout();
    auto *addBtn = new QPushButton("Add Directory...");
    auto *removeBtn = new QPushButton("Remove");
    btnLayout->addWidget(addBtn);
    btnLayout->addWidget(removeBtn);
    btnLayout->addStretch();

    connect(addBtn, &QPushButton::clicked, page, [list]() {
        QString dir = QFileDialog::getExistingDirectory(nullptr, "Select Project Directory");
        if (!dir.isEmpty()) list->addItem(dir);
    });
    connect(removeBtn, &QPushButton::clicked, page, [list]() {
        delete list->currentItem();
    });

    layout->addWidget(new QLabel("Project directories:", page));
    layout->addWidget(list);
    layout->addLayout(btnLayout);
    return page;
}

QWizardPage *SetupWizard::createLlmPage()
{
    auto *page = new QWizardPage(this);
    page->setTitle("Configure LLM Backend");
    page->setSubTitle("Choose the AI backend for generating reports.");

    auto *layout = new QFormLayout(page);
    auto *backendCombo = new QComboBox(page);
    backendCombo->addItems({"OpenAI", "Anthropic", "Ollama (local)"});

    auto *apiKeyEdit = new QLineEdit(page);
    apiKeyEdit->setPlaceholderText("sk-... or leave blank for Ollama");

    auto *modelCombo = new QComboBox(page);
    modelCombo->setEditable(true);
    modelCombo->addItems({
        "gpt-4o", "gpt-4o-mini",
        "claude-sonnet-4-20250514", "claude-opus-4-20250514",
        "llama3", "codestral"
    });

    auto *endpointEdit = new QLineEdit(page);
    endpointEdit->setPlaceholderText("Auto-detected based on backend");

    layout->addRow("Backend:", backendCombo);
    layout->addRow("API Key:", apiKeyEdit);
    layout->addRow("Model:", modelCombo);
    layout->addRow("Custom Endpoint:", endpointEdit);

    return page;
}

QWizardPage *SetupWizard::createTemplatePage()
{
    auto *page = new QWizardPage(this);
    page->setTitle("Report Preferences");
    page->setSubTitle("Configure when and how reports are generated.");

    auto *layout = new QFormLayout(page);
    auto *dailyTime = new QLineEdit("17:30");
    auto *weeklyDay = new QComboBox();
    weeklyDay->addItems({"Monday", "Tuesday", "Wednesday", "Thursday", "Friday"});
    weeklyDay->setCurrentIndex(4); // Friday
    auto *weeklyTime = new QLineEdit("17:00");
    auto *languageCombo = new QComboBox();
    languageCombo->addItems({"English", "Chinese (中文)", "Japanese (日本語)"});

    layout->addRow("Daily report time:", dailyTime);
    layout->addRow("Weekly report day:", weeklyDay);
    layout->addRow("Weekly report time:", weeklyTime);
    layout->addRow("Report language:", languageCombo);

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
