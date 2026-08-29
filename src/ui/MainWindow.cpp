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
#include <spdlog/spdlog.h>

MainWindow::MainWindow(TemplateEngine *templateEngine,
                       ReportGenerator *reportGenerator,
                       EventRepository *eventRepo,
                       ReportRepository *reportRepo,
                       SettingsRepository *settingsRepo,
                       QWidget *parent)
    : QMainWindow(parent)
    , m_templateEngine(templateEngine)
    , m_reportGenerator(reportGenerator)
    , m_reportRepo(reportRepo)
{
    setWindowTitle("DailyReport");
    setMinimumSize(980, 640);
    resize(1220, 780);

    // Create child widgets
    m_reportViewer = new ReportViewer(reportGenerator, reportRepo, this);
    m_historyWidget = new ReportHistoryWidget(reportRepo, this);
    m_settingsDialog = new SettingsDialog(settingsRepo, templateEngine, this);
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

    auto *brand = new QLabel("DailyReport", sidebar);
    brand->setObjectName("brandLabel");
    auto *tagline = new QLabel(sidebar);
    tagline->setObjectName("brandTagline");
    UiLanguage::bindText(tagline, "Activity to insight", "让工作轨迹变成日报");
    sidebarLayout->addWidget(brand);
    sidebarLayout->addWidget(tagline);
    sidebarLayout->addSpacing(18);

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

    auto *weeklyButton = new QPushButton(header);
    weeklyButton->setObjectName("secondaryButton");
    UiLanguage::bindText(weeklyButton, "Generate weekly", "生成周报");
    auto *dailyButton = new QPushButton(header);
    dailyButton->setObjectName("primaryButton");
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
            background: #f4f6f9;
            color: #172033;
        }
        QMenuBar {
            background: #182235;
            color: #dfe7f4;
            padding: 3px 8px;
        }
        QMenuBar::item:selected { background: #2b3850; }
        QMenu { background: #ffffff; color: #172033; border: 1px solid #dce2ec; }
        QMenu::item:selected { background: #e9f0ff; }
        QFrame#sidebar { background: #182235; }
        QLabel#brandLabel { color: #ffffff; font-size: 22px; font-weight: 600; }
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
        QPushButton {
            min-height: 36px;
            padding: 0 16px;
            border-radius: 7px;
            border: 1px solid #d4dbe7;
            background: #ffffff;
            color: #263248;
        }
        QPushButton:hover { border-color: #9daac0; background: #f8faff; }
        QPushButton#primaryButton {
            background: #356ae6;
            border-color: #356ae6;
            color: #ffffff;
            font-weight: 600;
        }
        QPushButton#primaryButton:hover { background: #2859cf; }
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
        QLineEdit, QComboBox, QSpinBox, QTimeEdit, QTextEdit {
            min-height: 32px;
            border: 1px solid #d4dbe7;
            border-radius: 6px;
            background: #ffffff;
            padding: 0 8px;
        }
        QTabWidget::pane { border: 1px solid #e0e5ed; border-radius: 8px; }
        QTabBar::tab { padding: 8px 14px; color: #5e6a7f; }
        QTabBar::tab:selected { color: #2859cf; font-weight: 600; }
        QHeaderView::section {
            background: #f5f7fa;
            color: #4c586c;
            border: none;
            border-bottom: 1px solid #dfe5ee;
            padding: 8px;
        }
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
    QAction *weeklyAction = fileMenu->addAction("", this, [this]() {
        m_reportViewer->generateReport("weekly");
    });
    UiLanguage::bindText(weeklyAction, "Generate Weekly Report", "生成周报");
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
                "Supports OpenAI, Anthropic, and local LLMs.",
                "DailyReport v1.0.0\n\n"
                "自动记录开发活动并生成日报或周报。\n"
                "支持 OpenAI、Anthropic 和本地大模型。"));
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
