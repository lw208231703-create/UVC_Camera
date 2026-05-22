#include "StandardUvcProtocol.h"
#include "core/ICameraDevice.h"
#include "infra/LogManager.h"
#include "core/CameraTypes.h"
#include <QImage>
#include <libyuv.h>

bool StandardUvcProtocol::initialize(ICameraDevice* device) {
    m_device = device;
    LOG_INFO("Standard UVC protocol initialized");
    return true;
}

std::vector<std::string> StandardUvcProtocol::getSupportedFormats() const {
    return {"Y800", "Y16", "YUYV", "MJPEG", "RGB"};
}

bool StandardUvcProtocol::parseFrame(const Frame& raw, ProcessedFrame& processed) {
    if (!raw.valid || raw.data.empty())
        return false;

    if (raw.format == "YUYV" || raw.format == "YUY2")
        return parseYUYV(raw, processed);
    if (raw.format == "MJPEG" || raw.format == "MJPG")
        return parseMJPEG(raw, processed);
    if (raw.format == "Y800" || raw.format == "GRAY8")
        return parseGray8(raw, processed);
    if (raw.format == "Y16" || raw.format == "GRAY16")
        return parseGray16(raw, processed);
    if (raw.format == "RGB")
        return parseGray8(raw, processed);

    LOG_WARNING(QString("Unknown format: %1").arg(QString::fromStdString(raw.format)));
    return false;
}

// ── YUYV → RGB via libyuv (SIMD-accelerated when available) ──

bool StandardUvcProtocol::parseYUYV(const Frame& raw, ProcessedFrame& processed) {
    int w = static_cast<int>(raw.width);
    int h = static_cast<int>(raw.height);

    // Step 1: YUY2 → ARGB (libyuv outputs BGRA in memory on little-endian)
    size_t argbSize = w * h * 4;
    m_tempBuf.resize(argbSize + w * h * 3);

    uint8_t* argb = m_tempBuf.data();
    uint8_t* rgb  = argb + argbSize;

    libyuv::YUY2ToARGB(raw.data.data(), w * 2,
                       argb, w * 4,
                       w, h);

    // Step 2: ARGB → RGB24 (drop alpha channel)
    libyuv::ARGBToRGB24(argb, w * 4,
                        rgb, w * 3,
                        w, h);

    size_t rgbSize = w * h * 3;
    processed.data.assign(rgb, rgb + rgbSize);
    processed.width = raw.width;
    processed.height = raw.height;
    processed.cv_type = FMT_RGB8;
    processed.timestamp_us = raw.timestamp_us;
    processed.valid = true;
    return true;
}

// ── MJPEG → RGB via QImage ──

bool StandardUvcProtocol::parseMJPEG(const Frame& raw, ProcessedFrame& processed) {
    QImage img;
    if (!img.loadFromData(raw.data.data(), (int)raw.data.size(), "JPEG"))
        return false;

    if (img.format() != QImage::Format_RGB888)
        img = img.convertToFormat(QImage::Format_RGB888);

    processed.width  = img.width();
    processed.height = img.height();
    processed.cv_type = FMT_RGB8;
    processed.data.assign(img.constBits(), img.constBits() + img.sizeInBytes());
    processed.timestamp_us = raw.timestamp_us;
    processed.valid = true;
    return true;
}

// ── Grayscale passthrough ──

bool StandardUvcProtocol::parseGray8(const Frame& raw, ProcessedFrame& processed) {
    processed.width = raw.width;
    processed.height = raw.height;
    processed.cv_type = FMT_GRAY8;
    processed.data = raw.data;
    processed.timestamp_us = raw.timestamp_us;
    processed.valid = true;
    return true;
}

bool StandardUvcProtocol::parseGray16(const Frame& raw, ProcessedFrame& processed) {
    processed.width = raw.width;
    processed.height = raw.height;
    processed.cv_type = FMT_GRAY16;
    processed.data = raw.data;
    processed.timestamp_us = raw.timestamp_us;
    processed.valid = true;
    return true;
}
