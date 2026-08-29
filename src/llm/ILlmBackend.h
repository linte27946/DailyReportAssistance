#pragma once

#include <QObject>
#include <QString>
#include <QFuture>
#include <QFutureInterface>
#include "LlmConfig.h"

/// Abstract interface for LLM backend implementations.
class ILlmBackend : public QObject {
    Q_OBJECT

public:
    explicit ILlmBackend(QObject *parent = nullptr) : QObject(parent) {}
    virtual ~ILlmBackend() = default;

    /// Human-readable backend name (e.g., "OpenAI", "Anthropic", "Ollama").
    virtual QString name() const = 0;

    /// Configured model identifier, used in report history and diagnostics.
    virtual QString model() const = 0;

    /// Configure this backend.
    virtual void configure(const LlmConfig &config) = 0;

    /// Check if the backend is available (can reach the endpoint).
    virtual QFuture<bool> isAvailable() = 0;

    /// Generate text from a prompt. Returns the generated text asynchronously.
    virtual QFuture<QString> generate(const QString &systemPrompt,
                                       const QString &userPrompt) = 0;

    /// Abort an in-flight request during application shutdown.
    virtual void cancel() = 0;

signals:
    /// Emitted for each token during streaming generation.
    void streamingToken(const QString &token);

    /// Emitted when generation completes successfully.
    void generationComplete(const QString &fullText);

    /// Emitted when generation fails.
    void generationError(const QString &errorMessage);

    /// Emitted to indicate generation progress (tokens or rough estimate).
    void generationProgress(int percent);
};
