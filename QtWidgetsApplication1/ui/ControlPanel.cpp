#include "ControlPanel.h"
#include "CameraSettingsWidget.h"
#include "infra/UiStrings.h"
#include <QVBoxLayout>
#include <QFormLayout>
#include <QScrollArea>
#include <QHBoxLayout>

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

    // ── Capture group ──
    {
        m_snapshotBtn = new QPushButton(TR("Snapshot"));
        m_snapshotBtn->setObjectName("captureBtn");
        m_snapshotBtn->setMinimumHeight(40);
        m_snapshotBtn->setEnabled(false);

        auto* capLayout = new QVBoxLayout;
        capLayout->addWidget(m_snapshotBtn);

        mainLayout->addWidget(makeGroup(TR("Capture"), capLayout));
    }

    // ── 16-bit depth control ──
    {
        auto* grp = makeGroup(TR("16-bit Display"), new QVBoxLayout);
        auto* lay = qobject_cast<QVBoxLayout*>(grp->layout());

        auto* row = new QHBoxLayout;
        m_bitShiftSelector = new BitShiftSelector;
        m_bitShiftSelector->setRange(0, 8);
        m_bitShiftSelector->setValue(8);

        m_bitShiftLabel = new QLabel("bits [15:8]");
        m_bitShiftLabel->setStyleSheet("color:#26C0A6; font-size:12px; min-width:80px;");

        connect(m_bitShiftSelector, &BitShiftSelector::valueChanged, this, [this](int val) {
            m_bitShiftLabel->setText(QString("bits [%1:%2]").arg(val + 7).arg(val));
        });

        row->addWidget(new QLabel(TR("Shift:")));
        row->addWidget(m_bitShiftSelector, 1);
        row->addWidget(m_bitShiftLabel);
        lay->addLayout(row);

        mainLayout->addWidget(grp);
    }

    // ── Camera Settings ──
    {
        m_cameraSettings = new CameraSettingsWidget;
        m_cameraSettings->setMaximumHeight(350);
        mainLayout->addWidget(m_cameraSettings);
    }

    // ── I2C 寄存器调试 ──
    {
        m_i2cGroup = makeGroup(TR("I2C 寄存器调试 (FT602直通)"), new QVBoxLayout);
        auto* grp = m_i2cGroup;
        auto* lay = qobject_cast<QVBoxLayout*>(grp->layout());

        auto makeLabel = [](const QString& s) {
            auto* lbl = new QLabel(s);
            lbl->setStyleSheet("color:#26C0A6; font-size:11px;");
            lbl->setFixedWidth(72);
            return lbl;
        };
        auto styleHex = [](QLineEdit* e, int w = 62) {
            e->setFixedWidth(w);
            e->setAlignment(Qt::AlignRight);
            e->setStyleSheet(
                "QLineEdit { background:#3C3C3C; border:1px solid #3E3E42; border-radius:2px;"
                "  padding:2px 6px; color:#CCCCCC; font-size:12px; font-family:Consolas,monospace; }"
                "QLineEdit:focus { border:1px solid #26C0A6; }");
        };

        // Row 1: I2C地址 + 寄存器地址 + 长度
        auto* r1 = new QHBoxLayout;
        m_i2cAddrEdit = new QLineEdit("0D");
        m_i2cAddrEdit->setPlaceholderText("0x0D");
        styleHex(m_i2cAddrEdit, 52);
        r1->addWidget(makeLabel(TR("I2C地址:")));
        r1->addWidget(m_i2cAddrEdit);

        m_i2cRegEdit = new QLineEdit("40");
        m_i2cRegEdit->setPlaceholderText("0x40");
        styleHex(m_i2cRegEdit, 58);
        r1->addWidget(makeLabel(TR("寄存器:")));
        r1->addWidget(m_i2cRegEdit);

        m_i2cLenEdit = new QLineEdit("1");
        m_i2cLenEdit->setPlaceholderText("bytes");
        m_i2cLenEdit->setFixedWidth(44);
        m_i2cLenEdit->setAlignment(Qt::AlignRight);
        m_i2cLenEdit->setStyleSheet(
            "QLineEdit { background:#3C3C3C; border:1px solid #3E3E42; border-radius:2px;"
            "  padding:2px 6px; color:#CCCCCC; font-size:12px; }"
            "QLineEdit:focus { border:1px solid #26C0A6; }");
        r1->addWidget(makeLabel(TR("长度:")));
        r1->addWidget(m_i2cLenEdit);
        r1->addStretch();
        lay->addLayout(r1);

        // Row 2: 写入数据
        auto* r2 = new QHBoxLayout;
        m_i2cDataEdit = new QLineEdit;
        m_i2cDataEdit->setPlaceholderText(TR("十六进制写入数据（如 A0 B1）"));
        m_i2cDataEdit->setStyleSheet(
            "QLineEdit { background:#3C3C3C; border:1px solid #3E3E42; border-radius:2px;"
            "  padding:2px 6px; color:#CCCCCC; font-size:12px; font-family:Consolas,monospace; }"
            "QLineEdit:focus { border:1px solid #E6A817; }");
        r2->addWidget(new QLabel(TR("写入:")));
        r2->addWidget(m_i2cDataEdit, 1);
        lay->addLayout(r2);

        // Row 3: 按钮
        auto* r3 = new QHBoxLayout;
        m_i2cReadBtn = new QPushButton(TR("读取"));
        m_i2cReadBtn->setObjectName("captureBtn");
        m_i2cReadBtn->setMinimumHeight(28);
        m_i2cWriteBtn = new QPushButton(TR("写入"));
        m_i2cWriteBtn->setObjectName("captureBtn");
        m_i2cWriteBtn->setMinimumHeight(28);
        r3->addWidget(m_i2cReadBtn);
        r3->addWidget(m_i2cWriteBtn);
        r3->addStretch();
        lay->addLayout(r3);

        // Result
        m_i2cResultLabel = new QLabel(TR("--"));
        m_i2cResultLabel->setStyleSheet(
            "color:#CCCCCC; background:#1A1A1A; border:1px solid #3E3E42;"
            "  border-radius:2px; padding:4px 8px; font-family:'Consolas',monospace; font-size:11px;");
        m_i2cResultLabel->setWordWrap(true);
        m_i2cResultLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
        m_i2cResultLabel->setMinimumHeight(32);
        lay->addWidget(m_i2cResultLabel);

        grp->setEnabled(false);
        mainLayout->addWidget(grp);
    }
    // ── Log area (hidden from user, still collects logs internally) ──
    {
        m_logView = new QTextEdit;
        m_logView->setReadOnly(true);
        m_logView->setMaximumHeight(0);
        m_logView->setVisible(false);
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

        auto* grp = makeGroup(TR("System Log"), new QVBoxLayout);
        grp->setVisible(false);
        if (grp && grp->layout()) {
            grp->layout()->addWidget(m_logView);
        }
        mainLayout->addWidget(grp);
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
    m_openBtn->setEnabled(!streaming);

    m_snapshotBtn->setEnabled(streaming);

    m_applyBtn->setText(streaming ? TR("Stop Streaming") : TR("Apply & Start Streaming"));
    m_applyBtn->setStyleSheet(streaming
        ? "QPushButton { background-color: #8B0000; } QPushButton:hover { background-color: #A00000; }"
        : "");
}

void ControlPanel::setI2cEnabled(bool enabled) {
    if (m_i2cGroup) m_i2cGroup->setEnabled(enabled);
    if (!enabled) m_i2cResultLabel->setText(TR("--"));
}
