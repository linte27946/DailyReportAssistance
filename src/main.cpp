#include <QApplication>
#include <QMessageBox>
#include <QTimer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QDir>
#include <QLocale>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include "app/Application.h"
#include "app/SingleInstance.h"
#include "util/Log.h"
#include "ui/MainWindow.h"
#include "ui/SystemTray.h"
#include "ui/SetupWizard.h"
#include "ui/UiLanguage.h"
#include "report/TemplateEngine.h"
#include "report/ReportGenerator.h"
#include "report/ReportScheduler.h"
#include "llm/LlmClient.h"
#include "llm/OpenAiBackend.h"
#include "llm/DeepSeekBackend.h"
#include "llm/AnthropicBackend.h"
#include "llm/OllamaBackend.h"
#include "storage/EventRepository.h"
#include "storage/ReportRepository.h"
#include "storage/SettingsRepository.h"
#include "monitor/MonitorEngine.h"
#include "monitor/FileSystemMonitor.h"
#include "monitor/ProcessMonitor.h"
#include "monitor/WindowFocusMonitor.h"
#include "monitor/WorkContextMonitor.h"
#include "monitor/InputActivityMonitor.h"
#include "monitor/BrowserUrlMonitor.h"
#include "monitor/GitMonitor.h"
#include "monitor/BuildMonitor.h"
#include "pipeline/EventPipeline.h"
#include "util/CryptoUtils.h"

namespace {

QStringList monitoredPaths(SettingsRepository *settings)
{
    QStringList paths;
    const QJsonDocument document = QJsonDocument::fromJson(
        settings->getValue("monitored_paths", "[]").toUtf8());
    for (const auto &value : document.array()) {
        if (!value.toString().isEmpty())
            paths.append(value.toString());
    }
    return paths;
}

LlmConfig defaultLlmConfig(const QString &backend)
{
    if (backend == "Anthropic") return LlmConfig::anthropicDefault();
    if (backend == "DeepSeek") return LlmConfig::deepSeekDefault();
    if (backend == "Ollama") return LlmConfig::ollamaDefault();
    return LlmConfig::openAiDefault();
}

void configureLlm(LlmClient *client, SettingsRepository *settings)
{
    const QString backend = settings->getValue("llm_backend").trimmed();
    if (backend.isEmpty() || !client->availableBackends().contains(backend)) {
        client->clearActiveBackend();
        spdlog::warn("No LLM backend is configured; report generation is disabled.");
        return;
    }

    LlmConfig config = defaultLlmConfig(backend);
    const QJsonObject stored = settings->getJson("llm_config");
    if (!stored["endpoint"].toString().trimmed().isEmpty())
        config.endpoint = stored["endpoint"].toString().trimmed();
    if (!stored["model"].toString().trimmed().isEmpty())
        config.model = stored["model"].toString().trimmed();
    if (stored.contains("temperature"))
        config.temperature = stored["temperature"].toDouble(config.temperature);
    if (stored.contains("maxTokens"))
        config.maxTokens = stored["maxTokens"].toInt(config.maxTokens);
    if (stored.contains("timeoutSecs"))
        config.timeoutSecs = stored["timeoutSecs"].toInt(config.timeoutSecs);
    config.apiKey = QString::fromUtf8(CryptoUtils::decrypt(
        settings->getValue("llm_api_key_encrypted").toUtf8()));

    client->configureBackend(backend, config);
    client->setActiveBackend(backend);
}

void configureScheduler(ReportScheduler *scheduler, SettingsRepository *settings)
{
    QTime daily = QTime::fromString(
        settings->getValue("daily_report_time", "17:30"), "HH:mm");
    QTime weekly = QTime::fromString(
        settings->getValue("weekly_report_time", "17:00"), "HH:mm");
    if (!daily.isValid()) daily = QTime(17, 30);
    if (!weekly.isValid()) weekly = QTime(17, 0);
    scheduler->setDailyReportTime(daily);
    scheduler->setWeeklyReportDay(
        qBound(1, settings->getInt("weekly_report_day", 5), 7));
    scheduler->setWeeklyReportTime(weekly);
    scheduler->setEnabled(settings->getBool("report_scheduler_enabled", true));
}

} // namespace

