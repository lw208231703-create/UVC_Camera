#pragma once

#include "core/IProtocolHandler.h"
#include <vector>

class StandardUvcProtocol : public IProtocolHandler {
public:
    bool initialize(ICameraDevice* device) override;
    bool parseFrame(const Frame& raw, ProcessedFrame& processed) override;
    std::string getProtocolName() const override { return "standard"; }
    std::vector<std::string> getSupportedFormats() const override;

private:
    bool parseYUYV(const Frame& raw, ProcessedFrame& processed);
    bool parseMJPEG(const Frame& raw, ProcessedFrame& processed);
    bool parseGray8(const Frame& raw, ProcessedFrame& processed);
    bool parseGray16(const Frame& raw, ProcessedFrame& processed);

    ICameraDevice* m_device = nullptr;
    std::vector<uint8_t> m_tempBuf;  // libyuv temporary buffer
};
