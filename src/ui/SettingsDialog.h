#pragma once

#include <QDialog>
#include <QTabWidget>
#include <QLineEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QTimeEdit>
#include <QListWidget>
#include <QTextEdit>

class SettingsRepository;

/// Settings dialog with tabs for General, Monitoring, LLM, and Reports.
class SettingsDialog : public QWidget {
    Q_OBJECT

public:
    explicit SettingsDialog(SettingsRepository *settings, QWidget *parent = nullptr);

    void loadSettings();
    void saveSettings();

private:
    void setupUi();

    // General tab widgets
    QCheckBox *m_autoStartChk = nullptr;
    QCheckBox *m_startMinimizedChk = nullptr;
    QSpinBox *m_afkThresholdSpin = nullptr;
    QSpinBox *m_dataRetentionSpin = nullptr;

    // Monitoring tab widgets
    QListWidget *m_projectPathsList = nullptr;
    QCheckBox *m_gitTrackingChk = nullptr;
    QCheckBox *m_browserTrackingChk = nullptr;
    QCheckBox *m_buildTrackingChk = nullptr;

    // LLM tab widgets
    QComboBox *m_backendCombo = nullptr;
    QLineEdit *m_endpointEdit = nullptr;
    QLineEdit *m_apiKeyEdit = nullptr;
    QLineEdit *m_modelEdit = nullptr;
    QDoubleSpinBox *m_temperatureSpin = nullptr;
    QSpinBox *m_maxTokensSpin = nullptr;
    QCheckBox *m_showKeyChk = nullptr;

    // Report tab widgets
    QComboBox *m_templateCombo = nullptr;
    QTextEdit *m_templateEdit = nullptr;
    QTimeEdit *m_dailyTimeEdit = nullptr;
    QSpinBox *m_weeklyDaySpin = nullptr;
    QTimeEdit *m_weeklyTimeEdit = nullptr;
    QComboBox *m_languageCombo = nullptr;

    SettingsRepository *m_settings = nullptr;
};
