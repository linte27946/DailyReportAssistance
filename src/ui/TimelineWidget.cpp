#include "TimelineWidget.h"
#include "UiLanguage.h"
#include "storage/EventRepository.h"
#include "core/Timeline.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QPushButton>
#include <QLabel>
#include <QCalendarWidget>
#include <QFrame>
#include <QStyle>
#include <QToolButton>
#include <QColor>

namespace {

QString categoryLabel(EventCategory category)
{
    switch (category) {
    case EventCategory::Coding:         return UiLanguage::text("Coding", "编码");
    case EventCategory::CodeReview:     return UiLanguage::text("Code review", "代码审查");
    case EventCategory::Debugging:      return UiLanguage::text("Debugging", "调试");
    case EventCategory::Building:       return UiLanguage::text("Building", "构建");
    case EventCategory::Testing:        return UiLanguage::text("Testing", "测试");
    case EventCategory::Documentation:  return UiLanguage::text("Documentation", "文档");
    case EventCategory::Communication:  return UiLanguage::text("Communication", "沟通");
    case EventCategory::VersionControl: return UiLanguage::text("Version control", "版本控制");
    case EventCategory::Browsing:       return UiLanguage::text("Browsing", "浏览网页");
    case EventCategory::Meeting:        return UiLanguage::text("Meeting", "会议");
    case EventCategory::Idle:           return UiLanguage::text("Idle", "空闲");
    case EventCategory::Other:          return UiLanguage::text("Other", "其他");
    case EventCategory::Distraction:    return UiLanguage::text("Entertainment", "娱乐/摸鱼");
    default:                            return UiLanguage::text("Unknown", "未知");
    }
}

QString typeLabel(EventType type)
{
    switch (type) {
    case EventType::FileCreated:        return UiLanguage::text("File created", "创建文件");
    case EventType::FileModified:       return UiLanguage::text("File modified", "修改文件");
    case EventType::FileDeleted:        return UiLanguage::text("File deleted", "删除文件");
    case EventType::FileRenamed:        return UiLanguage::text("File renamed", "重命名文件");
    case EventType::ProcessStarted:     return UiLanguage::text("Process started", "进程启动");
    case EventType::ProcessEnded:       return UiLanguage::text("Process ended", "进程结束");
    case EventType::WindowFocusChanged: return UiLanguage::text("Window changed", "切换窗口");
    case EventType::EditorContextChanged:return UiLanguage::text("Editor context", "编辑器上下文");
    case EventType::DocumentViewed:     return UiLanguage::text("Document viewed", "查看文档");
    case EventType::UserActive:         return UiLanguage::text("User active", "用户活跃");
    case EventType::UserIdle:           return UiLanguage::text("User idle", "用户空闲");
    case EventType::UrlVisited:         return UiLanguage::text("URL visited", "访问网址");
    case EventType::GitCommit:          return UiLanguage::text("Git commit", "Git 提交");
    case EventType::GitPush:            return UiLanguage::text("Git push", "Git 推送");
    case EventType::GitPull:            return UiLanguage::text("Git pull", "Git 拉取");
    case EventType::GitBranchSwitch:    return UiLanguage::text("Branch switched", "切换分支");
    case EventType::GitMerge:           return UiLanguage::text("Git merge", "Git 合并");
    case EventType::BuildStarted:       return UiLanguage::text("Build started", "开始构建");
    case EventType::BuildCompleted:     return UiLanguage::text("Build completed", "构建完成");
    case EventType::MeetingAttended:    return UiLanguage::text("Meeting attended", "实际参会");
    case EventType::SessionStarted:     return UiLanguage::text("Session started", "会话开始");
    case EventType::SessionEnded:       return UiLanguage::text("Session ended", "会话结束");
    default:                            return UiLanguage::text("Unknown", "未知");
    }
}

} // namespace

TimelineWidget::TimelineWidget(EventRepository *eventRepo, QWidget *parent)
    : QWidget(parent)
    , m_eventRepo(eventRepo)
{
    setupUi();
    refresh();
}

