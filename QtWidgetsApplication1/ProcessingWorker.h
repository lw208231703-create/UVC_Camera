#pragma once

#include <QObject>
#include <QImage>
#include <atomic>
#include <cstdint>
#include "core/CameraTypes.h"

class IProtocolHandler;

/**
 * @brief 帧处理工作线程：协议解析 + QImage 转换在独立线程中完成。
 *
 * 由 QtWidgetsApplication1 持有，通过 moveToThread() 移至 worker QThread。
 * 信号/槽全部使用 Qt::QueuedConnection（跨线程自动）：
 *   - 接收: processFrame(const Frame&) — 来自 libuvc 回调
 *   - 发送: frameDisplayReady(QImage, ProcessedFrame) — 发往主线程
 *   - 内置丢帧: 当正在处理上一帧时，新帧直接丢弃 (atomic busy flag)
 */
class ProcessingWorker : public QObject {
    Q_OBJECT
public:
    explicit ProcessingWorker(QObject* parent = nullptr);
    ~ProcessingWorker() override;

    void setProtocol(IProtocolHandler* protocol);
    void setBitShift(int shift);
    int bitShift() const;

    uint64_t processedCount() const;
    uint64_t droppedCount() const;

public slots:
    /// 在 worker 线程上处理原始帧（通过 QueuedConnection 从相机接收）
    void processFrame(const Frame& frame);

signals:
    /// 帧处理完成（QueuedConnection 回主线程）
    void frameDisplayReady(QImage img, ProcessedFrame parsed);

private:
    QImage frameToQImage(const ProcessedFrame& frame);

    IProtocolHandler* m_protocol = nullptr;
    std::atomic<int> m_bitShift{8};
    std::atomic<uint64_t> m_processedCount{0};
    std::atomic<uint64_t> m_droppedCount{0};
    std::atomic<bool> m_busy{false};
};
