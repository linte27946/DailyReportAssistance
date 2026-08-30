#pragma once

#include "IMonitor.h"
#include <QJsonObject>
#include <QList>
#include <QProcess>
#include <QTimer>

struct WeComMeetingListPage {
    QList<QJsonObject> meetingIds;
    bool hasMore = false;
    QString nextCursor;
};

struct WeComMeetingDetails {
    QString meetingId;
    QString subMeetingId;
    QString subject;
    QDateTime enteredAt;
    QDateTime quitAt;
};

/// Periodically imports the current user's actual WeCom meeting attendance by
/// calling the official, read-only wecom-cli meeting commands.
class WeComMeetingMonitor final : public IMonitor {
    Q_OBJECT

public:
    explicit WeComMeetingMonitor(QObject *parent = nullptr);

    bool start() override;
    void stop() override;
    QString name() const override { return QStringLiteral("WeComMeetingMonitor"); }

    void setCliPath(const QString &path) { m_cliPath = path.trimmed(); }
    void setSyncIntervalMinutes(int minutes);
    void setIdleThresholdPercent(int percent);
    void setEnabled(bool enabled);

    static QString resolveCliPath(const QString &configuredPath);
    static bool parseAuthorizationResponse(const QByteArray &output,
                                           bool *authorized,
                                           QString *botId = nullptr,
                                           QString *errorMessage = nullptr);
    static bool parseListResponse(const QByteArray &output,
                                  WeComMeetingListPage *page,
                                  QString *errorMessage = nullptr);
    static bool parseDetailsResponse(const QByteArray &output,
                                     QList<WeComMeetingDetails> *meetings,
                                     QString *errorMessage = nullptr);

public slots:
    void syncNow();

signals:
    void syncStarted();
    void syncFinished(int attendedMeetingCount);
    void syncFailed(const QString &message);

private:
    enum class RequestKind { None, List, Details };

    void requestList(const QString &cursor = {});
    void requestNextDetailsBatch();
    void runCli(RequestKind kind, const QStringList &arguments);
    void handleProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void failSync(const QString &message);
    void finishSync();
    RawEvent toRawEvent(const WeComMeetingDetails &meeting) const;

    QString m_cliPath = QStringLiteral("wecom-cli");
    QString m_resolvedCliPath;
    int m_syncIntervalMinutes = 30;
    int m_idleThresholdPercent = 30;
    bool m_enabled = false;
    bool m_syncing = false;
    bool m_commandActive = false;
    RequestKind m_requestKind = RequestKind::None;
    QList<QJsonObject> m_pendingMeetingIds;
    int m_nextDetailsIndex = 0;
    int m_attendedMeetings = 0;
    QProcess *m_process = nullptr;
    QTimer *m_syncTimer = nullptr;
    QTimer *m_timeoutTimer = nullptr;
};
