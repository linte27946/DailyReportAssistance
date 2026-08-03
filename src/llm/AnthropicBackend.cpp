#include "AnthropicBackend.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFutureInterface>
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

    connect(reply, &QNetworkReply::finished, [this, reply, fi]() {
        reply->deleteLater();
        m_activeReply = nullptr;

        if (reply->error() != QNetworkReply::NoError) {
            QString error = QString("Anthropic API error: %1").arg(reply->errorString());
            spdlog::error(error.toStdString());
            emit generationError(error);
            fi->reportResult(QString());
            fi->reportFinished();
            delete fi;
            return;
        }

        QByteArray responseData = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(responseData);
        QJsonObject obj = doc.object();

        // Parse Anthropic Messages response
        QJsonArray content = obj["content"].toArray();
        QString text;
        for (const auto &v : content) {
            QJsonObject block = v.toObject();
            if (block["type"].toString() == "text") {
                text += block["text"].toString();
            }
        }

        if (!text.isEmpty()) {
            emit generationComplete(text);
            fi->reportResult(text);
        } else {
            emit generationError("Anthropic returned empty response.");
            fi->reportResult(QString());
        }

        fi->reportFinished();
        delete fi;
    });

    // SSE streaming
    connect(reply, &QNetworkReply::readyRead, [this, reply]() {
        QByteArray chunk = reply->readAll();
        QString text = QString::fromUtf8(chunk);

        for (const auto &line : text.split('\n')) {
            if (line.startsWith("data: ")) {
                QString data = line.mid(6).trimmed();
                QJsonDocument d = QJsonDocument::fromJson(data.toUtf8());
                QJsonObject o = d.object();

                if (o["type"].toString() == "content_block_delta") {
                    QJsonObject delta = o["delta"].toObject();
                    QString token = delta["text"].toString();
                    if (!token.isEmpty()) {
                        emit streamingToken(token);
                    }
                }
            }
        }
    });

    return fi->future();
}

QByteArray AnthropicBackend::buildRequestBody(const QString &systemPrompt,
                                               const QString &userPrompt) const
{
    QJsonObject body;
    body["model"] = m_config.model;
    body["max_tokens"] = m_config.maxTokens;
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
