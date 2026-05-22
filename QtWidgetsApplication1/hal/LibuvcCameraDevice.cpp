#include "LibuvcCameraDevice.h"
#include "DeviceEnumerator.h"
#include "infra/LogManager.h"
#include <libuvc/libuvc.h>
#include <cstring>

LibuvcCameraDevice::LibuvcCameraDevice(QObject* parent)
    : QObject(parent) {}

LibuvcCameraDevice::~LibuvcCameraDevice() {
    stopStreaming();
    close();
}

void LibuvcCameraDevice::setRawDevice(struct uvc_device* dev) {
    m_rawDevice = dev;
}

// ── ICameraDevice ──

bool LibuvcCameraDevice::open() {
    if (m_opened) return true;
    if (!m_rawDevice) {
        LOG_ERROR("No device set");
        return false;
    }

    uvc_error_t res = uvc_open(m_rawDevice, &m_devh);
    if (res != UVC_SUCCESS) {
        QString errMsg = QString::fromUtf8(uvc_strerror(res));
        m_lastError = errMsg;
        LOG_ERROR(QString("uvc_open failed: %1").arg(errMsg));

        if (res == UVC_ERROR_ACCESS) {
            LOG_ERROR("This usually means the device driver is not WinUSB/libusbK.");
            LOG_ERROR("Use Zadig (https://zadig.akeo.ie) to replace the driver for this device.");
        }
        return false;
    }

    uvc_device_descriptor_t* desc = nullptr;
    if (uvc_get_device_descriptor(m_rawDevice, &desc) == UVC_SUCCESS) {
        m_deviceInfo.vendor_id = desc->idVendor;
        m_deviceInfo.product_id = desc->idProduct;
        if (desc->product)
            m_deviceInfo.name = QString::fromUtf8(desc->product).toStdString();
        if (desc->serialNumber)
            m_deviceInfo.serial_number = QString::fromUtf8(desc->serialNumber).toStdString();
        m_deviceInfo.bus_location = QString("%1:%2")
            .arg(uvc_get_bus_number(m_rawDevice))
            .arg(uvc_get_device_address(m_rawDevice)).toStdString();
        uvc_free_device_descriptor(desc);
    }

    // Enumerate supported formats
    m_deviceInfo.supported_formats.clear();
    const uvc_format_desc_t* fmtDesc = uvc_get_format_descs(m_devh);
    while (fmtDesc) {
        for (auto* frame = fmtDesc->frame_descs; frame; frame = frame->next) {
            CameraFormat cf;
            cf.width = frame->wWidth;
            cf.height = frame->wHeight;
            cf.fps = frame->dwDefaultFrameInterval > 0
                ? static_cast<uint32_t>(10000000.0 / frame->dwDefaultFrameInterval) : 30;
            memcpy(&cf.fourcc, fmtDesc->fourccFormat, 4);
            cf.bFormatIndex = fmtDesc->bFormatIndex;
            cf.bFrameIndex  = frame->bFrameIndex;
            cf.bDescriptorSubtype = fmtDesc->bDescriptorSubtype;
            cf.description = QString("%1x%2 @%3fps")
                .arg(cf.width).arg(cf.height).arg(cf.fps).toStdString();
            m_deviceInfo.supported_formats.push_back(cf);
        }
        fmtDesc = fmtDesc->next;
    }

    m_opened = true;
    LOG_INFO(QString("Camera opened: %1").arg(QString::fromStdString(m_deviceInfo.name)));
    return true;
}

void LibuvcCameraDevice::close() {
    stopStreaming();
    if (m_devh) {
        uvc_close(m_devh);
        m_devh = nullptr;
    }
    m_opened = false;
}

bool LibuvcCameraDevice::isOpen() const { return m_opened; }

std::vector<CameraFormat> LibuvcCameraDevice::getSupportedFormats() const {
    return m_deviceInfo.supported_formats;
}

bool LibuvcCameraDevice::setFormat(const CameraFormat& format) {
    m_currentFormat = format;
    return true;
}

