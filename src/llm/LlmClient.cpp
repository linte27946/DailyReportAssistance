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

    // Set as active if it's the first one
    if (m_activeName.isEmpty()) {
        m_activeName = name;
    }
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
    return m_backends.keys();
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
        fi.reportFinished();
        spdlog::error("LlmClient: No active backend.");
        return fi.future();
    }

    spdlog::info("LlmClient: Generating report with backend: {}",
                 m_activeName.toStdString());
    return m_backends[m_activeName]->generate(systemPrompt, userPrompt);
}

bool LlmClient::hasAvailableBackend() const
{
    return !m_backends.isEmpty() && m_backends.contains(m_activeName);
}

ILlmBackend *LlmClient::activeBackendPtr() const
{
    if (m_backends.contains(m_activeName))
        return m_backends[m_activeName].get();
    return nullptr;
}
