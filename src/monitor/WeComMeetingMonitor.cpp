#include "WeComMeetingMonitor.h"

#include <QDate>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QRegularExpression>
#include <QSet>
#include <QStandardPaths>
#include <QTimeZone>
#include <spdlog/spdlog.h>
#include <utility>

namespace {

QJsonObject findPayload(const QJsonObject &object,
                        const QStringList &expectedKeys)
{
    for (const QString &key : expectedKeys) {
        if (object.contains(key)) return object;
    }
    for (const QString &wrapper : {QStringLiteral("data"),
                                   QStringLiteral("result"),
                                   QStringLiteral("response")}) {
        const QJsonObject nested = object.value(wrapper).toObject();
        if (!nested.isEmpty()) {
            const QJsonObject payload = findPayload(nested, expectedKeys);
            if (!payload.isEmpty()) return payload;
        }
    }
    return {};
}

QJsonObject parseObject(const QByteArray &raw, const QStringList &expectedKeys,
                        QString *errorMessage)
{
    QByteArray json = raw.trimmed();
    QJsonParseError parseError;
    QJsonDocument document = QJsonDocument::fromJson(json, &parseError);

    // Some command-line versions print a short status line before their JSON.
    // Keep parsing strict, but tolerate that harmless prefix/suffix.
    if (!document.isObject()) {
        const qsizetype first = json.indexOf('{');
        const qsizetype last = json.lastIndexOf('}');
        if (first >= 0 && last > first) {
            json = json.mid(first, last - first + 1);
            document = QJsonDocument::fromJson(json, &parseError);
        }
    }

    if (!document.isObject()) {
        if (errorMessage) {
            *errorMessage = QString("Invalid JSON from wecom-cli: %1")
                .arg(parseError.errorString());
        }
        return {};
    }

    const QJsonObject payload = findPayload(document.object(), expectedKeys);
    if (payload.isEmpty() && errorMessage)
        *errorMessage = QStringLiteral("wecom-cli response is missing expected meeting data.");
    return payload;
}

QDateTime parseMeetingTime(const QString &value, const QString &timeZoneId)
{
    const QDateTime parsed = QDateTime::fromString(value, "yyyy-MM-dd HH:mm:ss");
    if (!parsed.isValid()) return {};

    const QTimeZone zone(timeZoneId.toUtf8());
    if (zone.isValid())
        return QDateTime(parsed.date(), parsed.time(), zone).toUTC();
    return QDateTime(parsed.date(), parsed.time(), Qt::LocalTime).toUTC();
}

QString processErrorText(QProcess *process)
{
    const QString standardError = QString::fromUtf8(
        process->readAllStandardError()).trimmed();
    return standardError.isEmpty() ? process->errorString() : standardError;
}

} // namespace

WeComMeetingMonitor::WeComMeetingMonitor(QObject *parent)
    : IMonitor(parent)
    , m_process(new QProcess(this))
    , m_syncTimer(new QTimer(this))
    , m_timeoutTimer(new QTimer(this))
{
    m_syncTimer->setTimerType(Qt::VeryCoarseTimer);
    m_timeoutTimer->setSingleShot(true);

    connect(m_syncTimer, &QTimer::timeout,
            this, &WeComMeetingMonitor::syncNow);
    connect(m_process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this, &WeComMeetingMonitor::handleProcessFinished);
    connect(m_process, &QProcess::errorOccurred, this,
            [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart && m_commandActive)
            failSync(processErrorText(m_process));
    });
    connect(m_timeoutTimer, &QTimer::timeout, this, [this]() {
        if (!m_commandActive) return;
        m_commandActive = false;
        m_process->kill();
        failSync(QStringLiteral("wecom-cli timed out after 30 seconds."));
    });
}

void WeComMeetingMonitor::setSyncIntervalMinutes(int minutes)
{
    m_syncIntervalMinutes = qBound(5, minutes, 24 * 60);
    if (m_syncTimer->isActive())
        m_syncTimer->start(m_syncIntervalMinutes * 60 * 1000);
}

void WeComMeetingMonitor::setIdleThresholdPercent(int percent)
{
    m_idleThresholdPercent = qBound(1, percent, 99);
}