CameraFormat LibuvcCameraDevice::getCurrentFormat() const { return m_currentFormat; }

bool LibuvcCameraDevice::startStreaming() {
    if (m_streaming) return true;
    if (!m_devh) return false;

    uvc_frame_format uvcFmt = UVC_FRAME_FORMAT_ANY;
    uint8_t subtype = m_currentFormat.bDescriptorSubtype;
    std::string fourcc(reinterpret_cast<const char*>(&m_currentFormat.fourcc), 4);

    // Filter non-printable chars from fourcc for comparison
    auto fourccTrim = [](const std::string& s) {
        std::string r;
        for (char c : s) if (c >= 32 && c < 127) r += c;
        return r;
    };
    std::string fc = fourccTrim(fourcc);

    // Map UVC descriptor subtype + FOURCC to libuvc frame format
    if (subtype == 0x06) { // UVC_VS_FORMAT_MJPEG
        uvcFmt = UVC_FRAME_FORMAT_MJPEG;
    } else if (subtype == 0x10) { // UVC_VS_FORMAT_FRAME_BASED
        uvcFmt = UVC_FRAME_FORMAT_H264;
    } else if (subtype == 0x04) { // UVC_VS_FORMAT_UNCOMPRESSED
        if (fc.find("YUYV") != std::string::npos || fc == "YUY2")
            uvcFmt = UVC_FRAME_FORMAT_YUYV;
        else if (fc == "UYVY")
            uvcFmt = UVC_FRAME_FORMAT_UYVY;
        else if (fc == "NV12")
            uvcFmt = UVC_FRAME_FORMAT_NV12;
        else if (fc == "P010")
            uvcFmt = UVC_FRAME_FORMAT_P010;
        else if (fc.find("Y16") != std::string::npos || fc == "Y16")
            uvcFmt = UVC_FRAME_FORMAT_GRAY16;
        else if (fc.find("Y80") != std::string::npos || fc.find("Y8") != std::string::npos || fc == "Y800")
            uvcFmt = UVC_FRAME_FORMAT_GRAY8;
        else if (fc.find("RGB") != std::string::npos)
            uvcFmt = UVC_FRAME_FORMAT_RGB;
        else if (fc.find("BGR") != std::string::npos)
            uvcFmt = UVC_FRAME_FORMAT_BGR;
        else if (fc == "BY8" || fc.find("BY8") != std::string::npos)
            uvcFmt = UVC_FRAME_FORMAT_BY8;
        else if (fc == "BA81")
            uvcFmt = UVC_FRAME_FORMAT_BA81;
        else
            uvcFmt = UVC_FRAME_FORMAT_UNCOMPRESSED; // generic fallback
    } else if (subtype == 0x12) { // UVC_VS_FORMAT_STREAM_BASED
        uvcFmt = UVC_FRAME_FORMAT_ANY;
    } else {
        // Unknown subtype, try FOURCC heuristics
        if (fc.find("MJPG") != std::string::npos)
            uvcFmt = UVC_FRAME_FORMAT_MJPEG;
        else if (fc.find("YUYV") != std::string::npos)
            uvcFmt = UVC_FRAME_FORMAT_YUYV;
        else
            uvcFmt = UVC_FRAME_FORMAT_ANY;
    }

    auto* ctrl = new struct uvc_stream_ctrl();
    memset(ctrl, 0, sizeof(struct uvc_stream_ctrl));

    int width  = static_cast<int>(m_currentFormat.width);
    int height = static_cast<int>(m_currentFormat.height);
    int fps    = static_cast<int>(m_currentFormat.fps);

    // Some UVC cameras need a dummy probe first to initialize the streaming interface.
    // Try up to 2 times; on first failure, do a warm-up probe then retry.
    uvc_error_t res = UVC_ERROR_INVALID_MODE;
    for (int attempt = 0; attempt < 2; attempt++) {
        res = uvc_get_stream_ctrl_format_size(m_devh, ctrl, uvcFmt, width, height, fps);
        if (res == UVC_SUCCESS) break;

        if (attempt == 0) {
            // Warm-up: probe a generic uncompressed format to initialize the VS interface
            uvc_stream_ctrl_t warmup = {};
            uvc_get_stream_ctrl_format_size(m_devh, &warmup, UVC_FRAME_FORMAT_ANY, 0, 0, 0);
        }
    }

    if (res != UVC_SUCCESS) {
        delete ctrl;
        LOG_ERROR(QString("uvc_get_stream_ctrl_format_size failed: %1 (fourcc=%2 subtype=%3 fps=%4)")
            .arg(uvc_strerror(res))
            .arg(QString::fromStdString(fc))
            .arg(subtype)
            .arg(fps));
        return false;
    }

    // Probe the stream control to negotiate with the device before starting
    uvc_probe_stream_ctrl(m_devh, ctrl);

    res = uvc_start_streaming(m_devh, ctrl, frameCallback, this, 0);
    if (res != UVC_SUCCESS) {
        delete ctrl;
        LOG_ERROR(QString("uvc_start_streaming failed: %1").arg(uvc_strerror(res)));
        return false;
    }

    m_streamCtrl = ctrl;
    m_streaming = true;
    m_totalBytes = 0;
    m_totalFrames = 0;
    m_droppedFrames = 0;
    m_lastFrameSequence = 0;

    LOG_INFO(QString("Streaming started: %1x%2").arg(m_currentFormat.width).arg(m_currentFormat.height));
    return true;
}

