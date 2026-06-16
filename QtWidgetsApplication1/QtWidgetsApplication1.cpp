#include "QtWidgetsApplication1.h"
#include "ui/ImageViewport.h"
#include "ui/ControlPanel.h"
#include "ui/CameraSettingsWidget.h"
#include "hal/DeviceEnumerator.h"
#include "hal/LibuvcCameraDevice.h"
#include "hal/UvcControls.h"
#include "hal/FTI2cBridge.h"
#include "protocol/StandardUvcProtocol.h"
#include "protocol/CustomUvcProtocol.h"
#include "ProcessingWorker.h"
#include "infra/LogManager.h"
#include "infra/ConfigManager.h"
#include "infra/UiStrings.h"
#include "core/CameraTypes.h"

#include <algorithm>
#include <QMenuBar>
#include <QStatusBar>
#include <QMessageBox>
#include <QVBoxLayout>
#include <QFileDialog>
#include <QDateTime>
#include <QCloseEvent>
#include <QApplication>
#include <QMetaType>
#include <libuvc/libuvc.h>
#include <opencv2/opencv.hpp>
#include <opencv2/imgproc.hpp>

// 注册 ProcessedFrame 为 Qt 元类型，支持跨线程 QueuedConnection
Q_DECLARE_METATYPE(ProcessedFrame)

QtWidgetsApplication1::QtWidgetsApplication1(QWidget* parent)
    : QMainWindow(parent)
{
    qRegisterMetaType<ProcessedFrame>("ProcessedFrame");

    setupUi();
    setupStyleSheet();
    setupMenuBar();
    setupStatusBar();
    connectSignals();

    setWindowTitle("UVC Camera");

    // ── 创建独立的帧处理线程 ──
    m_workerThread = new QThread(this);
    m_worker = new ProcessingWorker;  // no parent — will be moved
    m_worker->moveToThread(m_workerThread);

    // Worker → 主线程：处理好的帧送显
    connect(m_worker, &ProcessingWorker::frameDisplayReady,
            this, &QtWidgetsApplication1::onFrameProcessed,
            Qt::QueuedConnection);

    m_workerThread->start();

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

    // 清理帧处理线程
    if (m_workerThread) {
        m_workerThread->quit();
        m_workerThread->wait(3000);
    }
    delete m_worker;      // worker 没有 parent，手动删除
    m_worker = nullptr;
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

    // 16-bit shift — 同步到 worker 线程
    connect(m_controlPanel->bitShiftSlider(), &QSlider::valueChanged,
            this, [this](int val) {
        m_bitShift = val;
        if (m_worker)
            m_worker->setBitShift(val);
    });

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
    m_cameraIndexList.clear();

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
        if (dev.cameraCount > 1)
            label += QString(" ch%1/%2").arg(dev.cameraIndex).arg(dev.cameraCount);

        m_controlPanel->deviceCombo()->addItem(label,
            QVariant::fromValue(reinterpret_cast<quintptr>(dev.raw_device)));
        m_rawDeviceList.push_back(dev.raw_device);
        m_cameraIndexList.push_back(dev.cameraIndex);
    }
}

// ── Device open/close ──

