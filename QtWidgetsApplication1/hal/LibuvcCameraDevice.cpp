#include "LibuvcCameraDevice.h"
#include "DeviceEnumerator.h"
#include "infra/LogManager.h"
#include <libuvc/libuvc.h>

// 阻止 winsock.h 的 timeval 重定义冲突 (libusb.h → winsock.h vs sys/time.h)
#ifndef _WINSOCKAPI_
#define _WINSOCKAPI_
#endif
#include <libuvc/libuvc_internal.h>
#include <libusb.h>
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

    uvc_error_t res;
    if (m_cameraIndex > 0) {
        res = uvc_open2(m_rawDevice, &m_devh, m_cameraIndex);
        LOG_INFO(QString("uvc_open2 called with camera_idx=%1").arg(m_cameraIndex));
    } else {
        res = uvc_open(m_rawDevice, &m_devh);
    }
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
    m_realDroppedFrames = 0;
    m_minorShortFrames = 0;
    m_callbackLogCount = 0;
    m_statsLogCounter = 0;
    m_lastFrameSequence = 0;
    m_warmupCounter = 0;

    LOG_INFO(QString("Streaming started: %1x%2").arg(m_currentFormat.width).arg(m_currentFormat.height));

    // ── USB 环境诊断 (libusb 版本、设备速度、RAW_IO 状态) ──
    {
        const struct libusb_version* v = libusb_get_version();
        LOG_INFO(QString("[USB Env] libusb %1.%2.%3.%4 (API 0x%5)")
            .arg(v->major).arg(v->minor).arg(v->micro).arg(v->nano)
            .arg(LIBUSB_API_VERSION, 0, 16));

        libusb_device_handle* usbh = uvc_get_libusb_handle(m_devh);
        if (usbh) {
            libusb_device* dev = libusb_get_device(usbh);
            int speed = libusb_get_device_speed(dev);
            const char* speedName = "Unknown";
            switch (speed) {
                case 1: speedName = "Low"; break;
                case 2: speedName = "Full"; break;
                case 3: speedName = "High"; break;
                case 4: speedName = "SuperSpeed"; break;
                case 5: speedName = "SuperSpeedPlus"; break;
                case 6: speedName = "SuperSpeedPlusX2"; break;
            }
            LOG_INFO(QString("[USB Env] device speed: %1 (%2)").arg(speedName).arg(speed));

            // 获取 bulk IN endpoint 地址并检查 RAW_IO
            uint8_t endpoint = 0;
            const uvc_format_desc_t* fmtDesc = uvc_get_format_descs(m_devh);
            while (fmtDesc) {
                if (fmtDesc->bFormatIndex == m_currentFormat.bFormatIndex) {
                    // parent 是 uvc_streaming_interface, 在 libuvc_internal.h 中定义
                    endpoint = fmtDesc->parent->bEndpointAddress;
                    break;
                }
                fmtDesc = fmtDesc->next;
            }

            if (endpoint) {
                int maxPacket = libusb_get_max_packet_size(dev, endpoint);
                LOG_INFO(QString("[USB Env] bulk endpoint: 0x%1  max_packet: %2 bytes")
                    .arg(endpoint, 2, 16, QChar('0')).arg(maxPacket));

#if defined(_WIN32) && defined(LIBUSB_API_VERSION) && (LIBUSB_API_VERSION >= 0x0100010C)
                int supportsRaw = libusb_endpoint_supports_raw_io(usbh, endpoint);
                if (supportsRaw == 1) {
                    int maxRaw = libusb_get_max_raw_io_transfer_size(usbh, endpoint);
                    size_t payload = m_streamCtrl->dwMaxPayloadTransferSize;
                    size_t aligned = ((payload + maxPacket - 1) / maxPacket) * (size_t)maxPacket;
                    LOG_INFO(QString("[USB Env] RAW_IO: supported (max_transfer=%1 bytes / %2 KB)")
                        .arg(maxRaw).arg(maxRaw / 1024.0, 0, 'f', 1));
                    LOG_INFO(QString("[USB Env] RAW_IO: request aligned %1 → %2 bytes")
                        .arg(payload).arg(aligned));
                    if ((int)aligned > maxRaw) {
                        // stream.c 会 cap 到 maxRaw 并启用 RAW_IO (多 transfer 组装)
                        size_t capped = ((size_t)maxRaw / (size_t)maxPacket) * (size_t)maxPacket;
                        LOG_INFO(QString("[USB Env] RAW_IO: ENABLED (capped %1 → %2 bytes, %3 transfers/frame)")
                            .arg(aligned).arg(capped)
                            .arg((m_streamCtrl->dwMaxVideoFrameSize + capped - 1) / capped));
                    } else {
                        LOG_INFO("[USB Env] RAW_IO: ENABLED (by libuvc stream.c)");
                    }
                } else {
                    LOG_INFO(QString("[USB Env] RAW_IO: NOT supported (rc=%1) — using normal WinUSB path")
                        .arg(supportsRaw));
                }
#else
                LOG_INFO("[USB Env] RAW_IO: not compiled (libusb < 1.0.30)");
#endif
            }
        }
    }

    // ── 流启动诊断信息 ──
    if (m_streamCtrl) {
        auto* ctrl = m_streamCtrl;
        double frameSizeKB = ctrl->dwMaxVideoFrameSize / 1024.0;
        double payloadSizeKB = ctrl->dwMaxPayloadTransferSize / 1024.0;

        LOG_INFO(QString("[Stream Diag] ======== 流启动参数 ========"));
        LOG_INFO(QString("[Stream Diag]   帧大小 (dwMaxVideoFrameSize):  %1 bytes (%2 KB)")
            .arg(ctrl->dwMaxVideoFrameSize).arg(frameSizeKB, 0, 'f', 1));
        LOG_INFO(QString("[Stream Diag]   载荷上限 (dwMaxPayloadTransferSize): %1 bytes (%2 KB)")
            .arg(ctrl->dwMaxPayloadTransferSize).arg(payloadSizeKB, 0, 'f', 1));
        LOG_INFO(QString("[Stream Diag]   帧间隔 (dwFrameInterval): %1 (100ns单位) = ~%2 FPS")
            .arg(ctrl->dwFrameInterval)
            .arg(ctrl->dwFrameInterval > 0 ? 10000000.0 / ctrl->dwFrameInterval : 0, 0, 'f', 1));
        LOG_INFO(QString("[Stream Diag]   格式索引: %1  帧索引: %2  接口: %3")
            .arg(ctrl->bFormatIndex).arg(ctrl->bFrameIndex).arg(ctrl->bInterfaceNumber));

        // USB 3.0 bulk 传输配置诊断
        bool isWholeFrame = ctrl->dwMaxPayloadTransferSize >= ctrl->dwMaxVideoFrameSize;
        double xferKB = ctrl->dwMaxPayloadTransferSize / 1024.0;
        double poolMB = xferKB * LIBUVC_NUM_TRANSFER_BUFS / 1024.0;
        int transfersPerFrame = (int)((ctrl->dwMaxVideoFrameSize + ctrl->dwMaxPayloadTransferSize - 1) / ctrl->dwMaxPayloadTransferSize);

        LOG_INFO(QString("[Stream Diag]   传输模式: %1")
            .arg(isWholeFrame ? "整帧 Bulk 传输" : "分片 Bulk 传输"));
        LOG_INFO(QString("[Stream Diag]   每次 transfer: %1 KB")
            .arg(xferKB, 0, 'f', 1));
        LOG_INFO(QString("[Stream Diag]   Buffer 池: %1 个 x %2 KB = %3 MB")
            .arg(LIBUVC_NUM_TRANSFER_BUFS)
            .arg(xferKB, 0, 'f', 1)
            .arg(poolMB, 0, 'f', 1));
        LOG_INFO(QString("[Stream Diag]   每帧 transfer 数: ~%1").arg(transfersPerFrame));
        LOG_INFO(QString("[Stream Diag] ==============================="));
    }

    return true;
}

