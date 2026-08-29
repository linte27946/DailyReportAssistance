#pragma once

#include <QObject>
#include <map>
#include <QString>
#include <QFuture>
#include <memory>
#include "ILlmBackend.h"
#include "LlmConfig.h"

/// Facade for multiple LLM backends. Routes requests to the active backend.
class LlmClient : public QObject {
    Q_OBJECT

public:
    explicit LlmClient(QObject *parent = nullptr);
    ~LlmClient();

    /// Register a backend implementation. Takes ownership.
    void registerBackend(std::unique_ptr<ILlmBackend> backend);

    /// Set the active backend by name.
    bool setActiveBackend(const QString &name);

    void clearActiveBackend() { m_activeName.clear(); }

    /// Get the active backend name.
    QString activeBackend() const { return m_activeName; }

    /// List registered backend names.
    QStringList availableBackends() const;

    /// Configure a specific backend.
    bool configureBackend(const QString &name, const LlmConfig &config);

    /// Generate a report using the active backend.
    QFuture<QString> generateReport(const QString &systemPrompt,
                                     const QString &userPrompt);

    void cancelActiveGeneration();

    /// Check if any backend is available.
    bool hasAvailableBackend() const;

    /// Get the active backend pointer.
    ILlmBackend *activeBackendPtr() const;

signals:
    void streamingToken(const QString &token);
    void generationComplete(const QString &fullText);
    void generationError(const QString &errorMessage);

private:
    std::map<QString, std::unique_ptr<ILlmBackend>> m_backends;
    QString m_activeName;
};
