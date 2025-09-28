#include "Settings.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QStandardPaths>

namespace {
QString defaultConfigPath()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QDir().mkpath(dir);
    return dir + QLatin1String("/sunkiss_settings.json");
}

QJsonObject serializeProfile(const RecipeProfile &profile)
{
    QJsonObject obj;
    obj[QStringLiteral("batchLiters")] = profile.batchLiters;
    obj[QStringLiteral("targetPh")] = profile.targetPh;
    obj[QStringLiteral("fertAMlPerL")] = profile.fertAMlPerL;
    obj[QStringLiteral("fertBMlPerL")] = profile.fertBMlPerL;
    obj[QStringLiteral("temperature")] = profile.temperature;
    obj[QStringLiteral("name")] = profile.name;
    return obj;
}

RecipeProfile parseProfile(const QJsonObject &obj)
{
    RecipeProfile profile;
    profile.batchLiters = obj.value(QStringLiteral("batchLiters")).toDouble(profile.batchLiters);
    profile.targetPh = obj.value(QStringLiteral("targetPh")).toDouble(profile.targetPh);
    profile.fertAMlPerL = obj.value(QStringLiteral("fertAMlPerL")).toDouble(profile.fertAMlPerL);
    profile.fertBMlPerL = obj.value(QStringLiteral("fertBMlPerL")).toDouble(profile.fertBMlPerL);
    profile.temperature = obj.value(QStringLiteral("temperature")).toDouble(profile.temperature);
    profile.name = obj.value(QStringLiteral("name")).toString();
    return profile;
}

} // namespace

Settings::Settings(QObject *parent)
    : QObject(parent)
    , m_configPath(defaultConfigPath())
{
}

void Settings::load()
{
    QFile file(m_configPath);
    if (!file.exists()) {
        return;
    }
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }
    const auto json = QJsonDocument::fromJson(file.readAll());
    if (!json.isObject()) {
        return;
    }

    const QJsonArray devicesArray = json.object().value(QStringLiteral("devices")).toArray();
    m_devices.clear();
    for (const QJsonValue &value : devicesArray) {
        const QJsonObject obj = value.toObject();
        DeviceBinding binding;
        binding.portName = obj.value(QStringLiteral("portName")).toString();
        binding.deviceId = obj.value(QStringLiteral("deviceId")).toInt();
        binding.displayName = obj.value(QStringLiteral("displayName")).toString();
        binding.lastProfile = parseProfile(obj.value(QStringLiteral("lastProfile")).toObject());
        m_devices.append(binding);
    }
    emit changed();
}

void Settings::save() const
{
    QJsonArray devicesArray;
    for (const DeviceBinding &binding : m_devices) {
        QJsonObject obj;
        obj[QStringLiteral("portName")] = binding.portName;
        obj[QStringLiteral("deviceId")] = binding.deviceId;
        obj[QStringLiteral("displayName")] = binding.displayName;
        obj[QStringLiteral("lastProfile")] = serializeProfile(binding.lastProfile);
        devicesArray.append(obj);
    }

    QJsonObject root;
    root[QStringLiteral("devices")] = devicesArray;

    QFile file(m_configPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
}

QVector<DeviceBinding> &Settings::devices()
{
    return m_devices;
}

const QVector<DeviceBinding> &Settings::devices() const
{
    return m_devices;
}

void Settings::setConfigPath(const QString &path)
{
    m_configPath = path;
}

QString Settings::configPath() const
{
    return m_configPath;
}
