#include "LlmClient.h"
#include <spdlog/spdlog.h>

LlmClient::LlmClient(QObject *parent)
    : QObject(parent)
{
}

LlmClient::~LlmClient()
{
    m_backends.clear();
}

void LlmClient::registerBackend(std::unique_ptr<ILlmBackend> backend)
{
    if (!backend) {
        spdlog::error("LlmClient: Attempted to register null backend.");
        return;
    }

    QString name = backend->name();
    spdlog::info("LlmClient: Registering backend: {}", name.toStdString());

    // Forward signals from the backend
    connect(backend.get(), &ILlmBackend::streamingToken,
            this, &LlmClient::streamingToken, Qt::QueuedConnection);
    connect(backend.get(), &ILlmBackend::generationComplete,
            this, &LlmClient::generationComplete, Qt::QueuedConnection);
    connect(backend.get(), &ILlmBackend::generationError,
            this, &LlmClient::generationError, Qt::QueuedConnection);

    m_backends[name] = std::move(backend);

}

bool LlmClient::setActiveBackend(const QString &name)
{
    if (!m_backends.contains(name)) {
        spdlog::error("LlmClient: Backend not found: {}", name.toStdString());
        return false;
    }
    m_activeName = name;
    spdlog::info("LlmClient: Active backend set to: {}", name.toStdString());
    return true;
}

QStringList LlmClient::availableBackends() const
{
    QStringList result;
    for (const auto &pair : m_backends) {
        result.append(pair.first);
    }
    return result;
}

bool LlmClient::configureBackend(const QString &name, const LlmConfig &config)
{
    if (!m_backends.contains(name)) return false;
    m_backends[name]->configure(config);
    return true;
}

QFuture<QString> LlmClient::generateReport(const QString &systemPrompt,
                                             const QString &userPrompt)
{
    if (!m_backends.contains(m_activeName)) {
        QFutureInterface<QString> fi;
        fi.reportStarted();
        fi.reportResult("Error: No active LLM backend is configured.");
        fi.reportFinished();
        spdlog::error("LlmClient: No active backend.");
        return fi.future();
    }

    spdlog::info("LlmClient: Generating report with backend: {}",
                 m_activeName.toStdString());
    return m_backends[m_activeName]->generate(systemPrompt, userPrompt);
}

void LlmClient::cancelActiveGeneration()
{
    ILlmBackend *backend = activeBackendPtr();
    if (backend) backend->cancel();
}

bool LlmClient::hasAvailableBackend() const
{
    return !m_backends.empty() && m_backends.contains(m_activeName);
}

ILlmBackend *LlmClient::activeBackendPtr() const
{
    auto it = m_backends.find(m_activeName);
    if (it != m_backends.end())
        return it->second.get();
    return nullptr;
}
