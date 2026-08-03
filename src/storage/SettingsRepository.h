#pragma once

#include <QString>
#include <QVariant>
#include <QJsonObject>
#include <QJsonDocument>
#include <QSqlQuery>
#include <QSqlError>
#include <spdlog/spdlog.h>
#include "Database.h"

/// Key-value settings store backed by SQLite.
class SettingsRepository {
public:
    SettingsRepository() = default;

    /// Get a setting value by key, with a default if not found.
    QString getValue(const QString &key, const QString &defaultVal = {})
    {
        auto db = Database::instance().connection();
        QSqlQuery query(db);
        query.prepare("SELECT value FROM app_settings WHERE key = :key");
        query.bindValue(":key", key);

        if (query.exec() && query.next()) {
            return query.value("value").toString();
        }
        return defaultVal;
    }

    /// Set a setting value.
    bool setValue(const QString &key, const QString &value)
    {
        auto db = Database::instance().connection();
        QSqlQuery query(db);
        query.prepare(
            "INSERT INTO app_settings (key, value, updated_at) "
            "VALUES (:key, :value, datetime('now')) "
            "ON CONFLICT(key) DO UPDATE SET value = :value2, updated_at = datetime('now')"
        );
        query.bindValue(":key", key);
        query.bindValue(":value", value);
        query.bindValue(":value2", value);

        if (!query.exec()) {
            spdlog::error("Failed to save setting '{}': {}",
                          key.toStdString(), query.lastError().text().toStdString());
            return false;
        }
        return true;
    }

    /// Get a JSON object setting.
    QJsonObject getJson(const QString &key)
    {
        QString raw = getValue(key, "{}");
        QJsonDocument doc = QJsonDocument::fromJson(raw.toUtf8());
        if (doc.isObject()) return doc.object();
        return {};
    }

    /// Set a JSON object setting.
    bool setJson(const QString &key, const QJsonObject &obj)
    {
        QJsonDocument doc(obj);
        return setValue(key, QString::fromUtf8(doc.toJson(QJsonDocument::Compact)));
    }

    /// Get a boolean setting.
    bool getBool(const QString &key, bool defaultVal = false)
    {
        QString val = getValue(key, defaultVal ? "true" : "false");
        return val.toLower() == "true" || val == "1";
    }

    /// Set a boolean setting.
    bool setBool(const QString &key, bool value)
    {
        return setValue(key, value ? "true" : "false");
    }

    /// Get an integer setting.
    int getInt(const QString &key, int defaultVal = 0)
    {
        return getValue(key, QString::number(defaultVal)).toInt();
    }

    /// Set an integer setting.
    bool setInt(const QString &key, int value)
    {
        return setValue(key, QString::number(value));
    }

    /// Get all settings as a flat key-value map.
    QMap<QString, QString> allSettings()
    {
        QMap<QString, QString> result;
        auto db = Database::instance().connection();
        QSqlQuery query(db);
        query.exec("SELECT key, value FROM app_settings ORDER BY key");

        while (query.next()) {
            result[query.value("key").toString()] = query.value("value").toString();
        }
        return result;
    }

    /// Remove a setting.
    bool remove(const QString &key)
    {
        auto db = Database::instance().connection();
        QSqlQuery query(db);
        query.prepare("DELETE FROM app_settings WHERE key = :key");
        query.bindValue(":key", key);

        if (!query.exec()) {
            spdlog::error("Failed to remove setting '{}': {}",
                          key.toStdString(), query.lastError().text().toStdString());
            return false;
        }
        return true;
    }
};
