#include "ControlPanel.h"
#include "CameraSettingsWidget.h"
#include "infra/UiStrings.h"
#include <QVBoxLayout>
#include <QFormLayout>
#include <QScrollArea>

ControlPanel::ControlPanel(QWidget* parent)
    : QWidget(parent)
{
    auto* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);

    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    outerLayout->addWidget(scroll);

    auto* inner = new QWidget;
    auto* mainLayout = new QVBoxLayout(inner);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(8);
    scroll->setWidget(inner);

    // ── Device group ──
    {
        m_deviceCombo = new QComboBox;
        m_deviceCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        m_deviceCombo->setMinimumHeight(28);

        auto* refreshBtn = new QPushButton(TR("Refresh"));
        refreshBtn->setFixedWidth(70);
        connect(refreshBtn, &QPushButton::clicked, this, &ControlPanel::refreshDevices);

        auto* devTop = new QHBoxLayout;
        devTop->addWidget(m_deviceCombo);
        devTop->addWidget(refreshBtn);

        m_openBtn = new QPushButton(TR("Open Camera"));
        m_openBtn->setObjectName("openBtn");

        m_deviceInfoLabel = new QLabel(TR("No device selected"));
        m_deviceInfoLabel->setWordWrap(true);
        m_deviceInfoLabel->setStyleSheet("color:#858585; font-size:11px;");

        auto* devLayout = new QVBoxLayout;
        devLayout->addLayout(devTop);
        devLayout->addWidget(m_openBtn);
        devLayout->addWidget(m_deviceInfoLabel);

        mainLayout->addWidget(makeGroup(TR("Device Connection"), devLayout));
    }

    // ── Format group ──
    {
        m_formatCombo = new QComboBox;
        m_formatCombo->setEnabled(false);
        m_formatCombo->addItems({"Y16", "Y800", "YUYV", "MJPEG", "Custom 12-bit"});

        m_resolutionCombo = new QComboBox;
        m_resolutionCombo->setEnabled(false);
        m_resolutionCombo->addItem(TR("-- Select --"));

        m_applyBtn = new QPushButton(TR("Apply & Start Streaming"));
        m_applyBtn->setObjectName("applyBtn");
        m_applyBtn->setEnabled(false);

        auto* fmtLayout = new QFormLayout;
        fmtLayout->addRow(TR("Format:"), m_formatCombo);
        fmtLayout->addRow(TR("Resolution:"), m_resolutionCombo);

        auto* grpLayout = new QVBoxLayout;
        grpLayout->addLayout(fmtLayout);
        grpLayout->addWidget(m_applyBtn);

        mainLayout->addWidget(makeGroup(TR("Format & Stream"), grpLayout));
    }

    // ── Processing group ──
    {
        m_gaussianCheck = new QCheckBox(TR("Gaussian Blur"));
        m_gaussianCheck->setEnabled(false);
        m_histogramCheck = new QCheckBox(TR("Histogram Equalization"));
        m_histogramCheck->setEnabled(false);

        auto* procLayout = new QVBoxLayout;
        procLayout->addWidget(m_gaussianCheck);
        procLayout->addWidget(m_histogramCheck);

        mainLayout->addWidget(makeGroup(TR("Image Processing (OpenCV)"), procLayout));
    }

    // ── Capture group ──
    {
        m_snapshotBtn = new QPushButton(TR("Snapshot"));
        m_snapshotBtn->setObjectName("captureBtn");
        m_snapshotBtn->setMinimumHeight(40);
        m_snapshotBtn->setEnabled(false);

        m_recordBtn = new QPushButton(TR("Record Sequence"));
        m_recordBtn->setObjectName("captureBtn");
        m_recordBtn->setMinimumHeight(40);
        m_recordBtn->setEnabled(false);
        m_recordBtn->setCheckable(true);

        auto* capLayout = new QVBoxLayout;
        capLayout->addWidget(m_snapshotBtn);
        capLayout->addWidget(m_recordBtn);

        mainLayout->addWidget(makeGroup(TR("Capture"), capLayout));
    }

    // ── Camera Settings ──
    {
        m_cameraSettings = new CameraSettingsWidget;
        m_cameraSettings->setMaximumHeight(350);
        mainLayout->addWidget(m_cameraSettings);
    }

    // ── Log area ──
    {
        m_logView = new QTextEdit;
        m_logView->setReadOnly(true);
        m_logView->setMaximumHeight(150);
        m_logView->setStyleSheet(
            "QTextEdit {"
            "  background-color: #1A1A1A;"
            "  color: #CCCCCC;"
            "  border: 1px solid #3E3E42;"
            "  border-radius: 2px;"
            "  font-family: 'Consolas', 'Courier New', monospace;"
            "  font-size: 11px;"
            "}"
        );

        mainLayout->addWidget(makeGroup(TR("System Log"), new QVBoxLayout));
        auto* grp = qobject_cast<QGroupBox*>(mainLayout->itemAt(mainLayout->count() - 1)->widget());
        if (grp && grp->layout()) {
            grp->layout()->addWidget(m_logView);
        }
    }

    mainLayout->addStretch();
}

QGroupBox* ControlPanel::makeGroup(const QString& title, QLayout* layout) {
    auto* grp = new QGroupBox(title);
    grp->setStyleSheet(
        "QGroupBox {"
        "  font-weight: bold;"
        "  color: #CCCCCC;"
        "  border: 1px solid #3E3E42;"
        "  border-radius: 4px;"
        "  margin-top: 12px;"
        "  padding-top: 14px;"
        "}"
        "QGroupBox::title {"
        "  subcontrol-origin: margin;"
        "  left: 10px;"
        "  padding: 0 4px;"
        "  color: #26C0A6;"
        "}"
    );
    grp->setLayout(layout);
    return grp;
}

void ControlPanel::setDeviceOpen(bool open) {
    m_deviceCombo->setEnabled(!open);
    m_formatCombo->setEnabled(open);
    m_resolutionCombo->setEnabled(open);
    m_applyBtn->setEnabled(open);

    m_openBtn->setText(open ? TR("Close Camera") : TR("Open Camera"));
    m_openBtn->setStyleSheet(open
        ? "QPushButton { background-color: #8B0000; color: #CCCCCC; } QPushButton:hover { background-color: #A00000; }"
        : "");
}

void ControlPanel::setStreaming(bool streaming) {
    m_formatCombo->setEnabled(!streaming);
    m_resolutionCombo->setEnabled(!streaming);

    m_gaussianCheck->setEnabled(streaming);
    m_histogramCheck->setEnabled(streaming);
    m_snapshotBtn->setEnabled(streaming);
    m_recordBtn->setEnabled(streaming);

    m_applyBtn->setText(streaming ? TR("Stop Streaming") : TR("Apply & Start Streaming"));
    m_applyBtn->setStyleSheet(streaming
        ? "QPushButton { background-color: #8B0000; } QPushButton:hover { background-color: #A00000; }"
        : "");
}
