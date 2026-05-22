#include "CustomUvcProtocol.h"
#include "core/ICameraDevice.h"
#include "infra/LogManager.h"
#include <opencv2/core.hpp>

bool CustomUvcProtocol::initialize(ICameraDevice* device) {
    m_device = device;
    m_pixelBuffer.reserve(4096 * 4096);
    LOG_INFO(QString("Custom UVC protocol initialized (variant: %1)")
        .arg(m_variant == Bits12 ? "12-bit" : "14-bit"));
    return true;
}

std::string CustomUvcProtocol::getProtocolName() const {
    return m_variant == Bits12 ? "custom_12bit" : "custom_14bit";
}

std::vector<std::string> CustomUvcProtocol::getSupportedFormats() const {
    return {"custom_12bit", "custom_14bit"};
}

bool CustomUvcProtocol::parseFrame(const Frame& raw, ProcessedFrame& processed) {
    if (!raw.valid || raw.data.empty()) return false;

    if (raw.format == "custom_12bit")
        return parse12Bit(raw, processed);
    if (raw.format == "custom_14bit")
        return parse14Bit(raw, processed);

    return false;
}

// ── 12-bit unpacking ──
// Two 12-bit pixels packed into 3 bytes:
//   B0: P1[7:0]
//   B1[3:0]: P1[11:8], B1[7:4]: P2[3:0]
//   B2: P2[11:4]
// Output: two uint16_t values per 3 input bytes
bool CustomUvcProtocol::parse12Bit(const Frame& raw, ProcessedFrame& processed) {
    const uint8_t* src = raw.data.data();
    size_t srcSize = raw.data.size();

    if (srcSize % 3 != 0) {
        LOG_WARNING("12-bit data size not multiple of 3");
        return false;
    }

    size_t pixelCount = (srcSize / 3) * 2;
    m_pixelBuffer.resize(pixelCount);

    uint16_t* dst = m_pixelBuffer.data();
    for (size_t i = 0; i < srcSize; i += 3) {
        uint8_t b0 = src[i];
        uint8_t b1 = src[i + 1];
        uint8_t b2 = src[i + 2];

        // P1 = B0 | ((B1 & 0x0F) << 8)
        dst[0] = static_cast<uint16_t>(b0) | (static_cast<uint16_t>(b1 & 0x0F) << 8);
        // P2 = ((B1 & 0xF0) >> 4) | (B2 << 4)
        dst[1] = static_cast<uint16_t>(b1 >> 4) | (static_cast<uint16_t>(b2) << 4);
        dst += 2;
    }

    processed.data.assign(
        reinterpret_cast<uint8_t*>(m_pixelBuffer.data()),
        reinterpret_cast<uint8_t*>(m_pixelBuffer.data() + pixelCount));
    processed.width = raw.width;
    processed.height = raw.height;
    processed.cv_type = CV_16UC1;
    processed.timestamp_us = raw.timestamp_us;
    processed.valid = true;
    return true;
}

// ── 14-bit unpacking ──
// Four 14-bit pixels (56 bits) packed into 7 bytes.
// Bit layout (MSB first per group):
//   Byte0: P0[13:6]
//   Byte1: P0[5:0]  + P1[13:12]
//   Byte2: P1[11:4]
//   Byte3: P1[3:0]  + P2[13:10]
//   Byte4: P2[9:2]
//   Byte5: P2[1:0]  + P3[13:8]
//   Byte6: P3[7:0]
bool CustomUvcProtocol::parse14Bit(const Frame& raw, ProcessedFrame& processed) {
    const uint8_t* src = raw.data.data();
    size_t srcSize = raw.data.size();

    if (srcSize % 7 != 0) {
        LOG_WARNING("14-bit data size not multiple of 7");
        return false;
    }

    size_t pixelCount = (srcSize / 7) * 4;
    m_pixelBuffer.resize(pixelCount);

    uint16_t* dst = m_pixelBuffer.data();
    for (size_t i = 0; i < srcSize; i += 7) {
        uint8_t b0 = src[i];
        uint8_t b1 = src[i + 1];
        uint8_t b2 = src[i + 2];
        uint8_t b3 = src[i + 3];
        uint8_t b4 = src[i + 4];
        uint8_t b5 = src[i + 5];
        uint8_t b6 = src[i + 6];

        dst[0] = (static_cast<uint16_t>(b0) << 6) | (static_cast<uint16_t>(b1) >> 2);
        dst[1] = ((static_cast<uint16_t>(b1) & 0x03) << 12) | (static_cast<uint16_t>(b2) << 4) | (static_cast<uint16_t>(b3) >> 4);
        dst[2] = ((static_cast<uint16_t>(b3) & 0x0F) << 10) | (static_cast<uint16_t>(b4) << 2) | (static_cast<uint16_t>(b5) >> 6);
        dst[3] = ((static_cast<uint16_t>(b5) & 0x3F) << 8) | static_cast<uint16_t>(b6);
        dst += 4;
    }

    processed.data.assign(
        reinterpret_cast<uint8_t*>(m_pixelBuffer.data()),
        reinterpret_cast<uint8_t*>(m_pixelBuffer.data() + pixelCount));
    processed.width = raw.width;
    processed.height = raw.height;
    processed.cv_type = CV_16UC1;
    processed.timestamp_us = raw.timestamp_us;
    processed.valid = true;
    return true;
}
