#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct CameraFormat {
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t fourcc = 0;       // V4L2/Pixel format FourCC (e.g. "Y16 ", "Y800")
    uint32_t fps = 0;
    std::string description;
};

struct Frame {
    std::vector<uint8_t> data;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t frame_index = 0;
    uint64_t timestamp_us = 0;
    std::string format;        // "Y800", "Y16", "YUYV", "MJPEG", "custom_12bit", "custom_14bit"
    bool valid = false;
};

struct ProcessedFrame {
    std::vector<uint8_t> data;
    uint32_t width = 0;
    uint32_t height = 0;
    int cv_type = 0;           // CV_8UC1, CV_16UC1, CV_8UC3, etc.
    uint64_t timestamp_us = 0;
    bool valid = false;
};

struct CameraInfo {
    std::string name;
    uint16_t vendor_id = 0;
    uint16_t product_id = 0;
    std::string serial_number;
    std::string bus_location;
    std::vector<CameraFormat> supported_formats;
};
