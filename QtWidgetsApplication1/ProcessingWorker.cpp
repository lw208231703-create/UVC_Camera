#include "ProcessingWorker.h"
#include "core/IProtocolHandler.h"
#include "infra/LogManager.h"
#include <QMutexLocker>
#include <QDateTime>
#include <QElapsedTimer>
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

void ProcessingWorker::setDenoiseEnabled(bool enabled) {
    m_denoiseEnabled.store(enabled, std::memory_order_relaxed);
}

uint64_t ProcessingWorker::processedCount() const {
    return m_processedCount.load(std::memory_order_relaxed);
}

uint64_t ProcessingWorker::droppedCount() const {
    return m_droppedCount.load(std::memory_order_relaxed);
}

void ProcessingWorker::resetDiagCounters() {
    m_diagCount.store(0, std::memory_order_relaxed);
}

void ProcessingWorker::enqueueFrame(const Frame& frame) {
    Frame copy = frame;
    {
        QMutexLocker locker(&m_pendingMutex);
        if (m_hasPendingFrame)
            m_droppedCount.fetch_add(1, std::memory_order_relaxed);
        m_pendingFrame = std::move(copy);
        m_hasPendingFrame = true;
    }
    if (!m_draining.exchange(true, std::memory_order_acq_rel))
        QMetaObject::invokeMethod(this, "drainPending", Qt::QueuedConnection);
}

void ProcessingWorker::clearPending() {
    QMutexLocker locker(&m_pendingMutex);
    m_pendingFrame = Frame{};
    m_hasPendingFrame = false;
}

void ProcessingWorker::drainPending() {
    for (;;) {
        Frame frame;
        {
            QMutexLocker locker(&m_pendingMutex);
            if (!m_hasPendingFrame) {
                m_draining.store(false, std::memory_order_release);
                if (!m_hasPendingFrame)
                    return;
                m_draining.store(true, std::memory_order_release);
            }
            frame = std::move(m_pendingFrame);
            m_pendingFrame = Frame{};
            m_hasPendingFrame = false;
        }
        processFrame(std::move(frame));
    }
}

void ProcessingWorker::processFrame(Frame frame) {
    int64_t now = QDateTime::currentMSecsSinceEpoch() * 1000;
    int64_t queueUs = now - frame.pipeline_ts_us;

    QElapsedTimer timer;
    timer.start();
    int64_t tParse = 0, tConvert = 0;

    uint32_t diagIdx = m_diagCount.fetch_add(1, std::memory_order_relaxed);

    ProcessedFrame parsed;
    if (m_protocol && !m_protocol->parseFrame(frame, parsed)) {
        LOG_WARNING(QString("[Frame %1] parse failed for format %2 (size=%3)")
            .arg(diagIdx)
            .arg(QString::fromStdString(frame.format))
            .arg(frame.data.size()));
        return;
    }
    tParse = timer.nsecsElapsed() / 1000;
    if (!m_protocol)
        parsed = {};

    if (!parsed.valid) {
        if (diagIdx < 3)
            LOG_WARNING(QString("[Frame %1] parsed frame invalid").arg(diagIdx));
        return;
    }

    QImage img = frameToQImage(parsed);
    tConvert = timer.nsecsElapsed() / 1000;
    if (!img.isNull()) {
        m_processedCount.fetch_add(1, std::memory_order_relaxed);
        parsed.pipeline_ts_us = frame.pipeline_ts_us;
        emit frameDisplayReady(std::move(img), std::move(parsed));
    }

    int64_t totalUs = timer.nsecsElapsed() / 1000;
    if (diagIdx < 3 || queueUs > 10000 || totalUs > 50000)
        LOG_INFO(QString("[PipeDiag] seq=%1 queue=%2us parse=%3us convert=%4us total=%5us")
            .arg(frame.frame_index).arg(queueUs).arg(tParse).arg(tConvert).arg(totalUs));
}

QImage ProcessingWorker::frameToQImage(const ProcessedFrame& frame) {
    if (!frame.valid || frame.data.empty()) return {};

    int w = static_cast<int>(frame.width);
    int h = static_cast<int>(frame.height);
    bool denoise = m_denoiseEnabled.load(std::memory_order_relaxed);

    if (frame.cv_type == CV_8UC3) {
        size_t expected = static_cast<size_t>(w) * h * 3;
        if (frame.data.size() < expected) {
            LOG_WARNING(QString("frameToQImage: CV_8UC3 data truncated, expected %1, got %2")
                .arg(expected).arg(frame.data.size()));
            return {};
        }
        cv::Mat bgr(h, w, CV_8UC3, const_cast<uint8_t*>(frame.data.data()));
        if (denoise) cv::medianBlur(bgr, bgr, 3);
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
        QElapsedTimer t;
        t.start();
        auto* src16 = reinterpret_cast<const uint16_t*>(frame.data.data());
        size_t n = static_cast<size_t>(w) * h;
        int shift = m_bitShift.load(std::memory_order_relaxed);
        m_buf8.resize(n);
        for (size_t i = 0; i < n; i++)
            m_buf8[i] = static_cast<uint8_t>((src16[i] >> shift) & 0xFF);
        int64_t tShift = t.nsecsElapsed() / 1000;
        cv::Mat gray8(h, w, CV_8UC1, m_buf8.data());
        if (denoise) cv::medianBlur(gray8, gray8, 3);
        int64_t tBlur = t.nsecsElapsed() / 1000;
        QImage result = QImage(m_buf8.data(), w, h, QImage::Format_Grayscale8).copy();
        int64_t tCopy = t.nsecsElapsed() / 1000;
        LOG_INFO(QString("[PipeDiag] convert_detail: shift=%1us blur=%2us copy=%3us")
            .arg(tShift).arg(tBlur - tShift).arg(tCopy - tBlur));
        return result;

    } else {
        cv::Mat gray8(h, w, CV_8UC1, const_cast<uint8_t*>(frame.data.data()));
        if (denoise) cv::medianBlur(gray8, gray8, 3);
        return QImage(frame.data.data(), w, h, QImage::Format_Grayscale8).copy();
    }
}
