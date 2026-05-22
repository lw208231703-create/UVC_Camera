#pragma once

#include "CameraTypes.h"
#include <string>
#include <vector>

class ICameraDevice {
public:
    virtual ~ICameraDevice() = default;

    virtual bool open() = 0;
    virtual void close() = 0;
    virtual bool isOpen() const = 0;

    virtual std::vector<CameraFormat> getSupportedFormats() const = 0;
    virtual bool setFormat(const CameraFormat& format) = 0;
    virtual CameraFormat getCurrentFormat() const = 0;

    virtual bool startStreaming() = 0;
    virtual void stopStreaming() = 0;
    virtual bool isStreaming() const = 0;

    virtual bool readFrame(Frame& frame) = 0;

    virtual CameraInfo getDeviceInfo() const = 0;
};
