#include "QtWidgetsApplication1.h"
#include "ui/ImageViewport.h"
#include "ui/ControlPanel.h"
#include "ui/CameraSettingsWidget.h"
#include "hal/DeviceEnumerator.h"
#include "hal/LibuvcCameraDevice.h"
#include "hal/UvcControls.h"
#include "protocol/StandardUvcProtocol.h"
#include "protocol/CustomUvcProtocol.h"
#include "infra/LogManager.h"
#include "infra/ConfigManager.h"
#include "infra/UiStrings.h"
#include "core/CameraTypes.h"

#include <QMenuBar>
#include <QStatusBar>
#include <QMessageBox>
#include <QVBoxLayout>
#include <QFileDialog>
#include <QDateTime>
#include <QCloseEvent>
#include <QApplication>
#include <libuvc/libuvc.h>
#include <opencv2/opencv.hpp>
#include <opencv2/imgproc.hpp>

QtWidgetsApplication1::QtWidgetsApplication1(QWidget* parent)
    : QMainWindow(parent)
{
    setupUi();
    setupStyleSheet();
    setupMenuBar();
    setupStatusBar();
    connectSignals();

    setWindowTitle("UVC Camera");

    // Init libuvc context
    if (!DeviceEnumerator::instance().initialize()) {
        QMessageBox::critical(this, TR("Error"),
            TR("Failed to initialize libuvc.\nCheck USB driver installation."));
    }

    // Stats timer (4 Hz)
    m_statsTimer = new QTimer(this);
    m_statsTimer->setInterval(250);
    connect(m_statsTimer, &QTimer::timeout, this, &QtWidgetsApplication1::updateStats);
    m_statsTimer->start();
    m_statsElapsed.start();

    // Auto-refresh on startup
    onRefreshDevices();

    LOG_INFO("Application started");
}

QtWidgetsApplication1::~QtWidgetsApplication1() {
    stopAll();
    DeviceEnumerator::instance().shutdown();
}

void QtWidgetsApplication1::closeEvent(QCloseEvent* event) {
    stopAll();
    event->accept();
}

void QtWidgetsApplication1::setupUi() {
    m_splitter = new QSplitter(Qt::Horizontal);
    m_splitter->setHandleWidth(1);

    m_viewport = new ImageViewport;
    m_controlPanel = new ControlPanel;

    m_splitter->addWidget(m_viewport);
    m_splitter->addWidget(m_controlPanel);
    m_splitter->setStretchFactor(5, 6);  // viewport weight
    m_splitter->setStretchFactor(5, 2);  // control panel weight
    m_splitter->setChildrenCollapsible(false);

    setCentralWidget(m_splitter);
    resize(1920, 1080);  // initial window size
}

