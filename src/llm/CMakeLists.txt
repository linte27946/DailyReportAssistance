add_library(dailyreport_llm STATIC
    ILlmBackend.h
    LlmClient.cpp
    OpenAiBackend.cpp
    AnthropicBackend.cpp
    OllamaBackend.cpp
)

target_include_directories(dailyreport_llm PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(dailyreport_llm PUBLIC
    dailyreport_core
    dailyreport_util
    Qt6::Core
    Qt6::Network
)
