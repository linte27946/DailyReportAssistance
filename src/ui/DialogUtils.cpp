#include "DialogUtils.h"

#include "UiLanguage.h"
#include <QApplication>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QPushButton>
#include <QWidget>

namespace {

QString dialogStyleSheet()
{
    return QStringLiteral(R"(
        QDialog, QMessageBox, QFileDialog, QWizard {
            background: #f7f9fc;
            color: #172033;
            font-size: 13px;
        }
        QMessageBox { min-width: 460px; }
        QMessageBox QLabel#qt_msgbox_label {
            min-width: 390px;
            color: #263248;
            line-height: 1.35;
        }
        QPushButton {
            min-height: 36px;
            padding: 0 16px;
            border-radius: 7px;
            border: 1px solid #d4dbe7;
            background: #ffffff;
            color: #263248;
        }
        QPushButton:hover { border-color: #9daac0; background: #f5f8fd; }
        QPushButton:focus { border: 2px solid #8eacf4; }
        QPushButton#primaryButton {
            background: #356ae6;
            border-color: #356ae6;
            color: #ffffff;
            font-weight: 600;
        }
        QPushButton#primaryButton:hover { background: #2859cf; }
        QPushButton#dangerButton {
            color: #ffffff;
            background: #c94650;
            border-color: #c94650;
            font-weight: 600;
        }
        QPushButton#dangerButton:hover { background: #ad3540; }
        QLineEdit, QComboBox {
            min-height: 32px;
            border: 1px solid #d4dbe7;
            border-radius: 6px;
            background: #ffffff;
            padding: 0 8px;
        }
        QListView, QTreeView {
            background: #ffffff;
            border: 1px solid #dfe5ee;
            border-radius: 7px;
            alternate-background-color: #f8faff;
            selection-background-color: #dce8ff;
            selection-color: #20304d;
        }
        QHeaderView::section {
            background: #f0f3f8;
            color: #4c586c;
            border: none;
            border-bottom: 1px solid #dfe5ee;
            padding: 7px;
        }
        QWizardPage { background: #f7f9fc; }
        QWizard QLabel { color: #344157; }
    )");
}

void showMessage(QWidget *parent, QMessageBox::Icon icon,
                 const QString &title, const QString &message)
{
    QMessageBox box(icon, title, message, QMessageBox::NoButton, parent);
    box.setObjectName("appMessageBox");
    box.setWindowIcon(QApplication::windowIcon());
    box.setTextFormat(Qt::PlainText);
    box.setTextInteractionFlags(Qt::TextSelectableByMouse);
    auto *okButton = box.addButton(
        UiLanguage::text("OK", "确定"), QMessageBox::AcceptRole);
    okButton->setObjectName("primaryButton");
    box.setDefaultButton(okButton);
    DialogUtils::applyStyle(&box);
    box.exec();
}

} // namespace

void DialogUtils::applyStyle(QWidget *dialog)
{
    if (!dialog) return;
    dialog->setWindowIcon(QApplication::windowIcon());
    dialog->setStyleSheet(dialogStyleSheet());
}

void DialogUtils::information(QWidget *parent, const QString &title,
                              const QString &message)
{
    showMessage(parent, QMessageBox::Information, title, message);
}

void DialogUtils::warning(QWidget *parent, const QString &title,
                          const QString &message)
{
    showMessage(parent, QMessageBox::Warning, title, message);
}

void DialogUtils::critical(QWidget *parent, const QString &title,
                           const QString &message)
{
    showMessage(parent, QMessageBox::Critical, title, message);
}

bool DialogUtils::confirm(QWidget *parent, const QString &title,
                          const QString &message, const QString &confirmText,
                          bool destructive)
{
    QMessageBox box(destructive ? QMessageBox::Warning
                                : QMessageBox::Question,
                    title, message, QMessageBox::NoButton, parent);
    box.setObjectName("appMessageBox");
    box.setWindowIcon(QApplication::windowIcon());
    box.setTextFormat(Qt::PlainText);
    box.setTextInteractionFlags(Qt::TextSelectableByMouse);

    auto *cancelButton = box.addButton(
        UiLanguage::text("Cancel", "取消"), QMessageBox::RejectRole);
    auto *confirmButton = box.addButton(
        confirmText,
        destructive ? QMessageBox::DestructiveRole : QMessageBox::AcceptRole);
    confirmButton->setObjectName(destructive ? "dangerButton" : "primaryButton");
    box.setDefaultButton(cancelButton);
    box.setEscapeButton(cancelButton);
    applyStyle(&box);
    box.exec();
    return box.clickedButton() == confirmButton;
}

QString DialogUtils::selectDirectory(QWidget *parent, const QString &title,
                                     const QString &initialDirectory)
{
    QFileDialog dialog(parent, title,
        initialDirectory.isEmpty() ? QDir::homePath() : initialDirectory);
    dialog.setObjectName("appFileDialog");
    dialog.setOption(QFileDialog::DontUseNativeDialog, true);
    dialog.setOption(QFileDialog::ShowDirsOnly, true);
    dialog.setFileMode(QFileDialog::Directory);
    dialog.setAcceptMode(QFileDialog::AcceptOpen);
    dialog.setLabelText(QFileDialog::Accept,
                        UiLanguage::text("Select folder", "选择文件夹"));
    dialog.resize(820, 560);
    applyStyle(&dialog);
    if (dialog.exec() != QDialog::Accepted || dialog.selectedFiles().isEmpty())
        return {};
    return dialog.selectedFiles().constFirst();
}

QString DialogUtils::saveFile(QWidget *parent, const QString &title,
                              const QString &suggestedName,
                              const QString &nameFilters)
{
    QFileDialog dialog(parent, title);
    dialog.setObjectName("appFileDialog");
    dialog.setOption(QFileDialog::DontUseNativeDialog, true);
    dialog.setAcceptMode(QFileDialog::AcceptSave);
    dialog.setFileMode(QFileDialog::AnyFile);
    dialog.setNameFilters(nameFilters.split(";;", Qt::SkipEmptyParts));
    dialog.selectFile(suggestedName);
    const QString defaultSuffix = QFileInfo(suggestedName).suffix();
    if (!defaultSuffix.isEmpty())
        dialog.setDefaultSuffix(defaultSuffix);
    dialog.setLabelText(QFileDialog::Accept,
                        UiLanguage::text("Save", "保存"));
    dialog.resize(820, 560);
    applyStyle(&dialog);
    if (dialog.exec() != QDialog::Accepted || dialog.selectedFiles().isEmpty())
        return {};
    return dialog.selectedFiles().constFirst();
}
