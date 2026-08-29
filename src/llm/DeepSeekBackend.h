#pragma once

#include "OpenAiBackend.h"

/// DeepSeek's Chat Completions endpoint is OpenAI API compatible.
class DeepSeekBackend : public OpenAiBackend {
public:
    explicit DeepSeekBackend(QObject *parent = nullptr)
        : OpenAiBackend("DeepSeek", LlmConfig::deepSeekDefault(), parent)
    {
    }
};