void TimelineWidget::setupUi()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(22, 20, 22, 22);
    layout->setSpacing(14);

    auto *header = new QHBoxLayout();
    header->setSpacing(9);
    auto *dateLabel = new QLabel();
    dateLabel->setObjectName("toolbarLabel");
    UiLanguage::bindText(dateLabel, "View date", "查看日期");
    header->addWidget(dateLabel);

    auto *datePicker = new QFrame(this);
    datePicker->setObjectName("timelineDatePicker");
    auto *datePickerLayout = new QHBoxLayout(datePicker);
    datePickerLayout->setContentsMargins(3, 2, 3, 2);
    datePickerLayout->setSpacing(2);

    auto *previousDateBtn = new QToolButton(datePicker);
    previousDateBtn->setObjectName("dateNavButton");
    previousDateBtn->setIcon(style()->standardIcon(QStyle::SP_ArrowLeft));
    UiLanguage::bindTooltip(previousDateBtn, "Previous day", "前一天");
    datePickerLayout->addWidget(previousDateBtn);

    m_dateEdit = new QDateEdit(QDate::currentDate());
    m_dateEdit->setObjectName("timelineDateEdit");
    m_dateEdit->setCalendarPopup(true);
    m_dateEdit->setDisplayFormat("yyyy-MM-dd");
    m_dateEdit->setAlignment(Qt::AlignCenter);
    m_dateEdit->setMinimumDate(QDate(2000, 1, 1));
    m_dateEdit->setMaximumDate(QDate::currentDate());
    m_dateEdit->setMinimumWidth(142);
    m_dateEdit->calendarWidget()->setFirstDayOfWeek(Qt::Monday);
    m_dateEdit->calendarWidget()->setGridVisible(false);
    m_dateEdit->calendarWidget()->setVerticalHeaderFormat(
        QCalendarWidget::NoVerticalHeader);
    datePickerLayout->addWidget(m_dateEdit);

    auto *nextDateBtn = new QToolButton(datePicker);
    nextDateBtn->setObjectName("dateNavButton");
    nextDateBtn->setIcon(style()->standardIcon(QStyle::SP_ArrowRight));
    nextDateBtn->setEnabled(false);
    UiLanguage::bindTooltip(nextDateBtn, "Next day", "后一天");
    datePickerLayout->addWidget(nextDateBtn);
    header->addWidget(datePicker);

    connect(previousDateBtn, &QToolButton::clicked, this, [this]() {
        m_dateEdit->setDate(m_dateEdit->date().addDays(-1));
    });
    connect(nextDateBtn, &QToolButton::clicked, this, [this]() {
        m_dateEdit->setDate(qMin(QDate::currentDate(),
                                 m_dateEdit->date().addDays(1)));
    });
    connect(m_dateEdit, &QDateEdit::dateChanged, this,
            [this, nextDateBtn](const QDate &date) {
        nextDateBtn->setEnabled(date < QDate::currentDate());
        refresh();
    });

    auto *todayBtn = new QPushButton();
    UiLanguage::bindText(todayBtn, "Today", "今天");
    connect(todayBtn, &QPushButton::clicked, this, [this]() {
        if (m_dateEdit->date() == QDate::currentDate())
            refresh();
        else
            m_dateEdit->setDate(QDate::currentDate());
    });
    header->addWidget(todayBtn);

    auto *refreshBtn = new QPushButton();
    refreshBtn->setIcon(style()->standardIcon(QStyle::SP_BrowserReload));
    UiLanguage::bindText(refreshBtn, "Refresh", "刷新");
    connect(refreshBtn, &QPushButton::clicked, this, &TimelineWidget::refresh);
    header->addWidget(refreshBtn);
    header->addStretch();

    m_totalLabel = new QLabel();
    m_totalLabel->setObjectName("summaryBadge");
    header->addWidget(m_totalLabel);
    layout->addLayout(header);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(5);
    m_table->setHorizontalHeaderLabels({"", "", "", "", ""});
    UiLanguage::bindHeader(m_table, 0, "Time", "时间");
    UiLanguage::bindHeader(m_table, 1, "Category", "分类");
    UiLanguage::bindHeader(m_table, 2, "Type", "类型");
    UiLanguage::bindHeader(m_table, 3, "Description", "描述");
    UiLanguage::bindHeader(m_table, 4, "Application", "应用程序");
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setAlternatingRowColors(true);
    m_table->setShowGrid(false);
    m_table->verticalHeader()->setVisible(false);
    m_table->verticalHeader()->setDefaultSectionSize(38);
    m_table->setColumnWidth(0, 100);
    m_table->setColumnWidth(1, 100);
    m_table->setColumnWidth(2, 120);
    m_table->setColumnWidth(3, 280);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);

    layout->addWidget(m_table);
}

void TimelineWidget::refresh()
{
    m_table->setRowCount(0);
    QDate date = m_dateEdit->date();
    Timeline timeline = m_eventRepo->queryTimeline(date);
    const auto &events = timeline.events();
    UiLanguage::bindText(
        m_totalLabel,
        QString("Total events: %1").arg(events.size()),
        QString("事件总数：%1").arg(events.size()));

    m_table->setRowCount(events.size());
    int row = 0;
    for (const auto &e : events) {
        m_table->setItem(row, 0, new QTableWidgetItem(
            e.timestamp.toLocalTime().toString("HH:mm:ss")));
        auto *categoryItem = new QTableWidgetItem(categoryLabel(e.category));
        if (e.category == EventCategory::Distraction) {
            categoryItem->setBackground(QColor("#FFF1DB"));
            categoryItem->setForeground(QColor("#9A5B18"));
        } else if (e.category == EventCategory::Meeting) {
            categoryItem->setBackground(QColor("#E9F3FF"));
            categoryItem->setForeground(QColor("#2859A8"));
        }
        m_table->setItem(row, 1, categoryItem);
        m_table->setItem(row, 2, new QTableWidgetItem(
            typeLabel(e.type)));
        auto *descriptionItem = new QTableWidgetItem(e.description);
        descriptionItem->setToolTip(e.description);
        m_table->setItem(row, 3, descriptionItem);
        m_table->setItem(row, 4, new QTableWidgetItem(e.application));
        row++;
    }
}
