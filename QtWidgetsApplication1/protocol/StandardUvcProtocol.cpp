#include "StandardUvcProtocol.h"
#include "core/ICameraDevice.h"
#include "infra/LogManager.h"
#include <opencv2/opencv.hpp>
#include <opencv2/imgproc.hpp>

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
        return parseGray8(raw, processed); // passthrough

    LOG_WARNING(QString("Unknown format: %1").arg(QString::fromStdString(raw.format)));
    return false;
}

// ── YUYV → BGR (OpenCV) ──

bool StandardUvcProtocol::parseYUYV(const Frame& raw, ProcessedFrame& processed) {
    // 校验帧数据完整性，丢弃 USB 丢包导致的残缺帧
    size_t expected = static_cast<size_t>(raw.width) * raw.height * 2;
    if (raw.data.size() < expected) {
        LOG_WARNING(QString("YUYV frame truncated: expected %1 bytes, got %2")
            .arg(expected).arg(raw.data.size()));
        return false;
    }
    cv::Mat yuv(raw.height, raw.width, CV_8UC2, const_cast<uint8_t*>(raw.data.data()));
    cv::Mat bgr;
    cv::cvtColor(yuv, bgr, cv::COLOR_YUV2BGR_YUYV);

    processed.width  = bgr.cols;
    processed.height = bgr.rows;
    processed.cv_type = bgr.type();
    processed.data.assign(bgr.data, bgr.data + bgr.total() * bgr.elemSize());
    processed.timestamp_us = raw.timestamp_us;
    processed.valid = true;
    return true;
}

// ── MJPEG → BGR (OpenCV) ──

bool StandardUvcProtocol::parseMJPEG(const Frame& raw, ProcessedFrame& processed) {
    cv::Mat decoded = cv::imdecode(
        cv::Mat(1, static_cast<int>(raw.data.size()), CV_8UC1,
                const_cast<uint8_t*>(raw.data.data())),
        cv::IMREAD_UNCHANGED);

    if (decoded.empty()) return false;

    processed.width  = decoded.cols;
    processed.height = decoded.rows;
    processed.cv_type = decoded.type();
    processed.data.assign(decoded.data, decoded.data + decoded.total() * decoded.elemSize());
    processed.timestamp_us = raw.timestamp_us;
    processed.valid = true;
    return true;
}

// ── Grayscale passthrough ──

bool StandardUvcProtocol::parseGray8(const Frame& raw, ProcessedFrame& processed) {
    // 校验帧数据完整性，丢弃 USB 丢包导致的残缺帧
    size_t expected = static_cast<size_t>(raw.width) * raw.height;
    if (raw.data.size() < expected) {
        LOG_WARNING(QString("Gray8 frame truncated: expected %1 bytes, got %2")
            .arg(expected).arg(raw.data.size()));
        return false;
    }
    processed.width  = raw.width;
    processed.height = raw.height;
    processed.cv_type = CV_8UC1;
    processed.data = raw.data;
    processed.timestamp_us = raw.timestamp_us;
    processed.valid = true;
    return true;
}

bool StandardUvcProtocol::parseGray16(const Frame& raw, ProcessedFrame& processed) {
    // 校验帧数据完整性，丢弃 USB 丢包导致的残缺帧
    size_t expected = static_cast<size_t>(raw.width) * raw.height * 2;
    if (raw.data.size() < expected) {
        LOG_WARNING(QString("Gray16 frame truncated: expected %1 bytes, got %2")
            .arg(expected).arg(raw.data.size()));
        return false;
    }
    processed.width  = raw.width;
    processed.height = raw.height;
    processed.cv_type = CV_16UC1;
    processed.data = raw.data;
    processed.timestamp_us = raw.timestamp_us;
    processed.valid = true;
    return true;
}
