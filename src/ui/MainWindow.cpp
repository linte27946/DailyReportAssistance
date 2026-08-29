#include "MainWindow.h"
#include "ReportViewer.h"
#include "ReportHistoryWidget.h"
#include "SettingsDialog.h"
#include "TimelineWidget.h"
#include "UiLanguage.h"
#include "report/TemplateEngine.h"
#include "report/ReportGenerator.h"
#include "storage/EventRepository.h"
#include "storage/ReportRepository.h"
#include "storage/SettingsRepository.h"
#include "app/DataRetentionService.h"
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMenuBar>
#include <QPushButton>
#include <QVBoxLayout>
#include <QCloseEvent>
#include <QApplication>
#include <QSystemTrayIcon>
#include <QMessageBox>
#include <QStyle>
#include <spdlog/spdlog.h>

MainWindow::MainWindow(TemplateEngine *templateEngine,
                       ReportGenerator *reportGenerator,
                       EventRepository *eventRepo,
                       ReportRepository *reportRepo,
                       SettingsRepository *settingsRepo,
                       DataRetentionService *retentionService,
                       QWidget *parent)
    : QMainWindow(parent)
    , m_templateEngine(templateEngine)
    , m_reportGenerator(reportGenerator)
    , m_reportRepo(reportRepo)
    , m_settingsRepo(settingsRepo)
{
    setWindowTitle("DailyReport");
    setMinimumSize(980, 640);
    resize(1220, 780);

    // Create child widgets
    m_reportViewer = new ReportViewer(reportGenerator, reportRepo, this);
    m_historyWidget = new ReportHistoryWidget(reportRepo, this);
    m_settingsDialog = new SettingsDialog(
        settingsRepo, templateEngine, retentionService, this);
    m_timelineWidget = new TimelineWidget(eventRepo, this);

    connect(m_historyWidget, &ReportHistoryWidget::reportSelected,
            m_reportViewer, &ReportViewer::loadReport);
    connect(m_settingsDialog, &SettingsDialog::settingsSaved,
            this, &MainWindow::settingsSaved);
    connect(m_settingsDialog, &SettingsDialog::settingsSaved, this, [this]() {
        m_historyWidget->refresh();
        m_timelineWidget->refresh();
        updatePageHeader(m_stack->currentIndex());
    });
    if (retentionService) {
        connect(retentionService, &DataRetentionService::cleanupFinished,
                this, [this](int, int, const QDate &, const QDate &,
                             const QDateTime &, bool) {
            m_historyWidget->refresh();
            m_timelineWidget->refresh();
        });
    }

    setupUi();
    createMenuBar();

    selectPage(0);
}

