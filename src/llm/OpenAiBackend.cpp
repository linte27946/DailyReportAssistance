#include "OpenAiBackend.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFutureInterface>
#include <QEventLoop>
#include <QTimer>
#include <memory>
#include <spdlog/spdlog.h>

OpenAiBackend::OpenAiBackend(QObject *parent)
    : ILlmBackend(parent)
    , m_nam(new QNetworkAccessManager(this))
{
    m_config = LlmConfig::openAiDefault();
}

void OpenAiBackend::configure(const LlmConfig &config)
{
    m_config = config;
    m_config.endpoint = config.endpoint.isEmpty()
        ? LlmConfig::openAiDefault().endpoint
        : config.endpoint;
    spdlog::info("OpenAI backend configured: endpoint={}, model={}",
                 m_config.endpoint.toStdString(), m_config.model.toStdString());
}

void OpenAiBackend::cancel()
{
    if (m_activeReply) m_activeReply->abort();
}

QFuture<bool> OpenAiBackend::isAvailable()
{
    QFutureInterface<bool> fi;
    fi.reportStarted();

    // Simple connectivity check: send a lightweight request
    QNetworkRequest request(QUrl(m_config.endpoint));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("Authorization", ("Bearer " + m_config.apiKey).toUtf8());

    QJsonObject body;
    body["model"] = m_config.model;
    body["messages"] = QJsonArray{
        QJsonObject{{"role", "system"}, {"content", "ping"}},
        QJsonObject{{"role", "user"}, {"content", "pong"}},
    };
    body["max_tokens"] = 1;

    QNetworkReply *reply = m_nam->post(request,
        QJsonDocument(body).toJson(QJsonDocument::Compact));

    QObject::connect(reply, &QNetworkReply::finished, [reply, fi]() mutable {
        bool available = (reply->error() == QNetworkReply::NoError ||
                          reply->error() == QNetworkReply::AuthenticationRequiredError);
        // AuthenticationRequiredError means the endpoint is reachable but key is wrong
        fi.reportResult(available);
        fi.reportFinished();
        reply->deleteLater();
    });

    return fi.future();
}

QFuture<QString> OpenAiBackend::generate(const QString &systemPrompt,
                                          const QString &userPrompt)
{
    auto *fi = new QFutureInterface<QString>();
    fi->reportStarted();

    QNetworkRequest request(QUrl(m_config.endpoint));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("Authorization", ("Bearer " + m_config.apiKey).toUtf8());
    request.setTransferTimeout(m_config.timeoutSecs * 1000);

    QByteArray body = buildRequestBody(systemPrompt, userPrompt).toUtf8();
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
            const QByteArray data = line.mid(6).trimmed();
            if (data == "[DONE]") continue;

            const QJsonObject object = QJsonDocument::fromJson(data).object();
            const QJsonArray choices = object["choices"].toArray();
            if (choices.isEmpty()) continue;
            const QString token = choices.first().toObject()["delta"].toObject()["content"].toString();
            if (!token.isEmpty()) {
                *accumulatedText += token;
                emit streamingToken(token);
            }
        }
    };

    // Handle response
    connect(reply, &QNetworkReply::finished, [this, reply, fi, accumulatedText, pendingData, consumeStream]() {
        consumeStream(reply->readAll());
        reply->deleteLater();
        m_activeReply = nullptr;

        if (reply->error() != QNetworkReply::NoError) {
            QString error = QString("OpenAI API error: %1").arg(reply->errorString());
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
            emit generationError("OpenAI returned empty response.");
            fi->reportResult("Error: OpenAI returned an empty response.");
        }

        fi->reportFinished();
        delete fi;
    });

    // Handle streaming (SSE) if supported
    connect(reply, &QNetworkReply::readyRead, [reply, consumeStream]() {
        consumeStream(reply->readAll());
    });

    return fi->future();
}

QString OpenAiBackend::buildRequestBody(const QString &systemPrompt,
                                          const QString &userPrompt) const
{
    QJsonObject body;
    body["model"] = m_config.model;
    body["messages"] = QJsonArray{
        QJsonObject{{"role", "system"}, {"content", systemPrompt}},
        QJsonObject{{"role", "user"}, {"content", userPrompt}},
    };
    body["temperature"] = m_config.temperature;
    body["max_tokens"] = m_config.maxTokens;
    body["stream"] = true;

    return QString::fromUtf8(QJsonDocument(body).toJson(QJsonDocument::Compact));
}
