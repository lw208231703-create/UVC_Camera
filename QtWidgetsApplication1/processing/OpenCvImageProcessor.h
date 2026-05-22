#pragma once

#include "core/IImageProcessor.h"
#include <opencv2/core.hpp>

class OpenCvImageProcessor : public IImageProcessor {
public:
    bool process(const ProcessedFrame& input, ProcessedFrame& output) override;
    std::string getProcessorName() const override { return "opencv"; }

    void setGaussianBlur(bool enable)   { m_gaussianBlur = enable; }
    void setHistogramEq(bool enable)    { m_histogramEq = enable; }
    void setGaussianKernel(int k)       { m_gaussianKernel = k; }

    bool gaussianBlur() const           { return m_gaussianBlur; }
    bool histogramEq() const            { return m_histogramEq; }

private:
    bool m_gaussianBlur = false;
    bool m_histogramEq = false;
    int  m_gaussianKernel = 5;
};
