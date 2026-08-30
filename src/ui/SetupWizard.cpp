#include "SetupWizard.h"
#include "UiLanguage.h"
#include "DialogUtils.h"
#include "storage/SettingsRepository.h"
#include "util/CryptoUtils.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QListWidget>
#include <QPushButton>
#include <QCheckBox>
#include <QFormLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

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

SetupWizard::SetupWizard(SettingsRepository *settings, QWidget *parent)
    : QWizard(parent)
    , m_settings(settings)
{
    UiLanguage::bindWindowTitle(this,
                                "DailyReport - Setup wizard", "DailyReport - 初始设置");
    setMinimumSize(600, 450);
    resize(760, 540);
    setWizardStyle(QWizard::ModernStyle);
    setOption(QWizard::NoBackButtonOnStartPage, true);
    DialogUtils::applyStyle(this);

    addPage(createWelcomePage());
    addPage(createMonitoringPage());
    addPage(createLlmPage());
    addPage(createTemplatePage());
    addPage(createFinishPage());

    UiLanguage::bindText(button(QWizard::BackButton), "Back", "上一步");
    UiLanguage::bindText(button(QWizard::NextButton), "Next", "下一步");
    UiLanguage::bindText(button(QWizard::FinishButton), "Finish", "完成");
    UiLanguage::bindText(button(QWizard::CancelButton), "Cancel", "取消");
}

QWizardPage *SetupWizard::createWelcomePage()
{
    auto *page = new QWizardPage(this);
    UiLanguage::bindText(page, "Welcome to DailyReport", "欢迎使用 DailyReport");
    auto *layout = new QVBoxLayout(page);

    auto *label = new QLabel(page);
    UiLanguage::bindText(
        label,
        "DailyReport will automatically track your development activities and "
        "generate natural-language daily and weekly reports using AI.\n\n"
        "This wizard will help you configure:\n"
        "• Project directories to monitor\n"
        "• Your preferred LLM backend\n"
        "• Report schedule preferences\n\n"
        "All data is stored locally on your machine. Your privacy is respected.",
        "DailyReport 会自动记录你的开发活动，并使用 AI 生成自然语言日报和周报。\n\n"
        "此向导将帮助你配置：\n"
        "• 需要监控的项目目录\n"
        "• 使用的 AI 服务\n"
        "• 报告生成计划\n\n"
        "活动数据保存在本机，我们会认真保护你的隐私。");
    label->setWordWrap(true);
    layout->addWidget(label);
    return page;
}

QWizardPage *SetupWizard::createMonitoringPage()
{
    auto *page = new QWizardPage(this);
    UiLanguage::bindText(page, "Select project directories", "选择项目目录");
    UiLanguage::bindSubtitle(page,
                             "Choose which directories to monitor for code changes.",
                             "选择要监控代码变更的目录。");

    auto *layout = new QVBoxLayout(page);
    m_projectPaths = new QListWidget(page);
    auto *btnLayout = new QHBoxLayout();
    auto *addBtn = new QPushButton();
    auto *removeBtn = new QPushButton();
    UiLanguage::bindText(addBtn, "Add directory...", "添加目录……");
    UiLanguage::bindText(removeBtn, "Remove", "移除");
    btnLayout->addWidget(addBtn);
    btnLayout->addWidget(removeBtn);
    btnLayout->addStretch();

    connect(addBtn, &QPushButton::clicked, page, [this]() {
        const QString dir = DialogUtils::selectDirectory(
            this, UiLanguage::text("Select project directory", "选择项目目录"));
        if (!dir.isEmpty()) m_projectPaths->addItem(dir);
    });
    connect(removeBtn, &QPushButton::clicked, page, [this]() {
        delete m_projectPaths->currentItem();
    });

    auto *pathsLabel = new QLabel(page);
    UiLanguage::bindText(pathsLabel, "Project directories:", "项目目录：");
    layout->addWidget(pathsLabel);
    layout->addWidget(m_projectPaths);
    layout->addLayout(btnLayout);
    return page;
}

