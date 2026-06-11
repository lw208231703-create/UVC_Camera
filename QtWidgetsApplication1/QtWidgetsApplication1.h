#pragma once

#include <QtWidgets/QMainWindow>
#include <QSplitter>
#include <QLabel>
#include <QTimer>
#include <QElapsedTimer>
#include <QThread>
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
    void onSnapshot();
    void onToggleRecord(bool start);
    void onDeviceLost();
    void onStreamError(const QString& error);
    void updateStats();

private:
    void setupUi();
    void setupStyleSheet();
    void setupMenuBar();
    void setupStatusBar();
    void connectSignals();
    void closeEvent(QCloseEvent* event) override;

    void populateFormats();
    void stopAll();

    // UI
    QSplitter*     m_splitter;
    ImageViewport* m_viewport;
    ControlPanel*  m_controlPanel;

    QLabel* m_fpsLabel;
    QLabel* m_deviceStatusLabel;
    QLabel* m_bandwidthLabel;
    QLabel* m_droppedLabel;

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
    bool m_recording = false;
    ProcessedFrame m_lastFrame;
    int m_bitShift = 8;  // 16-bit display: bit shift (8=MSB, 0=LSB)

    // Stats
    QTimer*         m_statsTimer;
    QElapsedTimer   m_statsElapsed;
    uint64_t        m_lastFrameBytes = 0;
    uint32_t        m_lastFrameCount = 0;
    uint32_t        m_displayFrameCount = 0;

    // Snapshot counter
    int m_snapshotCounter = 0;
    int m_recordCounter = 0;
};
