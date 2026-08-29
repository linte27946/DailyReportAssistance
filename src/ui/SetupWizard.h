#pragma once

#include <QWizard>

class SettingsRepository;
class QListWidget;
class QComboBox;
class QLineEdit;

/// First-run setup wizard to configure monitoring paths, LLM backend, and preferences.
class SetupWizard : public QWizard {
    Q_OBJECT

public:
    explicit SetupWizard(SettingsRepository *settings, QWidget *parent = nullptr);

    void accept() override;

private:
    QWizardPage *createWelcomePage();
    QWizardPage *createMonitoringPage();
    QWizardPage *createLlmPage();
    QWizardPage *createTemplatePage();
    QWizardPage *createFinishPage();

    SettingsRepository *m_settings = nullptr;
    QListWidget *m_projectPaths = nullptr;
    QComboBox *m_backend = nullptr;
    QLineEdit *m_apiKey = nullptr;
    QComboBox *m_model = nullptr;
    QLineEdit *m_endpoint = nullptr;
    QLineEdit *m_dailyTime = nullptr;
    QComboBox *m_weeklyDay = nullptr;
    QLineEdit *m_weeklyTime = nullptr;
    QComboBox *m_language = nullptr;
};