QWizardPage *SetupWizard::createLlmPage()
{
    auto *page = new QWizardPage(this);
    UiLanguage::bindText(page, "Configure AI provider", "配置 AI 服务");
    UiLanguage::bindSubtitle(page,
                             "Choose the AI provider used to generate reports.",
                             "选择用于生成报告的 AI 服务。");

    auto *layout = new QFormLayout(page);
    m_backend = new QComboBox(page);
    m_backend->addItem("OpenAI", "OpenAI");
    m_backend->addItem("Anthropic", "Anthropic");
    m_backend->addItem("DeepSeek", "DeepSeek");
    m_backend->addItem("", "Ollama");
    UiLanguage::bindComboItem(m_backend, 3, "Ollama (local)", "Ollama（本地）");

    m_apiKey = new QLineEdit(page);
    m_apiKey->setEchoMode(QLineEdit::Password);
    UiLanguage::bindPlaceholder(
        m_apiKey, "sk-... or leave blank for Ollama", "sk-...；使用 Ollama 时可留空");

    m_model = new QComboBox(page);
    m_model->setEditable(true);
    m_model->addItems({
        "gpt-4o", "gpt-4o-mini",
        "claude-sonnet-4-20250514", "claude-opus-4-20250514",
        "llama3", "codestral"
    });

    m_endpoint = new QLineEdit(page);
    UiLanguage::bindPlaceholder(
        m_endpoint, "Auto-detected based on provider", "根据 AI 服务自动识别");
    connect(m_backend, &QComboBox::activated, page, [this](int) {
        const QString backend = m_backend->currentData().toString();
        if (backend == "DeepSeek") {
            m_endpoint->setText("https://api.deepseek.com/chat/completions");
            m_model->setCurrentText("deepseek-v4-flash");
        } else if (backend == "OpenAI") {
            m_endpoint->setText("https://api.openai.com/v1/chat/completions");
            m_model->setCurrentText("gpt-4o");
        } else if (backend == "Anthropic") {
            m_endpoint->setText("https://api.anthropic.com/v1/messages");
            m_model->setCurrentText("claude-sonnet-4-20250514");
        } else if (backend == "Ollama") {
            m_endpoint->setText("http://localhost:11434/api/generate");
            m_model->setCurrentText("llama3");
        }
    });

    addLocalizedRow(layout, "AI provider:", "AI 服务：", m_backend);
    addLocalizedRow(layout, "API key:", "API 密钥：", m_apiKey);
    addLocalizedRow(layout, "Model:", "模型：", m_model);
    addLocalizedRow(layout, "Custom endpoint:", "自定义 API 地址：", m_endpoint);

    return page;
}

QWizardPage *SetupWizard::createTemplatePage()
{
    auto *page = new QWizardPage(this);
    UiLanguage::bindText(page, "Report preferences", "报告设置");
    UiLanguage::bindSubtitle(page,
                             "Configure when and how reports are generated.",
                             "设置报告的生成时间和语言。");

    auto *layout = new QFormLayout(page);
    m_dailyTime = new QLineEdit("17:30");
    m_weeklyDay = new QComboBox();
    m_weeklyDay->addItems({"", "", "", "", "", "", ""});
    const QStringList days = {"Monday", "Tuesday", "Wednesday", "Thursday",
                              "Friday", "Saturday", "Sunday"};
    const QStringList daysZh = {"星期一", "星期二", "星期三", "星期四",
                                "星期五", "星期六", "星期日"};
    for (int i = 0; i < days.size(); ++i)
        UiLanguage::bindComboItem(m_weeklyDay, i, days.at(i), daysZh.at(i));
    m_weeklyDay->setCurrentIndex(4); // Friday
    m_weeklyTime = new QLineEdit("17:00");
    m_language = new QComboBox();
    m_language->addItem("", "en");
    m_language->addItem("", "zh-CN");
    UiLanguage::bindComboItem(m_language, 0, "English", "English");
    UiLanguage::bindComboItem(m_language, 1, "Simplified Chinese", "简体中文");
    m_language->setCurrentIndex(UiLanguage::isChinese() ? 1 : 0);
    connect(m_language, &QComboBox::currentIndexChanged, this, [this](int) {
        UiLanguage::setLanguage(m_language->currentData().toString());
        UiLanguage::apply(this);
    });

    addLocalizedRow(layout, "Daily report time:", "日报生成时间：", m_dailyTime);
    addLocalizedRow(layout, "Weekly report day:", "周报生成日期：", m_weeklyDay);
    addLocalizedRow(layout, "Weekly report time:", "周报生成时间：", m_weeklyTime);
    addLocalizedRow(layout, "Interface and report language:",
                    "界面与报告语言：", m_language);

    return page;
}

QWizardPage *SetupWizard::createFinishPage()
{
    auto *page = new QWizardPage(this);
    UiLanguage::bindText(page, "Setup complete", "设置完成");
    auto *layout = new QVBoxLayout(page);

    auto *label = new QLabel(page);
    UiLanguage::bindText(
        label,
        "DailyReport is ready to start monitoring!\n\n"
        "The application will run in the system tray and automatically:\n"
        "• Track your coding activity across configured directories\n"
        "• Monitor Git commits in those projects\n"
        "• Generate daily reports at the scheduled time\n\n"
        "You can change these settings anytime from the system tray menu.",
        "DailyReport 已准备好开始监控！\n\n"
        "程序将在系统托盘中运行，并自动：\n"
        "• 记录所选目录中的编码活动\n"
        "• 监控这些项目中的 Git 提交\n"
        "• 在计划时间生成日报\n\n"
        "你可以随时从系统托盘菜单修改这些设置。");
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

    const QString backend = m_backend->currentData().toString();
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
    const QString language = m_language->currentData().toString();
    m_settings->setValue("language", language);
    m_settings->setBool("setup_complete", true);

    QWizard::accept();
}