void WeComMeetingMonitor::setEnabled(bool enabled)
{
    if (m_enabled == enabled) return;
    m_enabled = enabled;
    if (!isRunning()) return;

    if (enabled) {
        m_syncTimer->start(m_syncIntervalMinutes * 60 * 1000);
        QTimer::singleShot(0, this, &WeComMeetingMonitor::syncNow);
    } else {
        m_syncTimer->stop();
        m_timeoutTimer->stop();
        m_commandActive = false;
        m_syncing = false;
        if (m_process->state() != QProcess::NotRunning)
            m_process->kill();
    }
}

QString WeComMeetingMonitor::resolveCliPath(const QString &configuredPath)
{
    const QString value = configuredPath.trimmed().isEmpty()
        ? QStringLiteral("wecom-cli") : configuredPath.trimmed();
    const QFileInfo explicitFile(value);
    if ((explicitFile.isAbsolute() || value.contains('/') || value.contains('\\'))
        && explicitFile.exists() && explicitFile.isFile()) {
        return explicitFile.absoluteFilePath();
    }
    return QStandardPaths::findExecutable(value);
}

bool WeComMeetingMonitor::start()
{
    if (isRunning()) return true;
    setRunning(true);
    if (m_enabled) {
        m_syncTimer->start(m_syncIntervalMinutes * 60 * 1000);
        QTimer::singleShot(0, this, &WeComMeetingMonitor::syncNow);
    }
    return true;
}

bool WeComMeetingMonitor::parseAuthorizationResponse(
    const QByteArray &output, bool *authorized, QString *botId,
    QString *errorMessage)
{
    if (!authorized) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Missing authorization result target.");
        return false;
    }

    *authorized = false;
    if (botId) botId->clear();

    QString text = QString::fromUtf8(output).trimmed();
    // Be tolerant of terminal color sequences even though redirected output
    // normally disables them.
    text.remove(QRegularExpression(
        QStringLiteral("\\x1B\\[[0-?]*[ -/]*[@-~]")));

    const QJsonDocument json = QJsonDocument::fromJson(text.toUtf8());
    if (json.isObject()) {
        const QJsonObject object = json.object();
        const QString status = object.value("status").toString().trimmed();
        if (status.compare("authorized", Qt::CaseInsensitive) == 0
            || status.compare("unauthorized", Qt::CaseInsensitive) == 0) {
            *authorized = status.compare("authorized", Qt::CaseInsensitive) == 0;
            if (botId) {
                *botId = object.value("bot_id").toString(
                    object.value("botId").toString()).trimmed();
            }
            return true;
        }
    }

    const QRegularExpression statusPattern(
        QStringLiteral("(?:^|\\n)\\s*Status\\s*:\\s*(authorized|unauthorized)\\s*(?:$|\\n)"),
        QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch statusMatch = statusPattern.match(text);
    QString status;
    if (statusMatch.hasMatch()) {
        status = statusMatch.captured(1);
    } else if (text.compare("authorized", Qt::CaseInsensitive) == 0
               || text.compare("unauthorized", Qt::CaseInsensitive) == 0) {
        status = text;
    }

    if (status.isEmpty()) {
        if (errorMessage)
            *errorMessage = QStringLiteral(
                "wecom-cli returned an unrecognized authorization status.");
        return false;
    }

    *authorized = status.compare("authorized", Qt::CaseInsensitive) == 0;
    if (botId) {
        const QRegularExpression botPattern(
            QStringLiteral("(?:^|\\n)\\s*Bot\\s*ID\\s*:\\s*([^\\r\\n]+)"),
            QRegularExpression::CaseInsensitiveOption);
        const QRegularExpressionMatch botMatch = botPattern.match(text);
        if (botMatch.hasMatch()) *botId = botMatch.captured(1).trimmed();
    }
    return true;
}

void WeComMeetingMonitor::stop()
{
    m_syncTimer->stop();
    m_timeoutTimer->stop();
    m_syncing = false;
    m_commandActive = false;
    if (m_process->state() != QProcess::NotRunning) {
        m_process->kill();
        m_process->waitForFinished(1000);
    }
    setRunning(false);
}

void WeComMeetingMonitor::syncNow()
{
    if (!m_enabled) return;
    if (m_syncing) return;
    m_resolvedCliPath = resolveCliPath(m_cliPath);
    if (m_resolvedCliPath.isEmpty()) {
        failSync(QStringLiteral(
            "wecom-cli was not found. Install @wecom/cli and complete 'wecom-cli auth init'."));
        return;
    }

    m_syncing = true;
    m_pendingMeetingIds.clear();
    m_nextDetailsIndex = 0;
    m_attendedMeetings = 0;
    emit syncStarted();
    requestList();
}

