#include "ConfigManager.h"
#include <QFile>
#include <QJsonDocument>

ConfigManager& ConfigManager::instance() {
    static ConfigManager inst;
    return inst;
}

bool ConfigManager::load(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
        return false;

    QJsonParseError error;
    m_data = QJsonDocument::fromJson(file.readAll(), &error).object();
    return error.error == QJsonParseError::NoError;
}

bool ConfigManager::save(const QString& filePath) const {
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly))
        return false;

    QJsonDocument doc(m_data);
    file.write(doc.toJson(QJsonDocument::Indented));
    return true;
}

QJsonObject ConfigManager::cameraConfig() const {
    return m_data.value("camera").toObject();
}

QJsonObject ConfigManager::protocolConfig() const {
    return m_data.value("protocol").toObject();
}

QJsonObject ConfigManager::imageProcessorConfig() const {
    return m_data.value("image_processor").toObject();
}
