#pragma once

#include <QWizard>

class SettingsRepository;

/// First-run setup wizard to configure monitoring paths, LLM backend, and preferences.
class SetupWizard : public QWizard {
    Q_OBJECT

public:
    explicit SetupWizard(SettingsRepository *settings, QWidget *parent = nullptr);

private:
    QWizardPage *createWelcomePage();
    QWizardPage *createMonitoringPage();
    QWizardPage *createLlmPage();
    QWizardPage *createTemplatePage();
    QWizardPage *createFinishPage();

    SettingsRepository *m_settings = nullptr;
};