void WeComMeetingMonitor::requestList(const QString &cursor)
{
    const QDate today = QDate::currentDate();
    QJsonObject request{
        {"begin_time", QDateTime(today.addDays(-7), QTime(0, 0), Qt::LocalTime)
                           .toString("yyyy-MM-dd HH:mm:ss")},
        {"end_time", QDateTime(today, QTime(23, 59, 59), Qt::LocalTime)
                         .toString("yyyy-MM-dd HH:mm:ss")},
        {"limit", 100},
    };
    if (!cursor.isEmpty()) request["cursor"] = cursor;
    runCli(RequestKind::List,
           {"meeting", "list", "--json",
            QString::fromUtf8(QJsonDocument(request).toJson(QJsonDocument::Compact))});
}

void WeComMeetingMonitor::requestNextDetailsBatch()
{
    if (m_nextDetailsIndex >= m_pendingMeetingIds.size()) {
        finishSync();
        return;
    }

    QJsonArray ids;
    const int end = qMin(m_nextDetailsIndex + 10, m_pendingMeetingIds.size());
    while (m_nextDetailsIndex < end)
        ids.append(m_pendingMeetingIds.at(m_nextDetailsIndex++));
    const QJsonObject request{{"meeting_ids", ids}};
    runCli(RequestKind::Details,
           {"meeting", "get", "--json",
            QString::fromUtf8(QJsonDocument(request).toJson(QJsonDocument::Compact))});
}

void WeComMeetingMonitor::runCli(RequestKind kind,
                                 const QStringList &arguments)
{
    if (m_process->state() != QProcess::NotRunning) {
        failSync(QStringLiteral("A previous wecom-cli command is still running."));
        return;
    }

    QString program = m_resolvedCliPath;
    QStringList processArguments = arguments;
#ifdef Q_OS_WIN
    if (program.endsWith(".cmd", Qt::CaseInsensitive)
        || program.endsWith(".bat", Qt::CaseInsensitive)) {
        processArguments.prepend(program);
        processArguments.prepend(QStringLiteral("/c"));
        processArguments.prepend(QStringLiteral("/d"));
        program = qEnvironmentVariable("COMSPEC", QStringLiteral("cmd.exe"));
    }
#endif

    m_requestKind = kind;
    m_commandActive = true;
    m_process->setProgram(program);
    m_process->setArguments(processArguments);
    m_process->start();
    m_timeoutTimer->start(30000);
}

void WeComMeetingMonitor::handleProcessFinished(
    int exitCode, QProcess::ExitStatus exitStatus)
{
    if (!m_commandActive) return;
    m_commandActive = false;
    m_timeoutTimer->stop();

    const QByteArray output = m_process->readAllStandardOutput();
    const QString standardError = QString::fromUtf8(
        m_process->readAllStandardError()).trimmed();
    if (exitStatus != QProcess::NormalExit || exitCode != 0) {
        failSync(standardError.isEmpty()
            ? QString("wecom-cli exited with code %1.").arg(exitCode)
            : standardError);
        return;
    }

    QString parseError;
    if (m_requestKind == RequestKind::List) {
        WeComMeetingListPage page;
        if (!parseListResponse(output, &page, &parseError)) {
            failSync(parseError);
            return;
        }
        QSet<QString> knownIds;
        for (const QJsonObject &known : std::as_const(m_pendingMeetingIds)) {
            knownIds.insert(known.value("meeting_id").toString() + "|"
                            + known.value("sub_meeting_id").toString());
        }
        for (const QJsonObject &id : page.meetingIds) {
            const QString key = id.value("meeting_id").toString() + "|"
                + id.value("sub_meeting_id").toString();
            if (!key.startsWith('|') && !knownIds.contains(key)) {
                knownIds.insert(key);
                m_pendingMeetingIds.append(id);
            }
        }
        if (page.hasMore && !page.nextCursor.isEmpty())
            requestList(page.nextCursor);
        else
            requestNextDetailsBatch();
        return;
    }

    if (m_requestKind == RequestKind::Details) {
        QList<WeComMeetingDetails> meetings;
        if (!parseDetailsResponse(output, &meetings, &parseError)) {
            failSync(parseError);
            return;
        }
        for (const WeComMeetingDetails &meeting : meetings) {
            emit rawEventCaptured(toRawEvent(meeting));
            ++m_attendedMeetings;
        }
        requestNextDetailsBatch();
    }
}

