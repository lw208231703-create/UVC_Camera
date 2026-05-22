#pragma once

#include <QtWidgets/QMainWindow>
#include <QSplitter>
#include <QLabel>
#include <QTimer>
#include <QElapsedTimer>
#include <memory>
#include <vector>

class ImageViewport;
class ControlPanel;
class LibuvcCameraDevice;
class IProtocolHandler;
class IImageProcessor;

struct CameraFormat;
struct Frame;
struct ProcessedFrame;

class QtWidgetsApplication1 : public QMainWindow {
    Q_OBJECT

public:
    explicit QtWidgetsApplication1(QWidget* parent = nullptr);
    ~QtWidgetsApplication1();

private slots:
    void onRefreshDevices();
    void onOpenDevice();
    void onApplyStream();
    void onFrameReady(const Frame& frame);
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

    // Frame processing pipeline
    void processFrame(const Frame& frame);
    QImage frameToQImage(const ProcessedFrame& frame);

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

    // Protocol & processing
    std::unique_ptr<IProtocolHandler> m_protocol;
    std::unique_ptr<IImageProcessor>  m_processor;

    // State
    bool m_deviceOpen = false;
    bool m_streaming = false;
    bool m_recording = false;

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
