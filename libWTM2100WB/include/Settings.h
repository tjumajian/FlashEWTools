#ifndef SETTINGS_H
#define SETTINGS_H

#include <QJsonObject>
#include <QVariant>

class Settings
{
public:
    Settings();
    Settings(const QString &filename);

    bool hasValue(const QString &key) const;
    bool hasValue(const QString &group, const QString &key) const;

    QVariant value(const QString &key) const;
    void setValue(const QString &key, const QVariant &value);

    QVariant value(const QString &group, const QString &key) const;
    void setValue(const QString &group, const QString &key, const QVariant &value);

    void reload();
    void save();
    void changeSettingsFile(const QString &filename);
    QString filename() const;

    static Settings *staticInstance();

private:
    static Settings *m_staticInstance;
    QJsonObject m_settingsData;
    QString m_filename;
};

#endif // SETTINGS_H
