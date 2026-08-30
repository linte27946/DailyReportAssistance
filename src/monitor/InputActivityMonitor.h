#pragma once

#include "IMonitor.h"
#include <QTimer>

/// Detects user idle time by polling GetLastInputInfo().
/// Emits UserActive/UserIdle events to distinguish work from AFK periods.
class InputActivityMonitor : public IMonitor {
    Q_OBJECT

public:
    explicit InputActivityMonitor(QObject *parent = nullptr);
    ~InputActivityMonitor() override;

    bool start() override;
    void stop() override;
    QString name() const override { return "InputActivityMonitor"; }

    /// Set the AFK threshold in seconds. Default: 300 (5 minutes).
    void setAfkThreshold(int seconds) { m_afkThresholdSecs = seconds; }
    int afkThreshold() const { return m_afkThresholdSecs; }

    /// Get the current idle time in seconds.
    static int getIdleTimeSecs();

private:
    void checkActivity();
    void emitStateSnapshot(const QDateTime &timestamp, const QString &reason);

    QTimer *m_checkTimer = nullptr;
    bool m_isIdle = false;
    QDateTime m_idleStartTime;
    QDate m_lastStateEmissionDate;
    int m_afkThresholdSecs = 300;  // 5 minutes default
    static constexpr int kCheckIntervalMs = 5000;  // Check every 5 seconds
};
