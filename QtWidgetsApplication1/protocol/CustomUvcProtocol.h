#pragma once

#include "core/IProtocolHandler.h"

class CustomUvcProtocol : public IProtocolHandler {
public:
    enum Variant { Bits12, Bits14 };

    explicit CustomUvcProtocol(Variant v = Bits12) : m_variant(v) {}

    bool initialize(ICameraDevice* device) override;
    bool parseFrame(const Frame& raw, ProcessedFrame& processed) override;
    std::string getProtocolName() const override;
    std::vector<std::string> getSupportedFormats() const override;

    void setVariant(Variant v) { m_variant = v; }

private:
    bool parse12Bit(const Frame& raw, ProcessedFrame& processed);
    bool parse14Bit(const Frame& raw, ProcessedFrame& processed);

    ICameraDevice* m_device = nullptr;
    Variant m_variant = Bits12;
    std::vector<uint16_t> m_pixelBuffer;
};