void LibuvcCameraDevice::stopStreaming() {
    if (!m_streaming) return;
    if (m_devh)
        uvc_stop_streaming(m_devh);
    delete m_streamCtrl;
    m_streamCtrl = nullptr;
    m_streaming = false;
    LOG_INFO("Streaming stopped");
}

bool LibuvcCameraDevice::isStreaming() const { return m_streaming; }

bool LibuvcCameraDevice::readFrame(Frame& frame) {
    // Not used in callback mode; frames arrive via signal
    return false;
}

CameraInfo LibuvcCameraDevice::getDeviceInfo() const { return m_deviceInfo; }

// ── Static callback from libuvc thread ──

void LibuvcCameraDevice::frameCallback(struct uvc_frame* uvcFrame, void* userPtr) {
    auto* self = static_cast<LibuvcCameraDevice*>(userPtr);
    if (!self || !self->m_streaming) return;

    Frame frame;
    frame.width = uvcFrame->width;
    frame.height = uvcFrame->height;
    frame.frame_index = uvcFrame->sequence;
    frame.timestamp_us = static_cast<uint64_t>(uvcFrame->capture_time.tv_sec) * 1000000
                       + uvcFrame->capture_time.tv_usec;
    frame.valid = true;

    switch (uvcFrame->frame_format) {
        case UVC_FRAME_FORMAT_GRAY8:  frame.format = "Y800"; break;
        case UVC_FRAME_FORMAT_GRAY16: frame.format = "Y16"; break;
        case UVC_FRAME_FORMAT_YUYV:   frame.format = "YUYV"; break;
        case UVC_FRAME_FORMAT_MJPEG:  frame.format = "MJPEG"; break;
        case UVC_FRAME_FORMAT_RGB:    frame.format = "RGB"; break;
        default:                       frame.format = "UNKNOWN"; break;
    }

    frame.data.assign(static_cast<uint8_t*>(uvcFrame->data),
                      static_cast<uint8_t*>(uvcFrame->data) + uvcFrame->data_bytes);

    // Drop detection via sequence gap
    if (self->m_lastFrameSequence != 0 &&
        uvcFrame->sequence != self->m_lastFrameSequence + 1) {
        self->m_droppedFrames++;
    }
    self->m_lastFrameSequence = uvcFrame->sequence;

    self->m_totalFrames++;
    self->m_totalBytes += uvcFrame->data_bytes;

    emit self->frameReady(frame);
}
