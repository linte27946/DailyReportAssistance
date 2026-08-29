#pragma once

#include "IMonitor.h"

/// Converts foreground-window changes into developer-oriented context events.
/// It records editor file/workspace names and reference-document names, but
/// never reads source code or document contents.
class WorkContextMonitor : public IMonitor {
    Q_OBJECT

public:
    explicit WorkContextMonitor(QObject *parent = nullptr);

    bool start() override;
    void stop() override;
    QString name() const override { return "WorkContextMonitor"; }

    void setTrackEditors(bool enabled) { m_trackEditors = enabled; }
    void setTrackDocuments(bool enabled) { m_trackDocuments = enabled; }

public slots:
    void processWindowEvent(const RawEvent &event);

private:
    static bool isEditor(const QString &processName, const QString &title);
    static bool isDocumentApplication(const QString &processName);
    static QString editorName(const QString &processName, const QString &title);
    static QString documentKind(const QString &documentName);
    static QString cleanTitlePart(QString value);

    QString m_lastContextKey;
    bool m_trackEditors = true;
    bool m_trackDocuments = true;
};