int main(int argc, char *argv[])
{
    // Construct QApplication first so QStandardPaths and platform plugins are
    // initialized before logging and single-instance coordination use them.
    Application app(argc, argv);
    UiLanguage::setLanguage(
        QLocale::system().language() == QLocale::Chinese ? "zh-CN" : "en");
    Log::init();
    spdlog::info("DailyReport starting...");

    // Check single instance - only one instance of the app should run
    SingleInstance guard("DailyReport_SingleInstance");
    if (!guard.tryLock()) {
        spdlog::warn("Another instance is already running. Exiting.");
        guard.notifyExistingInstance();
        return 0;
    }

    // Ensure data directory exists
    QString dataDir = Application::dataDirectory();
    if (!QDir().mkpath(dataDir)) {
        spdlog::critical("Failed to create application data directory: {}",
                         dataDir.toStdString());
        return 1;
    }

    // Initialize application services
    if (!app.initialize()) {
        spdlog::critical("Failed to initialize application services.");
        QMessageBox::critical(
            nullptr, "DailyReport",
            UiLanguage::text(
                "Failed to initialize application services.\n"
                "Please check the log files for details.",
                "应用服务初始化失败。\n请检查日志文件了解详细信息。"));
        return 1;
    }

    // --- Create application services ---

    // Core services
    auto *templateEngine = new TemplateEngine(&app);
    auto *llmClient = new LlmClient(&app);
    auto *eventRepo = new EventRepository();
    auto *reportRepo = new ReportRepository();
    auto *settingsRepo = new SettingsRepository();

    UiLanguage::setLanguage(settingsRepo->getValue("language", UiLanguage::language()));

    templateEngine->loadFromDatabase();
    llmClient->registerBackend(std::make_unique<OpenAiBackend>());
    llmClient->registerBackend(std::make_unique<DeepSeekBackend>());
    llmClient->registerBackend(std::make_unique<AnthropicBackend>());
    llmClient->registerBackend(std::make_unique<OllamaBackend>());

    const bool firstRun = !settingsRepo->getBool("setup_complete", false);
    if (firstRun) {
        SetupWizard wizard(settingsRepo);
        wizard.exec();
    }
    configureLlm(llmClient, settingsRepo);

    // Report generation
    auto *reportGenerator = new ReportGenerator(
        templateEngine, llmClient, eventRepo, reportRepo, &app);
    auto *reportScheduler = new ReportScheduler(reportGenerator, &app);
    reportGenerator->setReportLanguage(settingsRepo->getValue("language", "zh-CN"));
    configureScheduler(reportScheduler, settingsRepo);

    // Monitoring
    auto *monitorEngine = new MonitorEngine(&app);
    auto fileMonitor = std::make_unique<FileSystemMonitor>();
    fileMonitor->addWatchPaths(monitoredPaths(settingsRepo));
    monitorEngine->registerMonitor(std::move(fileMonitor));

    auto processMonitor = std::make_unique<ProcessMonitor>();
    ProcessMonitor *processMonitorPtr = processMonitor.get();

    // Start BuildMonitor before ProcessMonitor so the initial process snapshot
    // can be translated into build events instead of being missed at startup.
    if (settingsRepo->getBool("build_tracking_enabled", true)) {
        auto buildMonitor = std::make_unique<BuildMonitor>();
        BuildMonitor *buildMonitorPtr = buildMonitor.get();
        QObject::connect(processMonitorPtr, &IMonitor::rawEventCaptured,
                         buildMonitorPtr, &BuildMonitor::onProcessEvent);
        monitorEngine->registerMonitor(std::move(buildMonitor));
    }

    monitorEngine->registerMonitor(std::move(processMonitor));
    auto windowFocusMonitor = std::make_unique<WindowFocusMonitor>();
    WindowFocusMonitor *windowFocusMonitorPtr = windowFocusMonitor.get();
    const bool trackEditors = settingsRepo->getBool("editor_tracking_enabled", true);
    const bool trackDocuments = settingsRepo->getBool("document_tracking_enabled", true);
    if (trackEditors || trackDocuments) {
        auto workContextMonitor = std::make_unique<WorkContextMonitor>();
        workContextMonitor->setTrackEditors(trackEditors);
        workContextMonitor->setTrackDocuments(trackDocuments);
        WorkContextMonitor *workContextMonitorPtr = workContextMonitor.get();
        QObject::connect(windowFocusMonitorPtr, &IMonitor::rawEventCaptured,
                         workContextMonitorPtr, &WorkContextMonitor::processWindowEvent);
        monitorEngine->registerMonitor(std::move(workContextMonitor));
    }
    monitorEngine->registerMonitor(std::move(windowFocusMonitor));

    auto inputMonitor = std::make_unique<InputActivityMonitor>();
    inputMonitor->setAfkThreshold(settingsRepo->getInt("afk_threshold_secs", 300));
    monitorEngine->registerMonitor(std::move(inputMonitor));

    if (settingsRepo->getBool("browser_tracking_enabled", true)) {
        auto browserMonitor = std::make_unique<BrowserUrlMonitor>();
        browserMonitor->setCaptureFullUrl(
            settingsRepo->getBool("browser_capture_full_url", false));
        monitorEngine->registerMonitor(std::move(browserMonitor));
    }

    if (settingsRepo->getBool("git_tracking_enabled", true)) {
        auto gitMonitor = std::make_unique<GitMonitor>();
        gitMonitor->addRepoPaths(monitoredPaths(settingsRepo));
        monitorEngine->registerMonitor(std::move(gitMonitor));
    }

    // Event pipeline
    auto *pipeline = new EventPipeline(&app);
    pipeline->setSessionId(app.sessionId());
    pipeline->loadClassificationRules(
        settingsRepo->getValue("classification_rules", "{}").toUtf8());
    pipeline->loadFilterRules(
        settingsRepo->getValue("filter_rules", "{}").toUtf8());
    QObject::connect(monitorEngine, &MonitorEngine::rawEventCaptured,
                     pipeline, &EventPipeline::onRawEvent);
    QObject::connect(pipeline, &EventPipeline::eventsProcessed,
                     &app, [eventRepo](const QList<ActivityEvent> &events) {
                         if (!eventRepo->insertBatch(events))
                             spdlog::error("Failed to persist a processed event batch.");
                     });

    // UI
    auto *mainWindow = new MainWindow(
        templateEngine, reportGenerator, eventRepo, reportRepo, settingsRepo);
    auto *systemTray = new SystemTray(mainWindow, reportGenerator, reportScheduler, &app);

    // Wire tray ↔ window
    QObject::connect(systemTray, &SystemTray::showMainWindowRequested,
                     mainWindow, &QMainWindow::show);
    QObject::connect(systemTray, &SystemTray::showSettingsRequested,
                     mainWindow, &MainWindow::showSettings);
    QObject::connect(systemTray, &SystemTray::exitRequested,
                     &app, &QApplication::quit);
    QObject::connect(mainWindow, &MainWindow::closeToTray,
                     mainWindow, &QMainWindow::hide);
    QObject::connect(mainWindow, &MainWindow::settingsSaved,
                     &app, [=]() {
                         configureLlm(llmClient, settingsRepo);
                         configureScheduler(reportScheduler, settingsRepo);
                         reportGenerator->setReportLanguage(
                             settingsRepo->getValue("language", "zh-CN"));
                     });
    QObject::connect(&app, &QCoreApplication::aboutToQuit,
                     llmClient, &LlmClient::cancelActiveGeneration);

    // Initialize system tray
    if (!systemTray->initialize()) {
        spdlog::error("Failed to initialize system tray icon.");
    }

    pipeline->start();
    if (settingsRepo->getBool("monitoring_enabled", true))
        monitorEngine->startAll();

    eventRepo->pruneOlderThan(QDate::currentDate().addDays(
        -settingsRepo->getInt("data_retention_days", 90)));

    // Show window on first run (setup wizard will guide user)
    if (firstRun || !settingsRepo->getBool("start_minimized", false)
        || !systemTray->isAvailable()) {
        mainWindow->show();
    }

    spdlog::info("DailyReport initialized successfully.");
    int result = app.run();

    // Cleanup
    reportGenerator->waitForFinished();
    monitorEngine->stopAll();
    pipeline->stop();

    return result;
}