void MainWindow::setupUi()
{
    auto *central = new QWidget(this);
    auto *rootLayout = new QHBoxLayout(central);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    auto *sidebar = new QFrame(central);
    sidebar->setObjectName("sidebar");
    sidebar->setFixedWidth(220);

    auto *sidebarLayout = new QVBoxLayout(sidebar);
    sidebarLayout->setContentsMargins(18, 24, 18, 20);
    sidebarLayout->setSpacing(14);

    auto *brandRow = new QHBoxLayout();
    brandRow->setSpacing(10);
    auto *brandIcon = new QLabel(sidebar);
    brandIcon->setObjectName("brandIcon");
    brandIcon->setPixmap(QApplication::windowIcon().pixmap(38, 38));
    auto *brandText = new QVBoxLayout();
    brandText->setSpacing(1);
    auto *brand = new QLabel("DailyReport", sidebar);
    brand->setObjectName("brandLabel");
    auto *tagline = new QLabel(sidebar);
    tagline->setObjectName("brandTagline");
    UiLanguage::bindText(tagline, "Activity to insight", "让工作轨迹变成日报");
    brandText->addWidget(brand);
    brandText->addWidget(tagline);
    brandRow->addWidget(brandIcon);
    brandRow->addLayout(brandText, 1);
    sidebarLayout->addLayout(brandRow);
    sidebarLayout->addSpacing(20);

    auto *navigationLabel = new QLabel(sidebar);
    navigationLabel->setObjectName("navigationLabel");
    UiLanguage::bindText(navigationLabel, "WORKSPACE", "工作区");
    sidebarLayout->addWidget(navigationLabel);

    m_navigation = new QListWidget(sidebar);
    m_navigation->setObjectName("navigation");
    m_navigation->setFrameShape(QFrame::NoFrame);
    m_navigation->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_navigation->setSelectionMode(QAbstractItemView::SingleSelection);
    m_navigation->addItems({"", "", "", ""});
    m_navigation->item(0)->setIcon(
        style()->standardIcon(QStyle::SP_FileDialogContentsView));
    m_navigation->item(1)->setIcon(
        style()->standardIcon(QStyle::SP_FileDialogListView));
    m_navigation->item(2)->setIcon(
        style()->standardIcon(QStyle::SP_BrowserReload));
    m_navigation->item(3)->setIcon(
        style()->standardIcon(QStyle::SP_FileDialogDetailedView));
    UiLanguage::bindListItem(m_navigation, 0, "Reports", "报告中心");
    UiLanguage::bindListItem(m_navigation, 1, "History", "历史报告");
    UiLanguage::bindListItem(m_navigation, 2, "Activity timeline", "活动时间线");
    UiLanguage::bindListItem(m_navigation, 3, "Settings", "设置");
    for (int row = 0; row < m_navigation->count(); ++row) {
        m_navigation->item(row)->setSizeHint(QSize(0, 44));
    }
    sidebarLayout->addWidget(m_navigation);
    sidebarLayout->addStretch();

    auto *privacyLabel = new QLabel(sidebar);
    privacyLabel->setObjectName("privacyLabel");
    privacyLabel->setWordWrap(true);
    UiLanguage::bindText(
        privacyLabel,
        "Activity data stays on this device\nuntil a report is generated.",
        "活动数据保存在本机，\n仅在生成报告时发送给 AI。");
    sidebarLayout->addWidget(privacyLabel);

    auto *mainArea = new QWidget(central);
    mainArea->setObjectName("mainArea");
    auto *mainLayout = new QVBoxLayout(mainArea);
    mainLayout->setContentsMargins(28, 24, 28, 28);
    mainLayout->setSpacing(18);

    auto *header = new QFrame(mainArea);
    header->setObjectName("pageHeader");
    auto *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(12);

    auto *headingLayout = new QVBoxLayout();
    headingLayout->setSpacing(4);
    m_pageTitle = new QLabel(header);
    m_pageTitle->setObjectName("pageTitle");
    m_pageSubtitle = new QLabel(header);
    m_pageSubtitle->setObjectName("pageSubtitle");
    headingLayout->addWidget(m_pageTitle);
    headingLayout->addWidget(m_pageSubtitle);
    headerLayout->addLayout(headingLayout, 1);

    auto *monitoringStatus = new QLabel(header);
    monitoringStatus->setObjectName(
        m_settingsRepo->getBool("monitoring_enabled", true)
            ? "monitoringActive" : "monitoringPaused");
    UiLanguage::bindText(
        monitoringStatus,
        m_settingsRepo->getBool("monitoring_enabled", true)
            ? "● Local capture active" : "● Capture paused",
        m_settingsRepo->getBool("monitoring_enabled", true)
            ? "● 本机采集运行中" : "● 采集已暂停");
    headerLayout->addWidget(monitoringStatus);

    auto *weeklyButton = new QPushButton(header);
    weeklyButton->setObjectName("secondaryButton");
    weeklyButton->setIcon(style()->standardIcon(QStyle::SP_FileDialogListView));
    UiLanguage::bindText(weeklyButton, "Generate weekly", "生成周报");
    auto *dailyButton = new QPushButton(header);
    dailyButton->setObjectName("primaryButton");
    dailyButton->setIcon(style()->standardIcon(QStyle::SP_DialogApplyButton));
    UiLanguage::bindText(dailyButton, "Generate daily", "生成日报");
    headerLayout->addWidget(weeklyButton);
    headerLayout->addWidget(dailyButton);

    connect(weeklyButton, &QPushButton::clicked, this, [this]() {
        showReportViewer();
        m_reportViewer->generateReport("weekly");
    });
    connect(dailyButton, &QPushButton::clicked, this, [this]() {
        showReportViewer();
        m_reportViewer->generateReport("daily");
    });
    mainLayout->addWidget(header);

    m_stack = new QStackedWidget(this);
    m_stack->setObjectName("contentStack");
    m_stack->addWidget(m_reportViewer);   // index 0
    m_stack->addWidget(m_historyWidget);  // index 1
    m_stack->addWidget(m_timelineWidget); // index 2
    m_stack->addWidget(m_settingsDialog); // index 3
    mainLayout->addWidget(m_stack, 1);

    rootLayout->addWidget(sidebar);
    rootLayout->addWidget(mainArea, 1);
    setCentralWidget(central);

    connect(m_navigation, &QListWidget::currentRowChanged,
            this, &MainWindow::selectPage);

    setStyleSheet(R"(
        QMainWindow, QWidget#mainArea {
            background: #f3f5f9;
            color: #172033;
            font-size: 13px;
        }
        QMenuBar {
            background: #182235;
            color: #dfe7f4;
            padding: 3px 8px;
        }
        QMenuBar::item:selected { background: #2b3850; }
        QMenu { background: #ffffff; color: #172033; border: 1px solid #dce2ec; }
        QMenu::item:selected { background: #e9f0ff; }
        QFrame#sidebar { background: #162238; }
        QLabel#brandIcon { background: transparent; }
        QLabel#brandLabel { color: #ffffff; font-size: 20px; font-weight: 700; }
        QLabel#brandTagline { color: #91a1bb; font-size: 12px; }
        QLabel#navigationLabel { color: #70819d; font-size: 10px; font-weight: 600; }
        QListWidget#navigation {
            background: transparent;
            color: #bdc9da;
            border: none;
            outline: none;
        }
        QListWidget#navigation::item {
            border-radius: 8px;
            padding: 0 12px;
            margin: 2px 0;
        }
        QListWidget#navigation::item:hover { background: #22304a; color: #ffffff; }
        QListWidget#navigation::item:selected { background: #356ae6; color: #ffffff; }
        QLabel#privacyLabel {
            color: #8292ab;
            background: #202d43;
            border-radius: 8px;
            padding: 12px;
            font-size: 11px;
        }
        QLabel#pageTitle { color: #172033; font-size: 24px; font-weight: 600; }
        QLabel#pageSubtitle { color: #69758a; font-size: 12px; }
        QLabel#monitoringActive, QLabel#monitoringPaused {
            border-radius: 14px;
            padding: 6px 10px;
            font-size: 11px;
            font-weight: 600;
        }
        QLabel#monitoringActive { color: #147653; background: #e5f7f0; }
        QLabel#monitoringPaused { color: #9b5a15; background: #fff3df; }
        QPushButton {
            min-height: 36px;
            padding: 0 16px;
            border-radius: 7px;
            border: 1px solid #d4dbe7;
            background: #ffffff;
            color: #263248;
        }
        QPushButton:hover { border-color: #9daac0; background: #f8faff; }
        QPushButton:focus { border: 2px solid #8eacf4; }
        QPushButton:disabled { color: #9aa4b4; background: #eef1f5; }
        QPushButton#primaryButton {
            background: #356ae6;
            border-color: #356ae6;
            color: #ffffff;
            font-weight: 600;
        }
        QPushButton#primaryButton:hover { background: #2859cf; }
        QPushButton#dangerButton {
            color: #b43b45;
            background: #fff8f8;
            border-color: #efcbd0;
        }
        QPushButton#dangerButton:hover { background: #fff0f1; border-color: #dd9ca4; }
        QStackedWidget#contentStack {
            background: #ffffff;
            border: 1px solid #e0e5ed;
            border-radius: 12px;
        }
        QToolBar {
            background: transparent;
            border: none;
            spacing: 6px;
        }
        QToolButton {
            color: #344157;
            background: #f5f7fa;
            border: 1px solid #e0e5ed;
            border-radius: 6px;
            padding: 7px 10px;
        }
        QToolButton:hover { background: #eaf0fb; border-color: #c6d1e2; }
        QTextBrowser {
            background: #ffffff;
            color: #202a3b;
            border: 1px solid #e2e7ef;
            border-radius: 8px;
            padding: 16px;
            selection-background-color: #bfd2ff;
        }
        QLineEdit, QComboBox, QSpinBox, QDoubleSpinBox, QTimeEdit, QDateEdit, QTextEdit {
            min-height: 32px;
            border: 1px solid #d4dbe7;
            border-radius: 6px;
            background: #ffffff;
            padding: 0 8px;
        }
        QLineEdit:focus, QComboBox:focus, QSpinBox:focus,
        QDoubleSpinBox:focus, QTimeEdit:focus, QDateEdit:focus, QTextEdit:focus {
            border: 2px solid #83a5f6;
        }
        QTabWidget::pane {
            border: 1px solid #e0e5ed;
            border-radius: 10px;
            background: #ffffff;
            top: -1px;
        }
        QTabBar::tab {
            padding: 10px 14px;
            color: #667287;
            background: transparent;
            border-bottom: 2px solid transparent;
        }
        QTabBar::tab:hover { color: #2859cf; background: #f4f7fd; }
        QTabBar::tab:selected {
            color: #2859cf;
            font-weight: 600;
            border-bottom-color: #356ae6;
        }
        QGroupBox {
            color: #27344b;
            background: #fbfcfe;
            border: 1px solid #e2e7ef;
            border-radius: 9px;
            margin-top: 13px;
            padding: 14px 12px 10px 12px;
            font-weight: 600;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 12px;
            padding: 0 5px;
            background: #ffffff;
        }
        QLabel#settingsHint {
            color: #526179;
            background: #edf3ff;
            border: 1px solid #d6e3ff;
            border-radius: 8px;
            padding: 11px 13px;
        }
        QLabel#fieldHint { color: #7a8699; font-size: 11px; font-weight: 400; }
        QLabel#cleanupStatus {
            color: #526179;
            background: #f2f5f9;
            border-radius: 7px;
            padding: 9px 11px;
            font-weight: 400;
        }
        QLabel#summaryBadge {
            color: #526179;
            background: #eef2f8;
            border-radius: 13px;
            padding: 5px 10px;
            font-size: 11px;
        }
        QTableWidget {
            background: #ffffff;
            alternate-background-color: #f8faff;
            border: 1px solid #e1e6ee;
            border-radius: 8px;
            gridline-color: #edf0f5;
            selection-background-color: #dce8ff;
            selection-color: #20304d;
        }
        QTableWidget::item { padding: 7px; }
        QHeaderView::section {
            background: #f5f7fa;
            color: #4c586c;
            border: none;
            border-bottom: 1px solid #dfe5ee;
            padding: 8px;
        }
        QScrollBar:vertical { width: 10px; background: transparent; margin: 2px; }
        QScrollBar::handle:vertical { background: #c3ccda; border-radius: 5px; min-height: 28px; }
        QScrollBar::handle:vertical:hover { background: #a9b5c6; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
    )");
}

