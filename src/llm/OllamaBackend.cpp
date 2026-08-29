#include "OllamaBackend.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QFutureInterface>
#include <spdlog/spdlog.h>

OllamaBackend::OllamaBackend(QObject *parent)
    : ILlmBackend(parent)
    , m_nam(new QNetworkAccessManager(this))
{
    m_config = LlmConfig::ollamaDefault();
}

void OllamaBackend::configure(const LlmConfig &config)
{
    m_config = config;
    m_config.endpoint = config.endpoint.isEmpty()
        ? LlmConfig::ollamaDefault().endpoint
        : config.endpoint;
    spdlog::info("Ollama backend configured: endpoint={}, model={}",
                 m_config.endpoint.toStdString(), m_config.model.toStdString());
}

void OllamaBackend::cancel()
{
    if (m_activeReply) m_activeReply->abort();
}

QFuture<bool> OllamaBackend::isAvailable()
{
    QFutureInterface<bool> fi;
    fi.reportStarted();

    // Check Ollama health (try listing models)
    QUrl healthUrl(m_config.endpoint);
    healthUrl.setPath("/api/tags");  // List available models

    QNetworkRequest request(healthUrl);
    QNetworkReply *reply = m_nam->get(request);

    QObject::connect(reply, &QNetworkReply::finished, [reply, fi]() mutable {
        bool available = (reply->error() == QNetworkReply::NoError);
        fi.reportResult(available);
        fi.reportFinished();
        reply->deleteLater();
    });

    return fi.future();
}

QFuture<QString> OllamaBackend::generate(const QString &systemPrompt,
                                          const QString &userPrompt)
{
    auto *fi = new QFutureInterface<QString>();
    fi->reportStarted();

    QUrl endpoint(m_config.endpoint);
    if (endpoint.path().isEmpty() || !endpoint.path().contains("generate")) {
        endpoint.setPath("/api/generate");
    }

    QNetworkRequest request(endpoint);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setTransferTimeout(m_config.timeoutSecs * 1000);

    // Combine system + user prompt (Ollama doesn't have a dedicated system role)
    QString combinedPrompt = systemPrompt.isEmpty()
        ? userPrompt
        : QString("System: %1\n\nUser: %2").arg(systemPrompt, userPrompt);

    QByteArray body = buildRequestBody(systemPrompt, userPrompt);
    QNetworkReply *reply = m_nam->post(request, body);
    m_activeReply = reply;

    QString *accumulatedText = new QString();

    connect(reply, &QNetworkReply::readyRead, [this, reply, accumulatedText]() {
        // Ollama streams one JSON object per line
        while (reply->canReadLine()) {
            QByteArray line = reply->readLine().trimmed();
            if (line.isEmpty()) continue;

            QJsonDocument d = QJsonDocument::fromJson(line);
            if (!d.isObject()) continue;

            QJsonObject o = d.object();
            QString token = o["response"].toString();
            if (!token.isEmpty()) {
                *accumulatedText += token;
                emit streamingToken(token);
            }
        }
    });

    connect(reply, &QNetworkReply::finished, [this, reply, fi, accumulatedText]() {
        reply->deleteLater();
        m_activeReply = nullptr;

        if (reply->error() != QNetworkReply::NoError) {
            QString error = QString("Ollama API error: %1").arg(reply->errorString());
            spdlog::error(error.toStdString());
            emit generationError(error);
            fi->reportResult("Error: " + error);
        } else if (!accumulatedText->isEmpty()) {
            emit generationComplete(*accumulatedText);
            fi->reportResult(*accumulatedText);
        } else {
            // Non-streaming response fallback
            QByteArray data = reply->readAll();
            QJsonDocument d = QJsonDocument::fromJson(data);
            QString text = d.object()["response"].toString();
            if (!text.isEmpty()) {
                emit generationComplete(text);
                fi->reportResult(text);
            } else {
                emit generationError("Ollama returned empty response.");
                fi->reportResult("Error: Ollama returned an empty response.");
            }
        }

        delete accumulatedText;
        fi->reportFinished();
        delete fi;
    });

    return fi->future();
}

QByteArray OllamaBackend::buildRequestBody(const QString &systemPrompt,
                                            const QString &userPrompt) const
{
    QJsonObject body;
    body["model"] = m_config.model;
    body["stream"] = true;
    body["options"] = QJsonObject{
        {"temperature", m_config.temperature},
        {"num_predict", m_config.maxTokens},
    };

    // Combine prompts for Ollama
    QString prompt;
    if (!systemPrompt.isEmpty()) {
        prompt = QString("System: %1\n\nUser: %2").arg(systemPrompt, userPrompt);
    } else {
        prompt = userPrompt;
    }
    body["prompt"] = prompt;

    return QJsonDocument(body).toJson(QJsonDocument::Compact);
}