void LibuvcCameraDevice::stopStreaming() {
    bool wasStreaming = m_streaming.exchange(false);
    if (!wasStreaming) return;

    if (m_devh)
        uvc_stop_streaming(m_devh);
    delete m_streamCtrl;
    m_streamCtrl = nullptr;
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

    // ── Warmup skip: 流启动后前 N 帧数据不稳定，直接丢弃 ──
    uint32_t warmupIdx = self->m_warmupCounter.fetch_add(1, std::memory_order_relaxed);
    if (warmupIdx < kWarmupFrames) {
        LOG_INFO(QString("[Warmup] Skipping frame %1 (USB pipe not yet stable)")
            .arg(warmupIdx));
        return;
    }

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

    uint32_t cbLogIndex = self->m_callbackLogCount.fetch_add(1, std::memory_order_relaxed);
    if (cbLogIndex < 3) {
        LOG_INFO(QString("[CB Frame %1] fmt=%2 %3x%4 %5 bytes seq=%6")
            .arg(cbLogIndex)
            .arg(QString::fromStdString(frame.format))
            .arg(frame.width).arg(frame.height)
            .arg(frame.data.size())
            .arg(frame.frame_index));
    }

    // Drop detection via sequence gap (complete frame loss — frame never delivered)
    if (self->m_lastFrameSequence != 0 &&
        uvcFrame->sequence != self->m_lastFrameSequence + 1) {
        uint32_t gap = (uvcFrame->sequence > self->m_lastFrameSequence + 1)
            ? (uvcFrame->sequence - self->m_lastFrameSequence - 1) : 1;
        self->m_droppedFrames += gap;
        self->m_realDroppedFrames += gap;
        LOG_WARNING(QString("[REAL DROP] seq skip: last=%1 got=%2 gap=%3 (total_real=%4)")
            .arg(self->m_lastFrameSequence).arg(uvcFrame->sequence)
            .arg(gap).arg(self->m_realDroppedFrames.load()));
    }
    self->m_lastFrameSequence = uvcFrame->sequence;

    // Drop detection via data size (BULK truncation)
    // 区分 UVC header 未剥离的假告警 (diff<=16) 和真实 USB 丢包 (diff>1024)
    {
        size_t expected = 0;
        if (uvcFrame->frame_format == UVC_FRAME_FORMAT_GRAY16)
            expected = (size_t)uvcFrame->width * uvcFrame->height * 2;
        else if (uvcFrame->frame_format == UVC_FRAME_FORMAT_GRAY8)
            expected = (size_t)uvcFrame->width * uvcFrame->height;
        else if (uvcFrame->frame_format == UVC_FRAME_FORMAT_YUYV)
            expected = (size_t)uvcFrame->width * uvcFrame->height * 2;

        if (expected > 0) {
            int64_t diff = (int64_t)expected - (int64_t)uvcFrame->data_bytes;
            if (diff > 0) {
                if (diff <= kHeaderTolerance) {
                    // UVC payload header (12B) 未剥离或微小时序抖动 — 非真实丢包
                    self->m_minorShortFrames.fetch_add(1, std::memory_order_relaxed);
                } else if (diff > kRealDropThreshold) {
                    // 真实丢包: USB 传输截断 / FT602 FIFO 溢出 / XHCI 调度抖动
                    self->m_realDroppedFrames.fetch_add(1, std::memory_order_relaxed);
                    self->m_droppedFrames.fetch_add(1, std::memory_order_relaxed);
                    double lossPct = diff * 100.0 / (double)expected;
                    LOG_WARNING(QString("[REAL DROP] seq=%1: got=%2 expected=%3 (diff=%4, %5%)")
                        .arg(uvcFrame->sequence)
                        .arg(uvcFrame->data_bytes).arg(expected)
                        .arg(diff).arg(lossPct, 0, 'f', 1));
                } else {
                    // 16 < diff <= 1024: 临界区间, 记录但不计为真实丢包
                    self->m_minorShortFrames.fetch_add(1, std::memory_order_relaxed);
                    if (self->m_statsLogCounter.load() % 50 == 0)
                        LOG_INFO(QString("[Minor Short] seq=%1 diff=%2 (tolerance zone)")
                            .arg(uvcFrame->sequence).arg(diff));
                }
            }
        }
    }

    self->m_totalFrames++;
    self->m_totalBytes += uvcFrame->data_bytes;

    // ── 周期性统计: 每 100 帧输出真实丢帧率 ──
    {
        uint32_t statsIdx = self->m_statsLogCounter.fetch_add(1, std::memory_order_relaxed);
        if (statsIdx > 0 && statsIdx % 100 == 0) {
            uint32_t total   = self->m_totalFrames.load();
            uint32_t realDrops = self->m_realDroppedFrames.load();
            uint32_t minor    = self->m_minorShortFrames.load();
            double realDropRate = total > 0 ? realDrops * 100.0 / total : 0.0;
            LOG_INFO(QString("[Stats] frames=%1 real_drops=%2 (%3%) minor_short=%4 seq=%5")
                .arg(total).arg(realDrops)
                .arg(realDropRate, 0, 'f', 2)
                .arg(minor).arg(uvcFrame->sequence));
        }
    }

    emit self->frameReady(frame);
}
