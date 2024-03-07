#include "../include/Settings.h"
#include <QCoreApplication>
#include <QDir>
#include <QJsonDocument>

#ifndef SETTINGS_PATH
#define SETTINGS_PATH "settings.json"
#endif

Settings *Settings::m_staticInstance = nullptr;

Settings::Settings()
{
    QDir dir(QCoreApplication::instance()->applicationDirPath());
    m_filename = dir.absoluteFilePath(SETTINGS_PATH);
    if (!QFile::exists(m_filename))
    {
        m_filename = SETTINGS_PATH;
    }
    reload();
}

Settings::Settings(const QString &filename)
{
    m_filename = filename;
    reload();
}

bool Settings::hasValue(const QString &key) const
{
    return m_settingsData.contains(key);
}

bool Settings::hasValue(const QString &group, const QString &key) const
{
    if (!m_settingsData.contains(group))
    {
        return false;
    }
    QJsonObject obj = m_settingsData.value(group).toObject();
    return obj.contains(key);
}

QVariant Settings::value(const QString &key) const
{
    return m_settingsData.value(key).toVariant();
}

void Settings::setValue(const QString &key, const QVariant &value)
{
    QStringList keys = key.split(".");
    m_settingsData[key] = QJsonValue::fromVariant(value);
}

QVariant Settings::value(const QString &group, const QString &key) const
{
    QJsonObject obj = m_settingsData.value(group).toObject();
    return obj.value(key).toVariant();
}

void Settings::setValue(const QString &group, const QString &key, const QVariant &value)
{
    QJsonObject obj = m_settingsData.value(group).toObject();
    obj[key] = QJsonValue::fromVariant(value);
    m_settingsData[group] = obj;
}

void Settings::reload()
{
    QFile file(m_filename);
    if (file.open(QIODevice::ReadOnly))
    {
        QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        m_settingsData = doc.object();
    }
    //qDebug() << m_settingsData;
}

void Settings::save()
{
    QJsonDocument doc;
    doc.setObject(m_settingsData);
    QFile file(m_filename);
    if (file.open(QIODevice::WriteOnly))
    {
        file.write(doc.toJson(QJsonDocument::Indented));
    }
}

void Settings::changeSettingsFile(const QString &filename)
{
    m_filename = filename;
    reload();
}

QString Settings::filename() const
{
    return m_filename;
}

Settings *Settings::staticInstance()
{
    if (m_staticInstance == nullptr)
    {
        m_staticInstance = new Settings;
    }
    return m_staticInstance;
}
