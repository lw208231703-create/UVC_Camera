#pragma once

#include "CameraTypes.h"
#include <string>

class IImageProcessor {
public:
    virtual ~IImageProcessor() = default;

    virtual bool process(const ProcessedFrame& input, ProcessedFrame& output) = 0;
    virtual std::string getProcessorName() const = 0;
};
