#pragma once

#include "ILlmBackend.h"
#include <QNetworkAccessManager>
#include <QNetworkReply>

/// Ollama local LLM backend (/api/generate).
class OllamaBackend : public ILlmBackend {
    Q_OBJECT

public:
    explicit OllamaBackend(QObject *parent = nullptr);

    QString name() const override { return "Ollama"; }
    void configure(const LlmConfig &config) override;
    QFuture<bool> isAvailable() override;
    QFuture<QString> generate(const QString &systemPrompt,
                               const QString &userPrompt) override;

private:
    QNetworkAccessManager *m_nam = nullptr;
    LlmConfig m_config;
    QNetworkReply *m_activeReply = nullptr;
    QByteArray buildRequestBody(const QString &systemPrompt, const QString &userPrompt) const;
};
