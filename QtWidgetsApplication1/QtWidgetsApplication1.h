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
#include <QFileDialog>
#include <memory>
#include <vector>

class ImageViewport;
class ControlPanel;
class LibuvcCameraDevice;
class IProtocolHandler;
class ProcessingWorker;
class FTI2cBridge;
class ParameterWorker;
class BurstWorker;
class StatsWorker;

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
    void onBurst();
    void onBurstBrowse();

    void onDeviceLost();
    void onStreamError(const QString& error);
    void onInstallDriver();
    bool isWinUsbDriverInstalled(uint16_t vid, uint16_t pid);
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
    // ── I2C 参数工作线程 ──
    QThread*        m_paramThread = nullptr;
    ParameterWorker* m_paramWorker = nullptr;

    // ── 连拍 worker（独立线程）──
    QThread*     m_burstThread = nullptr;
    BurstWorker* m_burstWorker = nullptr;

    // ── 统计 worker（独立线程，滑动平均采样）──
    QThread*     m_statsThread = nullptr;
    StatsWorker* m_statsWorker = nullptr;

    // ── I2C 调试桥（直通，仅用于调试面板）──
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
    int m_bitShift = 4;          // 16-bit display: bit shift (4=MSB, 0=LSB)
    bool m_denoiseEnabled = true; // 降噪开关

    // ── 显示帧单槽位（worker → UI 合帧，防止主线程事件队列堆积帧拷贝）──
    // onFrameProcessed (DirectConnection, worker 线程) 写入；drainDisplay (主线程) 取出。
    QMutex m_displayMutex;
    QImage m_pendingImage;
    ProcessedFrame m_pendingParsed;
    bool m_hasDisplayFrame = false;
    std::atomic<bool> m_displayDraining{false};

    // Stats
    QTimer*         m_temperatureTimer;
    QElapsedTimer   m_statsElapsed;
    uint32_t        m_displayFrameCount = 0;
    std::atomic<uint64_t> m_statsFrameCount{0};  // 显示帧数 (renderFrame 递增)
    std::atomic<uint64_t> m_statsByteCount{0};   // 相机字节数

    // Snapshot counter
    int m_snapshotCounter = 0;
};
