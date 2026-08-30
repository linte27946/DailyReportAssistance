#pragma once

#include <QString>

class QWidget;

/// Consistent, bilingual modal dialogs used throughout the application.
class DialogUtils {
public:
    static void applyStyle(QWidget *dialog);

    static void information(QWidget *parent, const QString &title,
                            const QString &message);
    static void warning(QWidget *parent, const QString &title,
                        const QString &message);
    static void critical(QWidget *parent, const QString &title,
                         const QString &message);

    static bool confirm(QWidget *parent, const QString &title,
                        const QString &message, const QString &confirmText,
                        bool destructive = false);

    static QString selectDirectory(QWidget *parent, const QString &title,
                                   const QString &initialDirectory = {});
    static QString saveFile(QWidget *parent, const QString &title,
                            const QString &suggestedName,
                            const QString &nameFilters);
};
