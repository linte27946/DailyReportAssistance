#pragma once

#include <QString>

class QObject;
class QComboBox;
class QListWidget;
class QTableWidget;
class QTabWidget;
class QWidget;

/// Lightweight runtime localization for the hand-written Qt Widgets UI.
/// English is the source language; Simplified Chinese is the bundled locale.
class UiLanguage {
public:
    static void setLanguage(const QString &languageCode);
    static QString language();
    static bool isChinese();
    static QString text(const QString &english, const QString &chinese);

    static void bindText(QObject *object,
                         const QString &english, const QString &chinese);
    static void bindPlaceholder(QObject *object,
                                const QString &english, const QString &chinese);
    static void bindSubtitle(QObject *object,
                             const QString &english, const QString &chinese);
    static void bindWindowTitle(QObject *object,
                                const QString &english, const QString &chinese);
    static void bindSuffix(QObject *object,
                           const QString &english, const QString &chinese);
    static void bindSpecialValueText(QObject *object,
                                     const QString &english, const QString &chinese);
    static void bindTooltip(QObject *object,
                            const QString &english, const QString &chinese);

    static void bindTab(QTabWidget *tabs, QWidget *page,
                        const QString &english, const QString &chinese);
    static void bindComboItem(QComboBox *combo, int index,
                              const QString &english, const QString &chinese);
    static void bindListItem(QListWidget *list, int index,
                             const QString &english, const QString &chinese);
    static void bindHeader(QTableWidget *table, int column,
                           const QString &english, const QString &chinese);

    static void apply(QObject *root);
    static void applyAll();

private:
    static void bind(QObject *object, const char *role,
                     const QString &english, const QString &chinese);
    static void applyObject(QObject *object);
};
