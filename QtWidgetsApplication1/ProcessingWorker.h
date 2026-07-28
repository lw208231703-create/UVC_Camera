#pragma once

#include <QObject>
#include <QImage>
#include <QMutex>
#include <atomic>
#include <cstdint>
#include "core/CameraTypes.h"

class IProtocolHandler;

/**
 * @brief 帧处理工作线程：协议解析 + QImage 转换在独立线程中完成。
 *
 * 线程模型（有界队列，防止低性能 PC 上事件队列堆积帧深拷贝导致内存膨胀）：
 *   - enqueueFrame() — 由 libuvc 回调经 DirectConnection 同步调用。
 *     帧写入加锁单槽位（只保留最新），且仅在 worker 空闲时投递 drainPending() 无数据事件。
 *   - frameDisplayReady() — DirectConnection 到主窗口暂存槽。
 */
class ProcessingWorker : public QObject {
    Q_OBJECT
public:
    explicit ProcessingWorker(QObject* parent = nullptr);
    ~ProcessingWorker() override;

    void setProtocol(IProtocolHandler* protocol);
    void setBitShift(int shift);
    int bitShift() const;
    void setDenoiseEnabled(bool enabled);

    uint64_t processedCount() const;
    uint64_t droppedCount() const;

    void resetDiagCounters();

    /// 线程安全入队：可从任意线程调用。
    /// 槽位中未处理的旧帧被覆盖并计为丢帧——始终只保留最新。
    void enqueueFrame(const Frame& frame);

    /// 清空待处理帧（停流时调用）
    void clearPending();

signals:
    void frameDisplayReady(QImage img, ProcessedFrame parsed);

private slots:
    /// 在 worker 线程排空待处理帧（事件本身不携带数据）
    void drainPending();

private:
    void processFrame(Frame frame);
    QImage frameToQImage(const ProcessedFrame& frame);

    IProtocolHandler* m_protocol = nullptr;
    std::atomic<int> m_bitShift{8};
    std::atomic<bool> m_denoiseEnabled{true};
    std::atomic<uint64_t> m_processedCount{0};
    std::atomic<uint64_t> m_droppedCount{0};
    std::atomic<uint32_t> m_diagCount{0};

    // ── 最新帧单槽位（内存有界：最多 1 帧待处理 + 1 帧处理中）──
    QMutex m_pendingMutex;
    Frame m_pendingFrame;
    bool m_hasPendingFrame = false;
    std::atomic<bool> m_draining{false};
};
