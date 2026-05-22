#pragma once

#include <QString>
#include <QJsonObject>

class ConfigManager {
public:
    static ConfigManager& instance();

    bool load(const QString& filePath);
    bool save(const QString& filePath) const;

    QJsonObject& data()             { return m_data; }
    const QJsonObject& data() const { return m_data; }

    QJsonObject cameraConfig() const;
    QJsonObject protocolConfig() const;
    QJsonObject imageProcessorConfig() const;

private:
    ConfigManager() = default;
    QJsonObject m_data;
};