void QtWidgetsApplication1::setupStyleSheet() {
    setStyleSheet(QStringLiteral(R"(
        QWidget {
            background-color: #1E1E1E;
            color: #CCCCCC;
            font-family: "Segoe UI", "Microsoft YaHei", sans-serif;
            font-size: 13px;
        }
        QMainWindow::separator { background-color: #3E3E42; width: 1px; }
        QSplitter::handle { background-color: #3E3E42; }
        QMenuBar { background-color: #1E1E1E; border-bottom: 1px solid #3E3E42; }
        QMenuBar::item:selected { background-color: #3C3C3C; }
        QMenu { background-color: #252526; border: 1px solid #3E3E42; }
        QMenu::item:selected { background-color: #094771; }
        QPushButton {
            background-color: #2D2D2D; border: 1px solid #3E3E42;
            border-radius: 4px; padding: 6px 12px; color: #CCCCCC;
        }
        QPushButton:hover { background-color: #3C3C3C; border: 1px solid #26C0A6; }
        QPushButton:pressed { background-color: #26C0A6; color: #1E1E1E; }
        QPushButton:disabled { color: #555555; }
        QPushButton#openBtn { font-weight: bold; }
        QPushButton#applyBtn { font-weight: bold; }
        QPushButton#captureBtn { font-size: 14px; font-weight: bold; }
        QComboBox, QLineEdit {
            background-color: #3C3C3C; border: 1px solid #3E3E42;
            padding: 4px; border-radius: 2px; color: #CCCCCC;
        }
        QComboBox:focus, QLineEdit:focus { border: 1px solid #26C0A6; }
        QComboBox::drop-down { border: none; }
        QComboBox QAbstractItemView {
            background-color: #252526; border: 1px solid #3E3E42;
            selection-background-color: #094771; color: #CCCCCC;
        }
        QCheckBox { spacing: 6px; }
        QCheckBox::indicator {
            width: 16px; height: 16px; border: 1px solid #3E3E42;
            border-radius: 2px; background-color: #3C3C3C;
        }
        QCheckBox::indicator:checked { background-color: #26C0A6; border-color: #26C0A6; }
        QStatusBar { background-color: #007ACC; color: #FFFFFF; border-top: 1px solid #3E3E42; }
        QStatusBar::item { border: none; }
        QScrollBar:vertical { background: #1E1E1E; width: 10px; }
        QScrollBar::handle:vertical { background: #424242; min-height: 20px; border-radius: 5px; }
        QScrollBar::handle:vertical:hover { background: #4E4E4E; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
    )"));
}

void QtWidgetsApplication1::setupMenuBar() {
    auto* fileMenu = menuBar()->addMenu(TR("&File"));
    fileMenu->addAction(TR("&Load Config..."), [this]() {
        QString path = QFileDialog::getOpenFileName(this, TR("&Load Config..."), "", "JSON (*.json)");
        if (!path.isEmpty()) ConfigManager::instance().load(path);
    });
    fileMenu->addAction(TR("&Save Config..."), [this]() {
        QString path = QFileDialog::getSaveFileName(this, TR("&Save Config..."), "", "JSON (*.json)");
        if (!path.isEmpty()) ConfigManager::instance().save(path);
    });
    fileMenu->addSeparator();
    fileMenu->addAction(TR("E&xit"), this, &QWidget::close);

    menuBar()->addMenu(TR("&View"))->addAction(TR("&Fit to Window"));
    menuBar()->addMenu(TR("&Capture"))->addAction(TR("&Snapshot"), this, &QtWidgetsApplication1::onSnapshot);
    menuBar()->addMenu(TR("&Help"))->addAction(TR("&About"));
}

void QtWidgetsApplication1::setupStatusBar() {
    m_fpsLabel          = new QLabel(TR("FPS: --"));
    m_deviceStatusLabel = new QLabel(TR("Device: Disconnected"));
    m_bandwidthLabel    = new QLabel(TR("Rx: -- MB/s"));
    m_droppedLabel      = new QLabel(TR("Dropped: 0"));

    for (auto* lbl : {m_fpsLabel, m_deviceStatusLabel, m_bandwidthLabel, m_droppedLabel}) {
        lbl->setStyleSheet("color:#FFFFFF; padding: 0 10px;");
    }
    m_droppedLabel->setStyleSheet("color:#FF6666; padding: 0 10px;");

    statusBar()->addWidget(m_fpsLabel);
    statusBar()->addWidget(m_deviceStatusLabel);
    statusBar()->addWidget(m_bandwidthLabel);
    statusBar()->addWidget(m_droppedLabel);
}

void QtWidgetsApplication1::connectSignals() {
    // Control panel
    connect(m_controlPanel, &ControlPanel::refreshDevices,
            this, &QtWidgetsApplication1::onRefreshDevices);
    connect(m_controlPanel->openBtn(), &QPushButton::clicked,
            this, &QtWidgetsApplication1::onOpenDevice);
    connect(m_controlPanel->applyBtn(), &QPushButton::clicked,
            this, &QtWidgetsApplication1::onApplyStream);
    connect(m_controlPanel->snapshotBtn(), &QPushButton::clicked,
            this, &QtWidgetsApplication1::onSnapshot);
    connect(m_controlPanel->recordBtn(), &QPushButton::toggled,
            this, &QtWidgetsApplication1::onToggleRecord);

    // 16-bit shift
    connect(m_controlPanel->bitShiftSlider(), &QSlider::valueChanged,
            this, [this](int val) { m_bitShift = val; });

    // Log
    connect(&LogManager::instance(), &LogManager::newEntry, this,
        [this](const LogManager::Entry& entry) {
            static const char* colors[] = {"#858585", "#26C0A6", "#CCA700", "#F14C4C"};
            QString html = QString("<span style='color:%1'>[%2] %3</span>")
                .arg(colors[entry.level], entry.timestamp, entry.message);
            m_controlPanel->logView()->append(html);
        });
}

// ── Device enumeration ──

void QtWidgetsApplication1::onRefreshDevices() {
    LOG_INFO("Refreshing device list...");
    m_controlPanel->deviceCombo()->clear();

    // Release old raw pointers
    for (auto* dev : m_rawDeviceList)
        uvc_unref_device(static_cast<uvc_device_t*>(dev));
    m_rawDeviceList.clear();

    auto devices = DeviceEnumerator::instance().enumerate();

    if (devices.isEmpty()) {
        m_controlPanel->deviceCombo()->addItem(TR("-- No UVC devices found --"));
        return;
    }

    for (const auto& dev : devices) {
        QString label = QString("%1 [%2:%3] @ bus %4 addr %5")
            .arg(dev.name)
            .arg(dev.vid, 4, 16, QChar('0'))
            .arg(dev.pid, 4, 16, QChar('0'))
            .arg(dev.bus)
            .arg(dev.address);

        m_controlPanel->deviceCombo()->addItem(label,
            QVariant::fromValue(reinterpret_cast<quintptr>(dev.raw_device)));
        m_rawDeviceList.push_back(dev.raw_device);
    }
}

// ── Device open/close ──

void QtWidgetsApplication1::onOpenDevice() {
    if (m_deviceOpen) {
        // Close
        stopAll();
        m_camera.reset();
        m_protocol.reset();
        m_uvcControls.reset();
        m_controlPanel->cameraSettings()->clearControls();

        m_deviceOpen = false;
        m_controlPanel->setDeviceOpen(false);
        m_viewport->clearImage();
        m_viewport->setOverlayText("");
        m_deviceStatusLabel->setText(TR("Device: Disconnected"));
        m_fpsLabel->setText(TR("FPS: --"));
        m_bandwidthLabel->setText(TR("Rx: -- MB/s"));
        m_droppedLabel->setText(TR("Dropped: 0"));
        LOG_INFO("Device closed");
        return;
    }

    // Open
    int idx = m_controlPanel->deviceCombo()->currentIndex();
    if (idx < 0) return;

    auto rawDev = reinterpret_cast<uvc_device_t*>(
        m_controlPanel->deviceCombo()->currentData().value<quintptr>());
    if (!rawDev) {
        LOG_ERROR(TR("Invalid device selection").toStdString().c_str());
        return;
    }

    m_camera = std::make_unique<LibuvcCameraDevice>();
    m_camera->setRawDevice(rawDev);

    if (!m_camera->open()) {
        QString msg = TR("Failed to open camera device.\n\n%1"
                         "\n\nOn Windows, libuvc requires a WinUSB/libusbK driver.\n"
                         "Use Zadig (https://zadig.akeo.ie) to replace the driver.")
                          .arg(m_camera->lastError());
        QMessageBox::critical(this, TR("Error"), msg);
        m_camera.reset();
        return;
    }

    // Connect signals
    connect(m_camera.get(), &LibuvcCameraDevice::frameReady,
            this, &QtWidgetsApplication1::onFrameReady);
    connect(m_camera.get(), &LibuvcCameraDevice::deviceLost,
            this, &QtWidgetsApplication1::onDeviceLost);
    connect(m_camera.get(), &LibuvcCameraDevice::streamingError,
            this, &QtWidgetsApplication1::onStreamError);

    // Init protocol (default: standard)
    m_protocol = std::make_unique<StandardUvcProtocol>();
    m_protocol->initialize(m_camera.get());

    // Init UVC controls
    m_uvcControls = std::make_unique<UvcControls>(m_camera->deviceHandle());
    m_controlPanel->cameraSettings()->setControls(m_uvcControls.get());

    m_deviceOpen = true;
    m_controlPanel->setDeviceOpen(true);

    // Populate format/resolution combos
    populateFormats();

    // Show device info
    auto info = m_camera->getDeviceInfo();
    m_controlPanel->deviceInfoLabel()->setText(
        TR("VID:%1 PID:%2\n%3")
            .arg(info.vendor_id, 4, 16, QChar('0'))
            .arg(info.product_id, 4, 16, QChar('0'))
            .arg(QString::fromStdString(info.name)));

    m_deviceStatusLabel->setText(
        TR("Device: %1").arg(QString::fromStdString(info.name)));
    LOG_INFO(QString("Device opened: %1").arg(QString::fromStdString(info.name)));
}

void QtWidgetsApplication1::populateFormats() {
    m_controlPanel->formatCombo()->clear();
    m_controlPanel->resolutionCombo()->clear();

    if (!m_camera) return;

    auto formats = m_camera->getSupportedFormats();
    if (formats.empty()) {
        // Fallback defaults
        m_controlPanel->formatCombo()->addItem("Y16");
        m_controlPanel->formatCombo()->addItem("Y800");
        m_controlPanel->formatCombo()->addItem("YUYV");
        m_controlPanel->formatCombo()->addItem("MJPEG");
    } else {
        for (auto& fmt : formats) {
            char fourcc[5] = {};
            memcpy(fourcc, &fmt.fourcc, 4);
            QString label = QString("%1  %2x%3 @%4fps")
                .arg(QString::fromLatin1(fourcc, 4).trimmed())
                .arg(fmt.width).arg(fmt.height).arg(fmt.fps);
            m_controlPanel->formatCombo()->addItem(label);
        }
    }
}

// ── Stream start/stop ──

void QtWidgetsApplication1::onApplyStream() {
    if (!m_deviceOpen || !m_camera) return;

    if (m_streaming) {
        // Stop
        m_camera->stopStreaming();
        m_streaming = false;
        m_controlPanel->setStreaming(false);
        m_viewport->setOverlayText("");
        LOG_INFO("Streaming stopped");
        return;
    }

    // Get selected format
    auto currentText = m_controlPanel->formatCombo()->currentText();
    auto formats = m_camera->getSupportedFormats();

    CameraFormat fmt;
    if (!formats.empty() && m_controlPanel->formatCombo()->currentIndex() < (int)formats.size()) {
        fmt = formats[m_controlPanel->formatCombo()->currentIndex()];
    } else {
        // Default: 1280x1024 Y16
        fmt.width = 1280;
        fmt.height = 1024;
        const char* y16 = "Y16 ";
        memcpy(&fmt.fourcc, y16, 4);
        fmt.fps = 30;
    }

    m_camera->setFormat(fmt);

    QApplication::setOverrideCursor(Qt::WaitCursor);
    bool ok = m_camera->startStreaming();
    QApplication::restoreOverrideCursor();

    if (!ok) {
        QMessageBox::critical(this, TR("Error"), TR("Failed to start streaming."));
        return;
    }

    m_streaming = true;
    m_controlPanel->setStreaming(true);
    m_displayFrameCount = 0;
    m_lastFrameBytes = 0;
    m_lastFrameCount = 0;

    LOG_INFO(QString("Streaming started: %1x%2")
        .arg(fmt.width).arg(fmt.height));
}

// ── Frame processing pipeline ──

void QtWidgetsApplication1::onFrameReady(const Frame& frame) {
    if (!m_streaming) return;

    // Diagnostic: log first few frames
    if (m_displayFrameCount < 3) {
        LOG_INFO(QString("[Frame %1] raw: %2 %3x%4 %5 bytes")
            .arg(m_displayFrameCount)
            .arg(QString::fromStdString(frame.format))
            .arg(frame.width).arg(frame.height)
            .arg(frame.data.size()));
    }

    // Protocol: raw → processed
    ProcessedFrame parsed;
    if (m_protocol && !m_protocol->parseFrame(frame, parsed)) {
        if (m_displayFrameCount < 3)
            LOG_WARNING(QString("[Frame %1] parse failed for format %2")
                .arg(m_displayFrameCount)
                .arg(QString::fromStdString(frame.format)));
        return;
    }
    if (!m_protocol)
        parsed = {}; // shouldn't happen

    if (!parsed.valid) {
        if (m_displayFrameCount < 3)
            LOG_WARNING(QString("[Frame %1] parsed frame invalid").arg(m_displayFrameCount));
        return;
    }

    // Keep last frame for snapshot
    m_lastFrame = parsed;

    if (m_displayFrameCount < 3)
        LOG_INFO(QString("[Frame %1] parsed: type=%2 %3x%4 %5 bytes")
            .arg(m_displayFrameCount)
            .arg(parsed.cv_type)
            .arg(parsed.width).arg(parsed.height)
            .arg(parsed.data.size()));

    // Convert to QImage and display
    QImage img = frameToQImage(parsed);
    if (!img.isNull()) {
        m_viewport->setImage(img);

        // HUD overlay
        m_displayFrameCount++;
        auto now = QDateTime::currentDateTime();
        m_viewport->setOverlayText(
            QString("%1  Frame: %2")
                .arg(now.toString("hh:mm:ss.zzz"))
                .arg(m_displayFrameCount));
    }

    // Recording
    if (m_recording && !img.isNull()) {
        QString filename = QString("record_%1_%2.png")
            .arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss"))
            .arg(m_recordCounter++, 5, 10, QChar('0'));
        img.save(filename, "PNG");
    }
}

QImage QtWidgetsApplication1::frameToQImage(const ProcessedFrame& frame) {
    if (!frame.valid || frame.data.empty()) return {};

    int w = static_cast<int>(frame.width);
    int h = static_cast<int>(frame.height);

    if (frame.cv_type == CV_8UC3) {
        // BGR (OpenCV default) → RGB for QImage
        cv::Mat bgr(h, w, CV_8UC3, const_cast<uint8_t*>(frame.data.data()));
        cv::Mat rgb;
        cv::cvtColor(bgr, rgb, cv::COLOR_BGR2RGB);
        return QImage(rgb.data, rgb.cols, rgb.rows, rgb.step,
                      QImage::Format_RGB888).copy();

    } else if (frame.cv_type == CV_16UC1) {
        // 16-bit → extract 8-bit slice per slider position
        auto* src16 = reinterpret_cast<const uint16_t*>(frame.data.data());
        size_t n = static_cast<size_t>(w) * h;
        int shift = m_bitShift;
        std::vector<uint8_t> buf8(n);
        for (size_t i = 0; i < n; i++)
            buf8[i] = static_cast<uint8_t>((src16[i] >> shift) & 0xFF);
        return QImage(buf8.data(), w, h, QImage::Format_Grayscale8).copy();

    } else {
        // CV_8UC1 or unknown → grayscale passthrough
        return QImage(frame.data.data(), w, h, QImage::Format_Grayscale8).copy();
    }
}

// ── Snapshot (all formats → TIFF via cv::imwrite) ──

void QtWidgetsApplication1::onSnapshot() {
    if (!m_lastFrame.valid) return;

    QString ts = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");
    QString filename = QString("snapshot_%1.tiff").arg(ts);

    int w = static_cast<int>(m_lastFrame.width);
    int h = static_cast<int>(m_lastFrame.height);

    cv::Mat img(h, w, m_lastFrame.cv_type,
                const_cast<uint8_t*>(m_lastFrame.data.data()));
    cv::imwrite(filename.toStdString(), img);

    const char* desc = (m_lastFrame.cv_type == CV_16UC1) ? "16-bit TIFF"
                     : (m_lastFrame.cv_type == CV_8UC3)  ? "BGR TIFF"
                     : "Grayscale TIFF";
    LOG_INFO(QString("Snapshot saved: %1 (%2)").arg(filename, desc));
    m_snapshotCounter++;
}

void QtWidgetsApplication1::onToggleRecord(bool start) {
    m_recording = start;
    m_recordCounter = 0;
    if (start) {
        m_controlPanel->recordBtn()->setText(TR("Stop Recording"));
        LOG_INFO("Recording started...");
    } else {
        m_controlPanel->recordBtn()->setText(TR("Record Sequence"));
        LOG_INFO("Recording stopped.");
    }
}

// ── Error handling ──

void QtWidgetsApplication1::onDeviceLost() {
    LOG_ERROR("Device lost! USB disconnected.");
    stopAll();
    m_deviceOpen = false;
    m_streaming = false;
    m_viewport->clearImage();
    m_deviceStatusLabel->setText(TR("Device: Disconnected"));
    m_controlPanel->setDeviceOpen(false);
    m_controlPanel->setStreaming(false);

    QMessageBox::critical(this, TR("Device Lost"),
        TR("The camera device was disconnected.\nAll controls have been released."));
}

void QtWidgetsApplication1::onStreamError(const QString& error) {
    LOG_ERROR(QString("Stream error: %1").arg(error));
}

// ── Stats update ──

void QtWidgetsApplication1::updateStats() {
    if (!m_camera || !m_streaming) return;

    // Read from camera stats
    uint64_t totalBytes = m_camera->totalBytes();
    uint32_t totalFrames = m_camera->totalFrames();
    uint32_t dropped = m_camera->droppedFrames();

    double elapsed = m_statsElapsed.elapsed() / 1000.0;
    if (elapsed < 0.1) return;

    double fps = (totalFrames - m_lastFrameCount) / elapsed;
    double mbps = (totalBytes - m_lastFrameBytes) / elapsed / (1024.0 * 1024.0);

    m_fpsLabel->setText(QString("FPS: %1").arg(fps, 0, 'f', 1));
    m_bandwidthLabel->setText(QString("Rx: %1 MB/s").arg(mbps, 0, 'f', 1));
    m_droppedLabel->setText(QString("Dropped: %1").arg(dropped));

    m_lastFrameBytes = totalBytes;
    m_lastFrameCount = totalFrames;
    m_statsElapsed.restart();
}

// ── Helpers ──

void QtWidgetsApplication1::stopAll() {
    if (m_camera) {
        m_camera->stopStreaming();
    }
    m_streaming = false;
    m_recording = false;
}
