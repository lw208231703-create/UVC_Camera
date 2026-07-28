#pragma once

#include "core/IProtocolHandler.h"
#include <vector>

class StandardUvcProtocol : public IProtocolHandler {
public:
    bool initialize(ICameraDevice* device) override;
    bool parseFrame(Frame& raw, ProcessedFrame& processed) override;
    std::string getProtocolName() const override { return "standard"; }
    std::vector<std::string> getSupportedFormats() const override;

private:
    bool parseYUYV(Frame& raw, ProcessedFrame& processed);
    bool parseMJPEG(Frame& raw, ProcessedFrame& processed);
    bool parseGray8(Frame& raw, ProcessedFrame& processed);
    bool parseGray16(Frame& raw, ProcessedFrame& processed);

    ICameraDevice* m_device = nullptr;
};
