#pragma once

#include "ILlmBackend.h"
#include <QNetworkAccessManager>
#include <QNetworkReply>

/// Anthropic Claude Messages API backend with streaming support.
class AnthropicBackend : public ILlmBackend {
    Q_OBJECT

public:
    explicit AnthropicBackend(QObject *parent = nullptr);

    QString name() const override { return "Anthropic"; }
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

    QByteArray buildRequestBody(const QString &systemPrompt, const QString &userPrompt) const;
};
