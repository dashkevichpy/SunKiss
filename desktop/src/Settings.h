#pragma once

#include "RecipeProfile.h"

#include <QObject>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QVector>

struct DeviceBinding {
    QString portName;
    int deviceId = 0;
    QString displayName;
    RecipeProfile lastProfile;
};

class Settings : public QObject
{
    Q_OBJECT
public:
    explicit Settings(QObject *parent = nullptr);

    void load();
    void save() const;

    QVector<DeviceBinding> &devices();
    const QVector<DeviceBinding> &devices() const;

    void setConfigPath(const QString &path);
    QString configPath() const;

signals:
    void changed();

private:
    QString m_configPath;
    QVector<DeviceBinding> m_devices;
};