void WeComMeetingMonitor::failSync(const QString &message)
{
    const bool wasSyncing = m_syncing;
    m_syncing = false;
    m_commandActive = false;
    m_requestKind = RequestKind::None;
    m_timeoutTimer->stop();
    spdlog::warn("WeCom meeting sync failed: {}", message.toStdString());
    emit monitorError(name(), message);
    Q_UNUSED(wasSyncing);
    emit syncFailed(message);
}

void WeComMeetingMonitor::finishSync()
{
    const int count = m_attendedMeetings;
    m_syncing = false;
    m_requestKind = RequestKind::None;
    spdlog::info("WeCom meeting sync found {} attended meetings.", count);
    emit syncFinished(count);
}

RawEvent WeComMeetingMonitor::toRawEvent(
    const WeComMeetingDetails &meeting) const
{
    RawEvent event;
    event.timestamp = meeting.enteredAt;
    event.type = EventType::MeetingAttended;
    event.source = name();
    event.description = QString("WeCom meeting: %1").arg(
        meeting.subject.isEmpty() ? QStringLiteral("Untitled meeting")
                                  : meeting.subject);
    const QString externalId = QString("wecom:%1:%2:%3")
        .arg(meeting.meetingId, meeting.subMeetingId,
             meeting.enteredAt.toString(Qt::ISODateWithMs));
    event.metadata["externalId"] = externalId;
    event.metadata["meetingId"] = meeting.meetingId;
    event.metadata["subMeetingId"] = meeting.subMeetingId;
    event.metadata["subject"] = meeting.subject;
    event.metadata["endTime"] = meeting.quitAt.toString(Qt::ISODateWithMs);
    event.metadata["durationSecs"] = static_cast<int>(
        meeting.enteredAt.secsTo(meeting.quitAt));
    event.metadata["idleThresholdPercent"] = m_idleThresholdPercent;
    event.metadata["actualAttendance"] = true;
    event.metadata["integration"] = QStringLiteral("wecom-cli");
    return event;
}

bool WeComMeetingMonitor::parseListResponse(
    const QByteArray &output, WeComMeetingListPage *page,
    QString *errorMessage)
{
    if (!page) return false;
    *page = {};
    const QJsonObject payload = parseObject(
        output, {"created_meetings", "attended_meetings", "has_more"},
        errorMessage);
    if (payload.isEmpty()) return false;

    for (const QString &arrayName : {QStringLiteral("created_meetings"),
                                     QStringLiteral("attended_meetings")}) {
        for (const QJsonValue &value : payload.value(arrayName).toArray()) {
            const QJsonObject meeting = value.toObject();
            const QString meetingId = meeting.value("meeting_id").toString();
            if (meetingId.isEmpty()) continue;
            QJsonObject id{{"meeting_id", meetingId}};
            const QString subMeetingId = meeting.value("sub_meeting_id").toString();
            if (!subMeetingId.isEmpty()) id["sub_meeting_id"] = subMeetingId;
            page->meetingIds.append(id);
        }
    }
    page->hasMore = payload.value("has_more").toBool(false);
    page->nextCursor = payload.value("next_cursor").toString();
    return true;
}

bool WeComMeetingMonitor::parseDetailsResponse(
    const QByteArray &output, QList<WeComMeetingDetails> *meetings,
    QString *errorMessage)
{
    if (!meetings) return false;
    meetings->clear();
    const QJsonObject payload = parseObject(output, {"meetings"}, errorMessage);
    if (payload.isEmpty()) return false;

    for (const QJsonValue &value : payload.value("meetings").toArray()) {
        const QJsonObject object = value.toObject();
        const QString timeZoneId = object.value("timezone")
                                       .toObject().value("timezone_id").toString();
        WeComMeetingDetails meeting;
        meeting.meetingId = object.value("meeting_id").toString();
        meeting.subMeetingId = object.value("sub_meeting_id").toString();
        meeting.subject = object.value("subject").toString();
        meeting.enteredAt = parseMeetingTime(
            object.value("current_user_enter_time").toString(), timeZoneId);
        meeting.quitAt = parseMeetingTime(
            object.value("current_user_quit_time").toString(), timeZoneId);

        // An invitation or reservation is never enough. Only actual,
        // completed attendance with both timestamps becomes a candidate.
        if (meeting.meetingId.isEmpty() || !meeting.enteredAt.isValid()
            || !meeting.quitAt.isValid()
            || meeting.enteredAt >= meeting.quitAt) {
            continue;
        }
        meetings->append(meeting);
    }
    return true;
}
