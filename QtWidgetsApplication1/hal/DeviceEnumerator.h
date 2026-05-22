#pragma once

#include <QObject>
#include <QString>
#include <QList>
#include <memory>
#include <functional>

struct libusb_context;
struct uvc_context;
struct uvc_device;

class DeviceEnumerator : public QObject {
    Q_OBJECT

public:
    static DeviceEnumerator& instance();

    bool initialize();
    void shutdown();
    bool isInitialized() const { return m_ctx != nullptr; }

    struct DeviceEntry {
        QString name;
        uint16_t vid = 0;
        uint16_t pid = 0;
        QString serial;
        uint8_t bus = 0;
        uint8_t address = 0;
        uvc_device* raw_device = nullptr;
    };

    QList<DeviceEntry> enumerate();

    uvc_context* context() const { return m_ctx; }

signals:
    void deviceConnected(const QString& name);
    void deviceDisconnected(const QString& name);

private:
    DeviceEnumerator() = default;
    ~DeviceEnumerator();
    DeviceEnumerator(const DeviceEnumerator&) = delete;
    DeviceEnumerator& operator=(const DeviceEnumerator&) = delete;

    uvc_context* m_ctx = nullptr;
    bool m_initialized = false;
};
