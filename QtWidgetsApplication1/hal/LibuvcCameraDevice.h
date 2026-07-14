#pragma once

#include "core/ICameraDevice.h"
#include "core/CameraTypes.h"
#include <QObject>
#include <QMutex>
#include <QQueue>
#include <atomic>

struct uvc_device;
struct uvc_device_handle;
struct uvc_stream_ctrl;

class LibuvcCameraDevice : public QObject, public ICameraDevice {
    Q_OBJECT

public:
    explicit LibuvcCameraDevice(QObject* parent = nullptr);
    ~LibuvcCameraDevice() override;

    // ICameraDevice
    bool open() override;
    void close() override;
    bool isOpen() const override;

    std::vector<CameraFormat> getSupportedFormats() const override;
    bool setFormat(const CameraFormat& format) override;
    CameraFormat getCurrentFormat() const override;

    bool startStreaming() override;
    void stopStreaming() override;
    bool isStreaming() const override;

    bool readFrame(Frame& frame) override;

    CameraInfo getDeviceInfo() const override;

    // Set the raw uvc_device pointer from enumeration
    void setRawDevice(struct uvc_device* dev);
    void setCameraIndex(int idx) { m_cameraIndex = idx; }
    int cameraIndex() const { return m_cameraIndex; }
    struct uvc_device* rawDevice() const { return m_rawDevice; }
    struct uvc_device_handle* deviceHandle() const { return m_devh; }

    // Stats accessors
    uint64_t totalBytes() const    { return m_totalBytes; }
    uint32_t totalFrames() const   { return m_totalFrames; }
    uint32_t droppedFrames() const { return m_droppedFrames; }
    uint32_t realDroppedFrames() const { return m_realDroppedFrames; }
    uint32_t minorShortFrames() const  { return m_minorShortFrames; }
    QString lastError() const         { return m_lastError; }

signals:
    void frameReady(const Frame& frame);
    void deviceLost();
    void streamingError(const QString& error);

private:
    static void frameCallback(struct uvc_frame* uvcFrame, void* userPtr);

    void dispatchFrame(Frame&& frame);

    struct uvc_device*        m_rawDevice = nullptr;
    struct uvc_device_handle* m_devh = nullptr;
    struct uvc_stream_ctrl*   m_streamCtrl = nullptr;
    int                       m_cameraIndex = 0; // which VC interface to open
    CameraFormat       m_currentFormat;
    CameraInfo         m_deviceInfo;
    QString            m_lastError;

    std::atomic<bool>  m_streaming{false};
    std::atomic<bool>  m_opened{false};

    // Frame buffer pool
    QQueue<Frame>      m_freeBuffers;
    QQueue<Frame>      m_readyBuffers;
    mutable QMutex     m_bufferMutex;

    // Stats
    std::atomic<uint64_t> m_totalBytes{0};
    std::atomic<uint32_t> m_totalFrames{0};
    std::atomic<uint32_t> m_droppedFrames{0};
    std::atomic<uint32_t> m_realDroppedFrames{0};   // diff > kRealDropThreshold: 真实丢包
    std::atomic<uint32_t> m_minorShortFrames{0};     // diff <= kHeaderTolerance: UVC header 或微小容差
    std::atomic<uint32_t> m_callbackLogCount{0};
    std::atomic<uint32_t> m_statsLogCounter{0};      // 周期性统计日志计数
    uint32_t              m_lastFrameSequence = 0;

    // 尺寸容差常量
    // UVC payload header = 12 字节; diff <= 此值视为正常 (header 未剥或微小时序抖动)
    static constexpr int64_t kHeaderTolerance = 16;
    // diff > 此值视为真实丢包 (USB 传输截断/FIFO 溢出)
    static constexpr int64_t kRealDropThreshold = 1024;

    // Warmup: skip first N frames after stream start (USB pipe not yet stable)
    static constexpr uint32_t kWarmupFrames = 1;
    std::atomic<uint32_t> m_warmupCounter{0};
};
