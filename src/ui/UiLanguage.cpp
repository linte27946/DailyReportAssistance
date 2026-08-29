#include "UiLanguage.h"

#include <QAbstractButton>
#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QSpinBox>
#include <QSystemTrayIcon>
#include <QTabWidget>
#include <QTableWidget>
#include <QTextEdit>
#include <QWizardPage>

namespace {

QString g_language = "en";

constexpr auto EnglishProperty = "uiLanguageEnglish";
constexpr auto ChineseProperty = "uiLanguageChinese";
constexpr auto RoleProperty = "uiLanguageRole";
constexpr auto TabEnglishProperty = "uiTabEnglish";
constexpr auto TabChineseProperty = "uiTabChinese";
constexpr int ItemEnglishRole = Qt::UserRole + 51;
constexpr int ItemChineseRole = Qt::UserRole + 52;

QString localized(const QObject *object)
{
    return UiLanguage::isChinese()
        ? object->property(ChineseProperty).toString()
        : object->property(EnglishProperty).toString();
}

QString itemText(const QVariant &english, const QVariant &chinese)
{
    return UiLanguage::isChinese() ? chinese.toString() : english.toString();
}

} // namespace

void UiLanguage::setLanguage(const QString &languageCode)
{
    g_language = languageCode.startsWith("zh", Qt::CaseInsensitive)
        ? "zh-CN" : "en";
}

QString UiLanguage::language()
{
    return g_language;
}

bool UiLanguage::isChinese()
{
    return g_language == "zh-CN";
}

QString UiLanguage::text(const QString &english, const QString &chinese)
{
    return isChinese() ? chinese : english;
}

void UiLanguage::bind(QObject *object, const char *role,
                      const QString &english, const QString &chinese)
{
    if (!object) return;
    object->setProperty(EnglishProperty, english);
    object->setProperty(ChineseProperty, chinese);
    object->setProperty(RoleProperty, role);
    applyObject(object);
}

void UiLanguage::bindText(QObject *object,
                          const QString &english, const QString &chinese)
{
    bind(object, "text", english, chinese);
}

void UiLanguage::bindPlaceholder(QObject *object,
                                 const QString &english, const QString &chinese)
{
    bind(object, "placeholder", english, chinese);
}

void UiLanguage::bindSubtitle(QObject *object,
                              const QString &english, const QString &chinese)
{
    bind(object, "subtitle", english, chinese);
}

void UiLanguage::bindWindowTitle(QObject *object,
                                 const QString &english, const QString &chinese)
{
    bind(object, "windowTitle", english, chinese);
}

void UiLanguage::bindSuffix(QObject *object,
                            const QString &english, const QString &chinese)
{
    bind(object, "suffix", english, chinese);
}

void UiLanguage::bindSpecialValueText(QObject *object,
                                      const QString &english, const QString &chinese)
{
    bind(object, "specialValueText", english, chinese);
}

void UiLanguage::bindTooltip(QObject *object,
                             const QString &english, const QString &chinese)
{
    bind(object, "tooltip", english, chinese);
}

void UiLanguage::bindTab(QTabWidget *tabs, QWidget *page,
                         const QString &english, const QString &chinese)
{
    if (!tabs || !page) return;
    page->setProperty(TabEnglishProperty, english);
    page->setProperty(TabChineseProperty, chinese);
    const int index = tabs->indexOf(page);
    if (index >= 0) tabs->setTabText(index, text(english, chinese));
}

void UiLanguage::bindComboItem(QComboBox *combo, int index,
                               const QString &english, const QString &chinese)
{
    if (!combo || index < 0 || index >= combo->count()) return;
    combo->setItemData(index, english, ItemEnglishRole);
    combo->setItemData(index, chinese, ItemChineseRole);
    combo->setItemText(index, text(english, chinese));
}

void UiLanguage::bindListItem(QListWidget *list, int index,
                              const QString &english, const QString &chinese)
{
    if (!list || index < 0 || index >= list->count()) return;
    auto *item = list->item(index);
    item->setData(ItemEnglishRole, english);
    item->setData(ItemChineseRole, chinese);
    item->setText(text(english, chinese));
}