void MainWindow::createMenuBar()
{
    QMenu *fileMenu = menuBar()->addMenu("&File");
    UiLanguage::bindText(fileMenu, "&File", "文件(&F)");
    QAction *dailyAction = fileMenu->addAction("", this, [this]() {
        m_reportViewer->generateReport("daily");
    });
    UiLanguage::bindText(dailyAction, "Generate Daily Report", "生成日报");
    dailyAction->setIcon(style()->standardIcon(QStyle::SP_DialogApplyButton));
    QAction *weeklyAction = fileMenu->addAction("", this, [this]() {
        m_reportViewer->generateReport("weekly");
    });
    UiLanguage::bindText(weeklyAction, "Generate Weekly Report", "生成周报");
    weeklyAction->setIcon(style()->standardIcon(QStyle::SP_FileDialogListView));
    fileMenu->addSeparator();
    QAction *exitAction = fileMenu->addAction("", this, []() { QApplication::quit(); });
    UiLanguage::bindText(exitAction, "E&xit", "退出(&X)");

    QMenu *viewMenu = menuBar()->addMenu("&View");
    UiLanguage::bindText(viewMenu, "&View", "视图(&V)");
    QAction *reportAction = viewMenu->addAction("", this, &MainWindow::showReportViewer);
    QAction *historyAction = viewMenu->addAction("", this, &MainWindow::showHistory);
    QAction *timelineAction = viewMenu->addAction("", this, &MainWindow::showTimeline);
    QAction *settingsAction = viewMenu->addAction("", this, &MainWindow::showSettings);
    UiLanguage::bindText(reportAction, "Report Viewer", "报告中心");
    UiLanguage::bindText(historyAction, "Report History", "历史报告");
    UiLanguage::bindText(timelineAction, "Activity Timeline", "活动时间线");
    UiLanguage::bindText(settingsAction, "Settings", "设置");

    QMenu *helpMenu = menuBar()->addMenu("&Help");
    UiLanguage::bindText(helpMenu, "&Help", "帮助(&H)");
    QAction *aboutAction = helpMenu->addAction("", this, [this]() {
        QMessageBox::about(
            this,
            UiLanguage::text("About DailyReport", "关于 DailyReport"),
            UiLanguage::text(
                "DailyReport v1.0.0\n\n"
                "Automatic developer activity tracking and report generation.\n"
                "Supports OpenAI, DeepSeek, Anthropic, and local LLMs.",
                "DailyReport v1.0.0\n\n"
                "自动记录开发活动并生成日报或周报。\n"
                "支持 OpenAI、DeepSeek、Anthropic 和本地大模型。"));
    });
    UiLanguage::bindText(aboutAction, "About DailyReport", "关于 DailyReport");
}

