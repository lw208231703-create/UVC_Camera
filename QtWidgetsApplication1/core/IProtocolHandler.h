#pragma once

#include "CameraTypes.h"
#include <string>

class ICameraDevice;

class IProtocolHandler {
public:
    virtual ~IProtocolHandler() = default;

    virtual bool initialize(ICameraDevice* device) = 0;
    virtual bool parseFrame(const Frame& raw, ProcessedFrame& processed) = 0;
    virtual std::string getProtocolName() const = 0;
    virtual std::vector<std::string> getSupportedFormats() const = 0;
};
