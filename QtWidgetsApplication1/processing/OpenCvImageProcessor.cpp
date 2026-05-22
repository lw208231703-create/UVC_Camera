#include "OpenCvImageProcessor.h"
#include <opencv2/imgproc.hpp>

bool OpenCvImageProcessor::process(const ProcessedFrame& input, ProcessedFrame& output) {
    if (!input.valid || input.data.empty())
        return false;

    cv::Mat mat(input.height, input.width, input.cv_type,
                const_cast<uint8_t*>(input.data.data()));
    if (mat.empty()) return false;

    bool modified = false;

    if (m_gaussianBlur && mat.depth() == CV_8U) {
        cv::GaussianBlur(mat, mat, cv::Size(m_gaussianKernel, m_gaussianKernel), 1.5);
        modified = true;
    } else if (m_gaussianBlur && mat.depth() == CV_16U) {
        // 16-bit: convert to 32F for GaussianBlur, then back
        cv::Mat tmp;
        mat.convertTo(tmp, CV_32F);
        cv::GaussianBlur(tmp, tmp, cv::Size(m_gaussianKernel, m_gaussianKernel), 1.5);
        tmp.convertTo(mat, CV_16U);
        modified = true;
    }

    if (m_histogramEq && mat.channels() == 1 && mat.depth() == CV_8U) {
        cv::equalizeHist(mat.clone(), mat);
        modified = true;
    }

    if (modified) {
        output.data.assign(mat.data, mat.data + mat.total() * mat.elemSize());
    } else {
        output.data = input.data;
    }

    output.width = input.width;
    output.height = input.height;
    output.cv_type = mat.type();
    output.timestamp_us = input.timestamp_us;
    output.valid = true;
    return true;
}
