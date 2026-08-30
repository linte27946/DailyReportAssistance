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
#include <QProcess>

class SettingsRepository;
class TemplateEngine;
class DataRetentionService;
class WeComMeetingMonitor;
class QLabel;
class QPushButton;
class QTimer;

/// Settings dialog with tabs for General, Monitoring, LLM, and Reports.
class SettingsDialog : public QWidget {
    Q_OBJECT

public:
    explicit SettingsDialog(SettingsRepository *settings,
                            TemplateEngine *templateEngine,
                            DataRetentionService *retentionService,
                            WeComMeetingMonitor *weComMeetingMonitor = nullptr,
                            QWidget *parent = nullptr);

    void loadSettings();
    void saveSettings();

signals:
    void settingsSaved();

private:
    void setupUi();
    void saveRetentionSettings();
    void checkWeComAuthorization();
    void finishWeComAuthorizationCheck(int exitCode,
                                       QProcess::ExitStatus exitStatus);
    void setWeComStatus(const char *state,
                        const QString &english,
                        const QString &chinese);
    void updateWeComActionState();

    // General tab widgets
    QCheckBox *m_autoStartChk = nullptr;
    QCheckBox *m_startMinimizedChk = nullptr;
    QSpinBox *m_afkThresholdSpin = nullptr;

    // Data retention tab widgets
    QSpinBox *m_activityRetentionSpin = nullptr;
    QSpinBox *m_reportRetentionSpin = nullptr;
    QLabel *m_cleanupStatusLabel = nullptr;
    QLabel *m_saveStatusLabel = nullptr;

    // Monitoring tab widgets
    QListWidget *m_projectPathsList = nullptr;
    QCheckBox *m_gitTrackingChk = nullptr;
    QCheckBox *m_browserTrackingChk = nullptr;
    QCheckBox *m_browserFullUrlChk = nullptr;
    QCheckBox *m_distractionTrackingChk = nullptr;
    QCheckBox *m_buildTrackingChk = nullptr;
    QCheckBox *m_editorTrackingChk = nullptr;
    QCheckBox *m_documentTrackingChk = nullptr;

    // WeCom integration tab widgets
    QCheckBox *m_weComMeetingChk = nullptr;
    QLineEdit *m_weComCliPathEdit = nullptr;
    QSpinBox *m_weComSyncIntervalSpin = nullptr;
    QSpinBox *m_weComIdleThresholdSpin = nullptr;
    QLabel *m_weComStatusLabel = nullptr;
    QLabel *m_weComSyncStatusLabel = nullptr;
    QPushButton *m_weComAuthCheckButton = nullptr;
    QPushButton *m_weComSyncButton = nullptr;
    QProcess *m_weComAuthProcess = nullptr;
    QTimer *m_weComAuthTimeout = nullptr;
    bool m_weComAuthorized = false;
    bool m_weComAuthChecking = false;
    bool m_weComSyncing = false;

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
    QComboBox *m_weeklyDayCombo = nullptr;
    QTimeEdit *m_weeklyTimeEdit = nullptr;
    QComboBox *m_languageCombo = nullptr;

    SettingsRepository *m_settings = nullptr;
    TemplateEngine *m_templateEngine = nullptr;
    DataRetentionService *m_retentionService = nullptr;
    WeComMeetingMonitor *m_weComMeetingMonitor = nullptr;
};
