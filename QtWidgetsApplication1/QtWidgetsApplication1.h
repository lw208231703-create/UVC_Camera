#pragma once

#include <QtWidgets/QMainWindow>
#include <QSplitter>
#include <QLabel>
#include <QTimer>
#include <QElapsedTimer>
#include <QList>
#include <QThread>
#include <QImage>
#include <QMutex>
#include <atomic>
#include <memory>
#include <vector>

class ImageViewport;
class ControlPanel;
class LibuvcCameraDevice;
class IProtocolHandler;
class ProcessingWorker;
class FTI2cBridge;

#include "core/CameraTypes.h"

class QtWidgetsApplication1 : public QMainWindow {
    Q_OBJECT

public:
    explicit QtWidgetsApplication1(QWidget* parent = nullptr);
    ~QtWidgetsApplication1();

private slots:
    void onRefreshDevices();
    void onOpenDevice();
    void onApplyStream();
    void onFrameProcessed(QImage img, ProcessedFrame parsed);
    void drainDisplay();
    void onSnapshot();

    void onDeviceLost();
    void onStreamError(const QString& error);
    void updateStats();
    void updateDetectorTemperature();

private:
    void setupUi();
    void setupStyleSheet();
    void setupStatusBar();
    void connectSignals();
    void closeEvent(QCloseEvent* event) override;

    void populateFormats();
    void stopAll();
    void clearFramePipeline();
    void renderFrame(QImage img, ProcessedFrame parsed);

    // UI
    QSplitter*     m_splitter;
    ImageViewport* m_viewport;
    ControlPanel*  m_controlPanel;

    QLabel* m_fpsLabel;         // 相机回调 FPS
    QLabel* m_dispFpsLabel;     // 显示 FPS
    QLabel* m_bandwidthLabel;
    QLabel* m_temperatureLabel;

    // Hardware
    std::unique_ptr<LibuvcCameraDevice> m_camera;
    std::vector<void*> m_rawDeviceList;
    std::vector<int>   m_cameraIndexList; // camera_idx per device entry

    // Protocol & processing
    std::unique_ptr<IProtocolHandler> m_protocol;
    std::unique_ptr<class UvcControls> m_uvcControls;
    std::unique_ptr<FTI2cBridge> m_i2cBridge;

    // ── 独立帧处理线程 ──
    // Worker 线程在应用启动时创建，生命周期与窗口一致。
    // 帧的协议解析 + QImage 转换在此线程完成，不阻塞 UI。
    QThread*                m_workerThread = nullptr;
    ProcessingWorker*       m_worker = nullptr;

    // State
    bool m_deviceOpen = false;
    bool m_streaming = false;

    ProcessedFrame m_lastFrame;
    int m_bitShift = 8;          // 16-bit display: bit shift (8=MSB, 0=LSB)
    bool m_denoiseEnabled = true; // 降噪开关

    // ── 显示帧单槽位（worker → UI 合帧，防止主线程事件队列堆积帧拷贝）──
    // onFrameProcessed (DirectConnection, worker 线程) 写入；drainDisplay (主线程) 取出。
    QMutex m_displayMutex;
    QImage m_pendingImage;
    ProcessedFrame m_pendingParsed;
    bool m_hasDisplayFrame = false;
    std::atomic<bool> m_displayDraining{false};

    // Stats
    QTimer*         m_statsTimer;
    QTimer*         m_temperatureTimer;
    QElapsedTimer   m_statsElapsed;
    uint32_t        m_displayFrameCount = 0;
    uint64_t        m_statsFrameCount  = 0;   // 显示帧数 (renderFrame 递增)
    uint64_t        m_statsByteCount   = 0;   // 相机字节数
    qint64          m_lastStatsSampleTime  = 0; // 上次采样时间 (ms)
    uint64_t        m_lastStatsSampleFrames = 0;  // 上次采样显示帧数
    uint64_t        m_lastStatsSampleBytes  = 0;  // 上次采样字节数
    uint64_t        m_lastStatsSampleCamFrames = 0; // 上次采样相机帧数
    int             m_statsTickCounter  = 0;

    // Snapshot counter
    int m_snapshotCounter = 0;
};
