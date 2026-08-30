#include "ReportDeleteDialog.h"

#include "UiLanguage.h"
#include <QApplication>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QVBoxLayout>

namespace {

QPixmap deleteIconPixmap()
{
    QPixmap pixmap(28, 28);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    QPen pen(QColor("#C53F4A"), 2.2, Qt::SolidLine,
             Qt::RoundCap, Qt::RoundJoin);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    painter.drawLine(QPointF(5, 7), QPointF(23, 7));
    painter.drawLine(QPointF(11, 4), QPointF(17, 4));
    painter.drawRoundedRect(QRectF(8, 9, 12, 14), 2, 2);
    painter.drawLine(QPointF(12, 12), QPointF(12, 20));
    painter.drawLine(QPointF(16, 12), QPointF(16, 20));
    return pixmap;
}

} // namespace

ReportDeleteDialog::ReportDeleteDialog(const QString &reportTitle,
                                       const QString &reportType,
                                       const QDate &reportDate,
                                       QWidget *parent)
    : QDialog(parent)
{
    setObjectName("deleteReportDialog");
    setWindowTitle(UiLanguage::text("Delete report", "删除报告"));
    setWindowIcon(QApplication::windowIcon());
    setModal(true);
    setMinimumWidth(540);
    setMaximumWidth(620);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(26, 24, 26, 22);
    root->setSpacing(18);

    auto *headingRow = new QHBoxLayout();
    headingRow->setSpacing(15);
    auto *iconBadge = new QLabel(this);
    iconBadge->setObjectName("deleteIconBadge");
    iconBadge->setAlignment(Qt::AlignCenter);
    iconBadge->setFixedSize(54, 54);
    iconBadge->setPixmap(deleteIconPixmap());
    headingRow->addWidget(iconBadge, 0, Qt::AlignTop);

    auto *headingText = new QVBoxLayout();
    headingText->setSpacing(5);
    auto *title = new QLabel(
        UiLanguage::text("Delete this report?", "删除这份报告？"), this);
    title->setObjectName("deleteDialogHeading");
    auto *subtitle = new QLabel(
        UiLanguage::text(
            "The report will be removed from DailyReport history.",
            "该报告将从 DailyReport 的历史记录中移除。"),
        this);
    subtitle->setObjectName("deleteDialogSubtitle");
    subtitle->setWordWrap(true);
    headingText->addWidget(title);
    headingText->addWidget(subtitle);
    headingRow->addLayout(headingText, 1);
    root->addLayout(headingRow);

    auto *reportCard = new QFrame(this);
    reportCard->setObjectName("deleteReportCard");
    auto *cardLayout = new QVBoxLayout(reportCard);
    cardLayout->setContentsMargins(16, 13, 16, 13);
    cardLayout->setSpacing(5);
    auto *reportName = new QLabel(reportTitle, reportCard);
    reportName->setObjectName("deleteReportName");
    reportName->setWordWrap(true);
    const QString localizedType = reportType.compare("weekly", Qt::CaseInsensitive) == 0
        ? UiLanguage::text("Weekly report", "周报")
        : UiLanguage::text("Daily report", "日报");
    auto *reportMeta = new QLabel(
        QString("%1  ·  %2").arg(localizedType,
                                  reportDate.toString("yyyy-MM-dd")),
        reportCard);
    reportMeta->setObjectName("deleteReportMeta");
    cardLayout->addWidget(reportName);
    cardLayout->addWidget(reportMeta);
    root->addWidget(reportCard);

    auto *warningCard = new QFrame(this);
    warningCard->setObjectName("deleteWarningCard");
    auto *warningLayout = new QHBoxLayout(warningCard);
    warningLayout->setContentsMargins(13, 11, 13, 11);
    warningLayout->setSpacing(9);
    auto *warningMark = new QLabel(QStringLiteral("!"), warningCard);
    warningMark->setObjectName("deleteWarningMark");
    warningMark->setAlignment(Qt::AlignCenter);
    warningMark->setFixedSize(22, 22);
    auto *warningText = new QLabel(
        UiLanguage::text(
            "This cannot be undone. Previously exported files will not be affected.",
            "此操作无法撤销，但之前导出的文件不会受到影响。"),
        warningCard);
    warningText->setObjectName("deleteWarningText");
    warningText->setWordWrap(true);
    warningLayout->addWidget(warningMark, 0, Qt::AlignTop);
    warningLayout->addWidget(warningText, 1);
    root->addWidget(warningCard);

    auto *actions = new QHBoxLayout();
    actions->setSpacing(10);
    actions->addStretch();
    auto *cancelButton = new QPushButton(
        UiLanguage::text("Cancel", "取消"), this);
    cancelButton->setObjectName("deleteReportCancelButton");
    cancelButton->setMinimumWidth(96);
    cancelButton->setDefault(true);
    auto *deleteButton = new QPushButton(
        UiLanguage::text("Delete report", "删除报告"), this);
    deleteButton->setObjectName("deleteReportConfirmButton");
    deleteButton->setMinimumWidth(116);
    actions->addWidget(cancelButton);
    actions->addWidget(deleteButton);
    root->addLayout(actions);

    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    connect(deleteButton, &QPushButton::clicked, this, &QDialog::accept);
    cancelButton->setFocus(Qt::OtherFocusReason);

    setStyleSheet(R"(
        QDialog#deleteReportDialog {
            background: #ffffff;
            color: #172033;
        }
        QLabel#deleteIconBadge {
            background: #fff0f1;
            border: 1px solid #f4c8cd;
            border-radius: 27px;
        }
        QLabel#deleteDialogHeading {
            color: #172033;
            font-size: 20px;
            font-weight: 700;
        }
        QLabel#deleteDialogSubtitle {
            color: #6c778a;
            font-size: 12px;
        }
        QFrame#deleteReportCard {
            background: #f7f9fc;
            border: 1px solid #dfe5ee;
            border-radius: 9px;
        }
        QLabel#deleteReportName {
            color: #253149;
            font-size: 14px;
            font-weight: 600;
        }
        QLabel#deleteReportMeta { color: #738096; font-size: 11px; }
        QFrame#deleteWarningCard {
            background: #fff8ed;
            border: 1px solid #f1d7ad;
            border-radius: 8px;
        }
        QLabel#deleteWarningMark {
            color: #9c611e;
            background: #ffe7bf;
            border-radius: 11px;
            font-weight: 700;
        }
        QLabel#deleteWarningText { color: #785125; }
        QPushButton {
            min-height: 38px;
            padding: 0 16px;
            border-radius: 7px;
            border: 1px solid #d4dbe7;
            background: #ffffff;
            color: #263248;
        }
        QPushButton:hover { background: #f6f8fb; border-color: #aab5c7; }
        QPushButton:focus { border: 2px solid #8eacf4; }
        QPushButton#deleteReportConfirmButton {
            color: #ffffff;
            background: #c53f4a;
            border-color: #c53f4a;
            font-weight: 600;
        }
        QPushButton#deleteReportConfirmButton:hover {
            background: #ac303b;
            border-color: #ac303b;
        }
    )");
}

bool ReportDeleteDialog::confirm(const QString &reportTitle,
                                 const QString &reportType,
                                 const QDate &reportDate,
                                 QWidget *parent)
{
    ReportDeleteDialog dialog(reportTitle, reportType, reportDate, parent);
    return dialog.exec() == QDialog::Accepted;
}