void UiLanguage::bindHeader(QTableWidget *table, int column,
                            const QString &english, const QString &chinese)
{
    if (!table || column < 0 || column >= table->columnCount()) return;
    auto *item = table->horizontalHeaderItem(column);
    if (!item) {
        item = new QTableWidgetItem();
        table->setHorizontalHeaderItem(column, item);
    }
    item->setData(ItemEnglishRole, english);
    item->setData(ItemChineseRole, chinese);
    item->setText(text(english, chinese));
}

void UiLanguage::applyObject(QObject *object)
{
    if (!object || !object->property(RoleProperty).isValid()) return;
    const QString value = localized(object);
    const QString role = object->property(RoleProperty).toString();

    if (role == "text") {
        if (auto *action = qobject_cast<QAction *>(object)) action->setText(value);
        else if (auto *menu = qobject_cast<QMenu *>(object)) menu->setTitle(value);
        else if (auto *page = qobject_cast<QWizardPage *>(object)) page->setTitle(value);
        else if (auto *group = qobject_cast<QGroupBox *>(object)) group->setTitle(value);
        else if (auto *button = qobject_cast<QAbstractButton *>(object)) button->setText(value);
        else if (auto *label = qobject_cast<QLabel *>(object)) label->setText(value);
    } else if (role == "placeholder") {
        if (auto *lineEdit = qobject_cast<QLineEdit *>(object)) lineEdit->setPlaceholderText(value);
        else if (auto *textEdit = qobject_cast<QTextEdit *>(object)) textEdit->setPlaceholderText(value);
    } else if (role == "subtitle") {
        if (auto *page = qobject_cast<QWizardPage *>(object)) page->setSubTitle(value);
    } else if (role == "windowTitle") {
        if (auto *widget = qobject_cast<QWidget *>(object)) widget->setWindowTitle(value);
    } else if (role == "suffix") {
        if (auto *spin = qobject_cast<QSpinBox *>(object)) spin->setSuffix(value);
    } else if (role == "specialValueText") {
        if (auto *spin = qobject_cast<QSpinBox *>(object)) spin->setSpecialValueText(value);
    } else if (role == "tooltip") {
        if (auto *tray = qobject_cast<QSystemTrayIcon *>(object)) tray->setToolTip(value);
        else if (auto *widget = qobject_cast<QWidget *>(object)) widget->setToolTip(value);
        else if (auto *action = qobject_cast<QAction *>(object)) action->setToolTip(value);
    }
}

void UiLanguage::apply(QObject *root)
{
    if (!root) return;
    applyObject(root);

    if (auto *tabs = qobject_cast<QTabWidget *>(root)) {
        for (int i = 0; i < tabs->count(); ++i) {
            QWidget *page = tabs->widget(i);
            const QString english = page->property(TabEnglishProperty).toString();
            const QString chinese = page->property(TabChineseProperty).toString();
            if (!english.isEmpty() || !chinese.isEmpty())
                tabs->setTabText(i, text(english, chinese));
        }
    }
    if (auto *combo = qobject_cast<QComboBox *>(root)) {
        for (int i = 0; i < combo->count(); ++i) {
            const QVariant english = combo->itemData(i, ItemEnglishRole);
            const QVariant chinese = combo->itemData(i, ItemChineseRole);
            if (english.isValid() || chinese.isValid())
                combo->setItemText(i, itemText(english, chinese));
        }
    }
    if (auto *list = qobject_cast<QListWidget *>(root)) {
        for (int i = 0; i < list->count(); ++i) {
            auto *item = list->item(i);
            const QVariant english = item->data(ItemEnglishRole);
            const QVariant chinese = item->data(ItemChineseRole);
            if (english.isValid() || chinese.isValid())
                item->setText(itemText(english, chinese));
        }
    }
    if (auto *table = qobject_cast<QTableWidget *>(root)) {
        for (int column = 0; column < table->columnCount(); ++column) {
            auto *item = table->horizontalHeaderItem(column);
            if (!item) continue;
            const QVariant english = item->data(ItemEnglishRole);
            const QVariant chinese = item->data(ItemChineseRole);
            if (english.isValid() || chinese.isValid())
                item->setText(itemText(english, chinese));
        }
    }

    for (QObject *child : root->children()) apply(child);
}

void UiLanguage::applyAll()
{
    apply(qApp);
    for (QWidget *widget : QApplication::topLevelWidgets()) apply(widget);
}
