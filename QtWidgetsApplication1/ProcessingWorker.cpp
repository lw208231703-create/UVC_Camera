#include "ProcessingWorker.h"
#include "core/IProtocolHandler.h"
#include "infra/LogManager.h"
#include <opencv2/opencv.hpp>
#include <opencv2/imgproc.hpp>

ProcessingWorker::ProcessingWorker(QObject* parent) : QObject(parent) {}

ProcessingWorker::~ProcessingWorker() = default;

void ProcessingWorker::setProtocol(IProtocolHandler* protocol) {
    m_protocol = protocol;
}

void ProcessingWorker::setBitShift(int shift) {
    m_bitShift.store(shift, std::memory_order_relaxed);
}

int ProcessingWorker::bitShift() const {
    return m_bitShift.load(std::memory_order_relaxed);
}

uint64_t ProcessingWorker::processedCount() const {
    return m_processedCount.load(std::memory_order_relaxed);
}

uint64_t ProcessingWorker::droppedCount() const {
    return m_droppedCount.load(std::memory_order_relaxed);
}

void ProcessingWorker::processFrame(const Frame& frame) {
    // ── 丢帧保护：如果上一帧还在处理中，跳过当前帧 ──
    // 防止 10MB bulk 帧在处理管线中堆积形成背压
    bool expected = false;
    if (!m_busy.compare_exchange_strong(expected, true)) {
        m_droppedCount.fetch_add(1, std::memory_order_relaxed);
        return;
    }

    // ── 诊断日志：前 3 帧 ──
    static uint32_t diagCount = 0;
    diagCount++;
    if (diagCount <= 3) {
        LOG_INFO(QString("[Frame %1] raw: %2 %3x%4 %5 bytes")
            .arg(diagCount - 1)
            .arg(QString::fromStdString(frame.format))
            .arg(frame.width).arg(frame.height)
            .arg(frame.data.size()));
    }

    // ── 协议：raw → processed ──
    ProcessedFrame parsed;
    if (m_protocol && !m_protocol->parseFrame(frame, parsed)) {
        if (diagCount <= 3)
            LOG_WARNING(QString("[Frame %1] parse failed for format %2")
                .arg(diagCount - 1)
                .arg(QString::fromStdString(frame.format)));
        m_busy.store(false, std::memory_order_release);
        return;
    }
    if (!m_protocol)
        parsed = {};

    if (!parsed.valid) {
        if (diagCount <= 3)
            LOG_WARNING(QString("[Frame %1] parsed frame invalid").arg(diagCount - 1));
        m_busy.store(false, std::memory_order_release);
        return;
    }

    if (diagCount <= 3)
        LOG_INFO(QString("[Frame %1] parsed: type=%2 %3x%4 %5 bytes")
            .arg(diagCount - 1)
            .arg(parsed.cv_type)
            .arg(parsed.width).arg(parsed.height)
            .arg(parsed.data.size()));

    // ── QImage 转换（在 worker 线程执行，不阻塞 UI）──
    QImage img = frameToQImage(parsed);
    if (!img.isNull()) {
        m_processedCount.fetch_add(1, std::memory_order_relaxed);
        emit frameDisplayReady(img, parsed);
    }

    m_busy.store(false, std::memory_order_release);
}

QImage ProcessingWorker::frameToQImage(const ProcessedFrame& frame) {
    if (!frame.valid || frame.data.empty()) return {};

    int w = static_cast<int>(frame.width);
    int h = static_cast<int>(frame.height);

    if (frame.cv_type == CV_8UC3) {
        size_t expected = static_cast<size_t>(w) * h * 3;
        if (frame.data.size() < expected) {
            LOG_WARNING(QString("frameToQImage: CV_8UC3 data truncated, expected %1, got %2")
                .arg(expected).arg(frame.data.size()));
            return {};
        }
        // BGR (OpenCV default) → RGB for QImage
        cv::Mat bgr(h, w, CV_8UC3, const_cast<uint8_t*>(frame.data.data()));
        cv::Mat rgb;
        cv::cvtColor(bgr, rgb, cv::COLOR_BGR2RGB);
        return QImage(rgb.data, rgb.cols, rgb.rows, rgb.step,
                      QImage::Format_RGB888).copy();

    } else if (frame.cv_type == CV_16UC1) {
        size_t expected = static_cast<size_t>(w) * h * 2;
        if (frame.data.size() < expected) {
            LOG_WARNING(QString("frameToQImage: CV_16UC1 data truncated, expected %1, got %2")
                .arg(expected).arg(frame.data.size()));
            return {};
        }
        // 16-bit → extract 8-bit slice per slider position
        auto* src16 = reinterpret_cast<const uint16_t*>(frame.data.data());
        size_t n = static_cast<size_t>(w) * h;
        int shift = m_bitShift.load(std::memory_order_relaxed);
        std::vector<uint8_t> buf8(n);
        for (size_t i = 0; i < n; i++)
            buf8[i] = static_cast<uint8_t>((src16[i] >> shift) & 0xFF);
        return QImage(buf8.data(), w, h, QImage::Format_Grayscale8).copy();

    } else {
        // CV_8UC1 or unknown → grayscale passthrough
        return QImage(frame.data.data(), w, h, QImage::Format_Grayscale8).copy();
    }
}
