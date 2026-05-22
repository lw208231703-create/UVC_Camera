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
    struct uvc_device* rawDevice() const { return m_rawDevice; }
    struct uvc_device_handle* deviceHandle() const { return m_devh; }

    // Stats accessors
    uint64_t totalBytes() const    { return m_totalBytes; }
    uint32_t totalFrames() const   { return m_totalFrames; }
    uint32_t droppedFrames() const { return m_droppedFrames; }
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
    uint32_t              m_lastFrameSequence = 0;
};