void QtWidgetsApplication1::onOpenDevice() {
    if (m_deviceOpen) {
        // Close
        stopAll();
        m_camera.reset();
        // 清空 worker 中的协议指针，防止悬空引用
        if (m_worker) m_worker->setProtocol(nullptr);
        m_protocol.reset();
        m_uvcControls.reset();
        m_i2cBridge.reset();
        m_controlPanel->cameraSettings()->clearControls();
        m_controlPanel->setI2cEnabled(false);

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

    // Get camera index for multi-channel devices
    int cameraIdx = (idx < (int)m_cameraIndexList.size()) ? m_cameraIndexList[idx] : 0;

    m_camera = std::make_unique<LibuvcCameraDevice>();
    m_camera->setRawDevice(rawDev);
    m_camera->setCameraIndex(cameraIdx);

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
    // frameReady → worker 线程（帧处理在独立线程进行，不阻塞 UI）
    connect(m_camera.get(), &LibuvcCameraDevice::frameReady,
            m_worker, &ProcessingWorker::processFrame,
            Qt::QueuedConnection);
    connect(m_camera.get(), &LibuvcCameraDevice::deviceLost,
            this, &QtWidgetsApplication1::onDeviceLost);
    connect(m_camera.get(), &LibuvcCameraDevice::streamingError,
            this, &QtWidgetsApplication1::onStreamError);

    // Init protocol (default: standard)
    m_protocol = std::make_unique<StandardUvcProtocol>();
    m_protocol->initialize(m_camera.get());

    // 将协议处理器指针传给 worker 线程
    m_worker->setProtocol(m_protocol.get());

    // Init UVC controls
    m_uvcControls = std::make_unique<UvcControls>(m_camera->deviceHandle());
    m_controlPanel->cameraSettings()->setControls(m_uvcControls.get());

    // Init I2C bridge + enable panel
    m_i2cBridge = std::make_unique<FTI2cBridge>(m_uvcControls->libusbHandle(), 0x0D);
    m_controlPanel->setI2cEnabled(true);
    m_controlPanel->cameraSettings()->setI2cBridge(m_i2cBridge.get());

    // ── I2C Read / Write connections ──
    {
        auto* pan = m_controlPanel;
        auto* bridge = m_i2cBridge.get();

        disconnect(pan->i2cReadBtn(), nullptr, nullptr, nullptr);
        disconnect(pan->i2cWriteBtn(), nullptr, nullptr, nullptr);

        // Read
        connect(pan->i2cReadBtn(), &QPushButton::clicked, this, [pan, bridge]() {
            if (!bridge || !bridge->isValid()) {
                pan->i2cResultLabel()->setText(TR("I2C 桥不可用"));
                return;
            }
            bool ok;

            // Parse I2C address
            uint8_t i2cAddr = (uint8_t)pan->i2cAddrEdit()->text().toUInt(&ok, 16);
            if (ok) bridge->setI2cAddress(i2cAddr);

            // Parse register address
            uint16_t regAddr = (uint16_t)pan->i2cRegEdit()->text().toUInt(&ok, 16);
            if (!ok) { pan->i2cResultLabel()->setText(TR("无效的寄存器地址")); return; }

            // Parse length
            int len = pan->i2cLenEdit()->text().toInt(&ok);
            if (!ok || len < 1 || len > 256) { pan->i2cResultLabel()->setText(TR("长度需在 1-256")); return; }

            std::vector<uint8_t> buf(len, 0);
            int ret = bridge->readReg(regAddr, buf.data(), len);
            if (ret <= 0) {
                pan->i2cResultLabel()->setText(QString("读取失败 (ret=%1)").arg(ret));
                return;
            }

            // Read raw bytes, then display in big-endian for human readability
            QStringList rawParts, beParts;
            for (int i = 0; i < ret; i++)
                rawParts << QString("%1").arg(buf[i], 2, 16, QChar('0')).toUpper();
            for (int i = ret - 1; i >= 0; i--)
                beParts << QString("%1").arg(buf[i], 2, 16, QChar('0')).toUpper();
            pan->i2cResultLabel()->setText(
                QString("读 [0x%1] %2 字节\n"
                        "  Raw: %3\n"
                        "  BE : %4")
                    .arg(regAddr, 4, 16, QChar('0')).arg(ret)
                    .arg(rawParts.join(' ')).arg(beParts.join(' ')));
        });

        // Write
        connect(pan->i2cWriteBtn(), &QPushButton::clicked, this, [pan, bridge]() {
            if (!bridge || !bridge->isValid()) {
                pan->i2cResultLabel()->setText(TR("I2C 桥不可用"));
                return;
            }
            bool ok;

            uint8_t i2cAddr = (uint8_t)pan->i2cAddrEdit()->text().toUInt(&ok, 16);
            if (ok) bridge->setI2cAddress(i2cAddr);

            uint16_t regAddr = (uint16_t)pan->i2cRegEdit()->text().toUInt(&ok, 16);
            if (!ok) { pan->i2cResultLabel()->setText(TR("无效的寄存器地址")); return; }

            // Parse hex bytes.
            // "1E 1E" or "1E,1E" → bytes sent as-is: [0x1E, 0x1E]
            // "1234" (no spaces) → treated as a 16-bit value, sent little-endian: [0x34, 0x12]
            QString raw = pan->i2cDataEdit()->text().trimmed();
            bool hasSep = raw.contains(' ') || raw.contains(',');
            QString hex = raw;
            hex.remove(' ').remove(',');
            if (hex.isEmpty()) { pan->i2cResultLabel()->setText(TR("无写入数据")); return; }
            if (hex.length() % 2 != 0) {
                pan->i2cResultLabel()->setText(TR("十六进制数据长度须为偶数"));
                return;
            }

            std::vector<uint8_t> wbuf;
          //  bool ok;
            for (int i = 0; i < hex.length(); i += 2) {
                uint8_t b = (uint8_t)hex.mid(i, 2).toUInt(&ok, 16);
                if (!ok) { pan->i2cResultLabel()->setText(QString("无效的十六进制: %1").arg(hex.mid(i, 2))); return; }
                wbuf.push_back(b);
            }
            // Reverse byte order for little-endian when user typed a continuous value (no separators)
            if (!hasSep && wbuf.size() > 1)
                std::reverse(wbuf.begin(), wbuf.end());

            int ret = bridge->writeReg(regAddr, wbuf.data(), (int)wbuf.size());
            if (ret <= 0) {
                pan->i2cResultLabel()->setText(QString("写入失败 (ret=%1)").arg(ret));
                return;
            }
            pan->i2cResultLabel()->setText(
                QString("写 [0x%1] %2 字节 OK").arg(regAddr, 4, 16, QChar('0')).arg(ret));
        });
    }

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
        m_viewport->clearImage();        // 清除残留画面，防止重启时闪现旧帧
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

    // 重置 worker 诊断计数器，确保新一轮启流的前3帧日志正常输出
    if (m_worker) m_worker->resetDiagCounters();

    LOG_INFO(QString("Streaming started: %1x%2")
        .arg(fmt.width).arg(fmt.height));
}

// ── Frame display (主线程，仅负责 UI) ──
// 帧的协议解析 + QImage 转换已在 ProcessingWorker 线程中完成。

void QtWidgetsApplication1::onFrameProcessed(QImage img, ProcessedFrame parsed) {
    if (!m_streaming) return;
    if (img.isNull()) return;

    // 保存最后帧用于截图
    m_lastFrame = std::move(parsed);

    // 显示
    m_viewport->setImage(img);

    // HUD overlay
    m_displayFrameCount++;
    auto now = QDateTime::currentDateTime();
    m_viewport->setOverlayText(
        QString("%1  Frame: %2")
            .arg(now.toString("hh:mm:ss.zzz"))
            .arg(m_displayFrameCount));

    // Recording
    if (m_recording) {
        QString filename = QString("record_%1_%2.png")
            .arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss"))
            .arg(m_recordCounter++, 5, 10, QChar('0'));
        img.save(filename, "PNG");
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