void MainWindow::selectPage(int index)
{
    if (index < 0 || index >= m_stack->count()) return;
    m_stack->setCurrentIndex(index);
    if (m_navigation->currentRow() != index) {
        m_navigation->setCurrentRow(index);
    }
    updatePageHeader(index);
}

void MainWindow::updatePageHeader(int index)
{
    static const QList<QPair<QString, QString>> titles = {
        {"Reports", "报告中心"},
        {"Report history", "历史报告"},
        {"Activity timeline", "活动时间线"},
        {"Settings", "设置"}
    };
    static const QList<QPair<QString, QString>> subtitles = {
        {"Generate, review, and export your daily or weekly work summary.",
         "生成、查看并导出你的日报或周报。"},
        {"Open previous reports and continue from where you left off.",
         "查看以前生成的报告并继续编辑。"},
        {"Review the local activity signals used to build your reports.",
         "查看用于生成报告的本机活动记录。"},
        {"Configure monitoring, schedules, privacy, and your AI provider.",
         "配置监控、计划任务、隐私和 AI 服务。"}
    };
    if (index < 0 || index >= titles.size()) return;
    UiLanguage::bindText(m_pageTitle, titles.at(index).first, titles.at(index).second);
    UiLanguage::bindText(m_pageSubtitle,
                         subtitles.at(index).first, subtitles.at(index).second);
}

void MainWindow::showReportViewer() { selectPage(0); }
void MainWindow::showHistory() { selectPage(1); }
void MainWindow::showTimeline() { selectPage(2); }
void MainWindow::showSettings() { selectPage(3); }

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (QSystemTrayIcon::isSystemTrayAvailable()) {
        hide();
        event->ignore();
        emit closeToTray();
    } else {
        event->accept();
        QApplication::quit();
    }
}
