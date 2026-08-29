#pragma once

#include <QString>
#include <QJsonObject>

/// Configuration for an LLM backend.
struct LlmConfig {
    QString endpoint;        // API endpoint URL
    QString apiKey;          // API key (stored encrypted at rest)
    QString model;           // Model name (e.g., "gpt-4o", "claude-sonnet-4-20250514")
    double temperature = 0.7;
    int maxTokens = 4096;
    int timeoutSecs = 120;
    QString systemPrompt;    // Custom system prompt override

    QJsonObject toJson() const
    {
        QJsonObject obj;
        obj["endpoint"] = endpoint;
        // Note: apiKey is NOT serialized to JSON for security
        obj["model"] = model;
        obj["temperature"] = temperature;
        obj["maxTokens"] = maxTokens;
        obj["timeoutSecs"] = timeoutSecs;
        obj["systemPrompt"] = systemPrompt;
        return obj;
    }

    static LlmConfig fromJson(const QJsonObject &obj)
    {
        LlmConfig cfg;
        cfg.endpoint = obj["endpoint"].toString();
        cfg.model = obj["model"].toString();
        cfg.temperature = obj["temperature"].toDouble(0.7);
        cfg.maxTokens = obj["maxTokens"].toInt(4096);
        cfg.timeoutSecs = obj["timeoutSecs"].toInt(120);
        cfg.systemPrompt = obj["systemPrompt"].toString();
        return cfg;
    }

    /// Predefined endpoints for common backends.
    static LlmConfig openAiDefault()
    {
        LlmConfig cfg;
        cfg.endpoint = "https://api.openai.com/v1/chat/completions";
        cfg.model = "gpt-4o";
        cfg.temperature = 0.7;
        cfg.maxTokens = 4096;
        return cfg;
    }

    static LlmConfig anthropicDefault()
    {
        LlmConfig cfg;
        cfg.endpoint = "https://api.anthropic.com/v1/messages";
        cfg.model = "claude-sonnet-4-20250514";
        cfg.temperature = 0.7;
        cfg.maxTokens = 4096;
        return cfg;
    }

    static LlmConfig deepSeekDefault()
    {
        LlmConfig cfg;
        cfg.endpoint = "https://api.deepseek.com/chat/completions";
        cfg.model = "deepseek-v4-flash";
        cfg.temperature = 0.7;
        cfg.maxTokens = 4096;
        return cfg;
    }

    static LlmConfig ollamaDefault()
    {
        LlmConfig cfg;
        cfg.endpoint = "http://localhost:11434/api/generate";
        cfg.model = "llama3";
        cfg.temperature = 0.7;
        cfg.maxTokens = 4096;
        return cfg;
    }
};
