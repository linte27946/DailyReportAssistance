#pragma once

#include <QDate>
#include <QDialog>
#include <QString>

/// Purpose-built confirmation dialog for deleting one generated report.
/// It deliberately avoids the cramped native QMessageBox presentation and
/// makes the affected report and irreversible action explicit.
class ReportDeleteDialog final : public QDialog {
public:
    explicit ReportDeleteDialog(const QString &reportTitle,
                                const QString &reportType,
                                const QDate &reportDate,
                                QWidget *parent = nullptr);

    static bool confirm(const QString &reportTitle,
                        const QString &reportType,
                        const QDate &reportDate,
                        QWidget *parent = nullptr);
};
