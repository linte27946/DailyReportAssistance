#include "AnthropicBackend.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFutureInterface>
#include <memory>
#include <spdlog/spdlog.h>

AnthropicBackend::AnthropicBackend(QObject *parent)
    : ILlmBackend(parent)
    , m_nam(new QNetworkAccessManager(this))
{
    m_config = LlmConfig::anthropicDefault();
}

void AnthropicBackend::configure(const LlmConfig &config)
{
    m_config = config;
    m_config.endpoint = config.endpoint.isEmpty()
        ? LlmConfig::anthropicDefault().endpoint
        : config.endpoint;
    spdlog::info("Anthropic backend configured: endpoint={}, model={}",
                 m_config.endpoint.toStdString(), m_config.model.toStdString());
}

void AnthropicBackend::cancel()
{
    if (m_activeReply) m_activeReply->abort();
}

QFuture<bool> AnthropicBackend::isAvailable()
{
    QFutureInterface<bool> fi;
    fi.reportStarted();

    QNetworkRequest request(QUrl(m_config.endpoint));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("x-api-key", m_config.apiKey.toUtf8());
    request.setRawHeader("anthropic-version", "2023-06-01");

    QJsonObject body;
    body["model"] = m_config.model;
    body["max_tokens"] = 1;
    body["messages"] = QJsonArray{
        QJsonObject{{"role", "user"}, {"content", "ping"}}
    };

    QNetworkReply *reply = m_nam->post(request,
        QJsonDocument(body).toJson(QJsonDocument::Compact));

    QObject::connect(reply, &QNetworkReply::finished, [reply, fi]() mutable {
        bool available = (reply->error() == QNetworkReply::NoError ||
                          reply->error() == QNetworkReply::AuthenticationRequiredError);
        fi.reportResult(available);
        fi.reportFinished();
        reply->deleteLater();
    });

    return fi.future();
}

QFuture<QString> AnthropicBackend::generate(const QString &systemPrompt,
                                             const QString &userPrompt)
{
    auto *fi = new QFutureInterface<QString>();
    fi->reportStarted();

    QNetworkRequest request(QUrl(m_config.endpoint));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("x-api-key", m_config.apiKey.toUtf8());
    request.setRawHeader("anthropic-version", "2023-06-01");
    request.setTransferTimeout(m_config.timeoutSecs * 1000);

    QByteArray body = buildRequestBody(systemPrompt, userPrompt);
    QNetworkReply *reply = m_nam->post(request, body);
    m_activeReply = reply;
    auto accumulatedText = std::make_shared<QString>();
    auto pendingData = std::make_shared<QByteArray>();

    auto consumeStream = [this, accumulatedText, pendingData](const QByteArray &chunk) {
        pendingData->append(chunk);
        while (true) {
            const qsizetype newline = pendingData->indexOf('\n');
            if (newline < 0) break;
            const QByteArray line = pendingData->left(newline).trimmed();
            pendingData->remove(0, newline + 1);
            if (!line.startsWith("data: ")) continue;
            const QJsonObject object = QJsonDocument::fromJson(line.mid(6).trimmed()).object();
            if (object["type"].toString() != "content_block_delta") continue;
            const QString token = object["delta"].toObject()["text"].toString();
            if (!token.isEmpty()) {
                *accumulatedText += token;
                emit streamingToken(token);
            }
        }
    };

    connect(reply, &QNetworkReply::finished, [this, reply, fi, accumulatedText, consumeStream]() {
        consumeStream(reply->readAll());
        reply->deleteLater();
        m_activeReply = nullptr;

        if (reply->error() != QNetworkReply::NoError) {
            QString error = QString("Anthropic API error: %1").arg(reply->errorString());
            spdlog::error(error.toStdString());
            emit generationError(error);
            fi->reportResult("Error: " + error);
            fi->reportFinished();
            delete fi;
            return;
        }

        if (!accumulatedText->isEmpty()) {
            emit generationComplete(*accumulatedText);
            fi->reportResult(*accumulatedText);
        } else {
            emit generationError("Anthropic returned empty response.");
            fi->reportResult("Error: Anthropic returned an empty response.");
        }

        fi->reportFinished();
        delete fi;
    });

    // SSE streaming
    connect(reply, &QNetworkReply::readyRead, [reply, consumeStream]() {
        consumeStream(reply->readAll());
    });

    return fi->future();
}

QByteArray AnthropicBackend::buildRequestBody(const QString &systemPrompt,
                                               const QString &userPrompt) const
{
    QJsonObject body;
    body["model"] = m_config.model;
    body["max_tokens"] = m_config.maxTokens;
    body["temperature"] = m_config.temperature;
    body["stream"] = true;

    // System prompt as top-level field
    if (!systemPrompt.isEmpty()) {
        body["system"] = systemPrompt;
    }

    // Messages array
    body["messages"] = QJsonArray{
        QJsonObject{{"role", "user"}, {"content", userPrompt}}
    };

    return QJsonDocument(body).toJson(QJsonDocument::Compact);
}
