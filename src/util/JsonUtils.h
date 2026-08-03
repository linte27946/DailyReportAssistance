#pragma once

#include <QString>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QByteArray>
#include <nlohmann/json.hpp>

/// Convenience helpers for JSON conversion between Qt and nlohmann types.
namespace JsonUtils {

/// Convert QJsonDocument to a compact JSON string.
inline QString toCompactString(const QJsonDocument &doc)
{
    return QString::fromUtf8(doc.toJson(QJsonDocument::Compact));
}

/// Convert QJsonDocument to an indented JSON string.
inline QString toPrettyString(const QJsonDocument &doc)
{
    return QString::fromUtf8(doc.toJson(QJsonDocument::Indented));
}

/// Parse JSON string to QJsonDocument, returning null on failure.
inline QJsonDocument parse(const QString &json)
{
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8(), &error);
    if (error.error != QJsonParseError::NoError) {
        return QJsonDocument();
    }
    return doc;
}

/// Convert a QJsonObject to a compact JSON string.
inline QString toString(const QJsonObject &obj)
{
    QJsonDocument doc(obj);
    return toCompactString(doc);
}

/// Convert a QJsonArray to a compact JSON string.
inline QString toString(const QJsonArray &arr)
{
    QJsonDocument doc(arr);
    return toCompactString(doc);
}

/// Try to get a string value from a QJsonObject, returning a default if missing.
inline QString getString(const QJsonObject &obj, const QString &key, const QString &defaultVal = {})
{
    if (obj.contains(key) && obj[key].isString())
        return obj[key].toString();
    return defaultVal;
}

/// Try to get an int value from a QJsonObject, returning a default if missing.
inline int getInt(const QJsonObject &obj, const QString &key, int defaultVal = 0)
{
    if (obj.contains(key))
        return obj[key].toInt(defaultVal);
    return defaultVal;
}

/// Try to get a bool value from a QJsonObject, returning a default if missing.
inline bool getBool(const QJsonObject &obj, const QString &key, bool defaultVal = false)
{
    if (obj.contains(key) && obj[key].isBool())
        return obj[key].toBool();
    return defaultVal;
}

} // namespace JsonUtils
