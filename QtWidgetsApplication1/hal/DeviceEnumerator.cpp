#include "DeviceEnumerator.h"
#include "infra/LogManager.h"
#include <libuvc/libuvc.h>

DeviceEnumerator& DeviceEnumerator::instance() {
    static DeviceEnumerator inst;
    return inst;
}

DeviceEnumerator::~DeviceEnumerator() {
    shutdown();
}

bool DeviceEnumerator::initialize() {
    if (m_initialized) return true;

    uvc_error_t res = uvc_init(&m_ctx, nullptr);
    if (res != UVC_SUCCESS) {
        LOG_ERROR(QString("uvc_init failed: %1").arg(uvc_strerror(res)));
        return false;
    }

    m_initialized = true;
    LOG_INFO("libuvc context initialized");
    return true;
}

void DeviceEnumerator::shutdown() {
    if (m_ctx) {
        uvc_exit(m_ctx);
        m_ctx = nullptr;
        LOG_INFO("libuvc context shutdown");
    }
    m_initialized = false;
}

QList<DeviceEnumerator::DeviceEntry> DeviceEnumerator::enumerate() {
    QList<DeviceEntry> result;

    if (!m_ctx) {
        LOG_ERROR("Enumerate called before uvc_init");
        return result;
    }

    uvc_device_t** list = nullptr;
    uvc_error_t res = uvc_get_device_list(m_ctx, &list);
    if (res != UVC_SUCCESS) {
        LOG_ERROR(QString("uvc_get_device_list failed: %1").arg(uvc_strerror(res)));
        return result;
    }

    for (int i = 0; list[i] != nullptr; i++) {
        uvc_device_descriptor_t* desc = nullptr;
        if (uvc_get_device_descriptor(list[i], &desc) != UVC_SUCCESS)
            continue;

        // Detect how many UVC camera channels (VC interfaces) this device has
        int cameraCount = uvc_get_camera_count(list[i]);
        if (cameraCount < 1) cameraCount = 1;

        for (int ch = 0; ch < cameraCount; ch++) {
            DeviceEntry entry;
            entry.vid = desc->idVendor;
            entry.pid = desc->idProduct;

            if (desc->product) {
                entry.name = QString::fromUtf8(desc->product);
            } else {
                entry.name = QString("USB Camera %1:%2")
                    .arg(desc->idVendor, 4, 16, QChar('0'))
                    .arg(desc->idProduct, 4, 16, QChar('0'));
            }
            if (desc->manufacturer)
                entry.name = QString::fromUtf8(desc->manufacturer) + " " + entry.name;

            // Append channel suffix for multi-channel devices
            if (cameraCount > 1)
                entry.name += QString(" [CH%1]").arg(ch);

            entry.serial = desc->serialNumber ? QString::fromUtf8(desc->serialNumber) : QString();
            entry.bus = uvc_get_bus_number(list[i]);
            entry.address = uvc_get_device_address(list[i]);
            entry.cameraIndex = ch;
            entry.cameraCount = cameraCount;

            // Keep a reference so the device pointer stays valid
            uvc_ref_device(list[i]);
            entry.raw_device = list[i];

            result.append(entry);
        }

        uvc_free_device_descriptor(desc);
    }

    uvc_free_device_list(list, 0);
    LOG_INFO(QString("Found %1 UVC device(s)").arg(result.size()));
    return result;
}
