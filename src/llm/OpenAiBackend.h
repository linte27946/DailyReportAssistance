#pragma once

#include "ILlmBackend.h"
#include <QNetworkAccessManager>
#include <QNetworkReply>

/// OpenAI Chat Completions API backend with streaming support.
class OpenAiBackend : public ILlmBackend {
    Q_OBJECT

public:
    explicit OpenAiBackend(QObject *parent = nullptr);

    QString name() const override { return m_backendName; }
    QString model() const override { return m_config.model; }
    void configure(const LlmConfig &config) override;
    QFuture<bool> isAvailable() override;
    QFuture<QString> generate(const QString &systemPrompt,
                               const QString &userPrompt) override;
    void cancel() override;

protected:
    OpenAiBackend(const QString &backendName,
                  const LlmConfig &defaultConfig,
                  QObject *parent = nullptr);

private:
    QNetworkAccessManager *m_nam = nullptr;
    LlmConfig m_config;
    QNetworkReply *m_activeReply = nullptr;
    QString m_backendName;
    LlmConfig m_defaultConfig;

    QString buildRequestBody(const QString &systemPrompt, const QString &userPrompt) const;
};
