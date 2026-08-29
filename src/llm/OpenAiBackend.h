#pragma once

#include "ILlmBackend.h"
#include <QNetworkAccessManager>
#include <QNetworkReply>

/// OpenAI Chat Completions API backend with streaming support.
class OpenAiBackend : public ILlmBackend {
    Q_OBJECT

public:
    explicit OpenAiBackend(QObject *parent = nullptr);

    QString name() const override { return "OpenAI"; }
    QString model() const override { return m_config.model; }
    void configure(const LlmConfig &config) override;
    QFuture<bool> isAvailable() override;
    QFuture<QString> generate(const QString &systemPrompt,
                               const QString &userPrompt) override;
    void cancel() override;

private:
    QNetworkAccessManager *m_nam = nullptr;
    LlmConfig m_config;
    QNetworkReply *m_activeReply = nullptr;

    QString buildRequestBody(const QString &systemPrompt, const QString &userPrompt) const;
};
