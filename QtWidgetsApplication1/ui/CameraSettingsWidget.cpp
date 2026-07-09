#include "CameraSettingsWidget.h"
#include "hal/UvcControls.h"
#include "hal/FTI2cBridge.h"
#include "infra/UiStrings.h"
#include "infra/LogManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QIntValidator>
#include <QTimer>
#include <QFrame>
#include <cmath>

CameraSettingsWidget::CameraSettingsWidget(QWidget* parent)
    : QScrollArea(parent)
{
    setWidgetResizable(true);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setupUi();
}

QGroupBox* CameraSettingsWidget::makeGroup(const QString& title) {
    auto* grp = new QGroupBox(title);
    grp->setStyleSheet(
        "QGroupBox { font-weight: bold; color: #CCCCCC; border: 1px solid #3E3E42;"
        "  border-radius: 4px; margin-top: 12px; padding-top: 14px; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 4px; color: #26C0A6; }");
    auto* lay = new QVBoxLayout(grp);
    lay->setSpacing(4);
    return grp;
}

QWidget* CameraSettingsWidget::makeSliderRow(const QString& name, QSlider*& slider, QLabel*& label,
                                              int min, int max, int val) {
    auto* w = new QWidget;
    auto* lay = new QHBoxLayout(w);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(6);

    auto* nameLbl = new QLabel(name);
    nameLbl->setFixedWidth(90);
    nameLbl->setStyleSheet("color:#CCCCCC; font-size:12px;");

    slider = new QSlider(Qt::Horizontal);
    slider->setRange(min, max);
    slider->setValue(val);
    slider->setStyleSheet(
        "QSlider::groove:horizontal { background:#3C3C3C; height:6px; border-radius:3px; }"
        "QSlider::handle:horizontal { background:#26C0A6; width:14px; margin:-5px 0; border-radius:7px; }"
        "QSlider::sub-page:horizontal { background:#26C0A6; border-radius:3px; }");

    label = new QLabel(QString::number(val));
    label->setFixedWidth(50);
    label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    label->setStyleSheet("color:#CCCCCC; font-size:12px;");

    lay->addWidget(nameLbl);
    lay->addWidget(slider, 1);
    lay->addWidget(label);
    return w;
}

QWidget* CameraSettingsWidget::makeInputRow(const QString& name, QLineEdit*& edit, const QString& initialText) {
    auto* w = new QWidget;
    auto* lay = new QHBoxLayout(w);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(6);

    auto* nameLbl = new QLabel(name);
    nameLbl->setFixedWidth(90);
    nameLbl->setStyleSheet("color:#CCCCCC; font-size:12px;");

    edit = new QLineEdit(initialText);
    edit->setAlignment(Qt::AlignRight);
    edit->setStyleSheet(
        "QLineEdit { background:#3C3C3C; border:1px solid #3E3E42; border-radius:2px;"
        "  padding:2px 6px; color:#CCCCCC; font-size:12px; }"
        "QLineEdit:focus { border:1px solid #26C0A6; }");

    lay->addWidget(nameLbl);
    lay->addWidget(edit, 1);
    return w;
}

QWidget* CameraSettingsWidget::makeCheckRow(const QString& name, QCheckBox*& check, bool checked) {
    check = new QCheckBox(name);
    check->setChecked(checked);
    check->setStyleSheet("color:#CCCCCC; font-size:12px;");
    return check;
}

QWidget* CameraSettingsWidget::makeComboRow(const QString& name, QComboBox*& combo) {
    auto* w = new QWidget;
    auto* lay = new QHBoxLayout(w);
    lay->setContentsMargins(0, 0, 0, 0);
    auto* lbl = new QLabel(name);
    lbl->setFixedWidth(90);
    lbl->setStyleSheet("color:#CCCCCC; font-size:12px;");
    combo = new QComboBox;
    lay->addWidget(lbl);
    lay->addWidget(combo, 1);
    return w;
}

void CameraSettingsWidget::connectSlider(QSlider* slider, QLabel* label, double scale,
                                          std::function<bool(int)> setter) {
    connect(slider, &QSlider::valueChanged, this, [=](int val) {
        label->setText(QString::number(val));
        if (m_updating || !m_ctrl) return;
        setter(val);
    });
    Q_UNUSED(scale);
}

void CameraSettingsWidget::setupUi() {
    m_content = new QWidget;
    auto* main = new QVBoxLayout(m_content);
    main->setSpacing(6);
    main->setContentsMargins(0, 0, 0, 0);

    // ── Gain ──
    {
        auto* grp = makeGroup(TR("Gain"));
        auto* lay = qobject_cast<QVBoxLayout*>(grp->layout());

        lay->addWidget(makeSliderRow(TR("Gain"), m_gainSlider, m_gainLabel, 0, 255, 0));
        connectSlider(m_gainSlider, m_gainLabel, 1.0,
                      [this](int v) { return m_ctrl->setGain((uint16_t)v); });

        main->addWidget(grp);
    }

    // ── Exposure ──
    {
        auto* grp = makeGroup(TR("Exposure"));
        auto* lay = qobject_cast<QVBoxLayout*>(grp->layout());

        // Exposure time: line edit, apply on Enter or focus lost
        lay->addWidget(makeInputRow(TR("Exp. Time"), m_exposureEdit, "156"));
        m_exposureEdit->setValidator(new QIntValidator(0, 10000000, m_exposureEdit));

        auto applyExposure = [this]() {
            if (m_updating || !m_ctrl) return;
            bool ok;
            uint32_t val = (uint32_t)m_exposureEdit->text().toUInt(&ok);
            if (ok) m_ctrl->setExposureAbs(val);
        };

        // editingFinished fires on Enter and when focus leaves the field
        connect(m_exposureEdit, &QLineEdit::editingFinished, this, applyExposure);

        lay->addWidget(makeComboRow(TR("AE Mode"), m_aeModeCombo));
        m_aeModeCombo->addItem(TR("Manual"), 1);
        m_aeModeCombo->addItem(TR("Auto"), 2);
        m_aeModeCombo->addItem(TR("Shutter Priority"), 4);
        m_aeModeCombo->addItem(TR("Aperture Priority"), 8);
        connect(m_aeModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
                [this](int idx) {
            if (!m_updating && m_ctrl)
                m_ctrl->setAeMode((uint8_t)m_aeModeCombo->itemData(idx).toUInt());
        });

        lay->addWidget(makeCheckRow(TR("AE Priority"), m_aePriorityCheck, false));
        connect(m_aePriorityCheck, &QCheckBox::toggled, this, [this](bool on) {
            if (!m_updating && m_ctrl) m_ctrl->setAePriority(on ? 1 : 0);
        });

        main->addWidget(grp);
    }

    // ── Exposure Timing Conversion (μs ↔ line) ──
    {
        auto* grp = makeGroup(TR("曝光换算 (μs ↔ 行)"));
        auto* lay = qobject_cast<QVBoxLayout*>(grp->layout());

        // Row helper: label + value
        auto makeInfoRow = [](const QString& name, QLabel*& label) -> QWidget* {
            auto* w = new QWidget;
            auto* h = new QHBoxLayout(w);
            h->setContentsMargins(0, 0, 0, 0);
            h->setSpacing(6);
            auto* nameLbl = new QLabel(name);
            nameLbl->setFixedWidth(120);
            nameLbl->setStyleSheet("color:#999999; font-size:12px;");
            label = new QLabel("--");
            label->setStyleSheet("color:#26C0A6; font-size:12px; font-weight:bold;");
            h->addWidget(nameLbl);
            h->addWidget(label, 1);
            return w;
        };

        // Pixel clock input
        {
            auto* w = new QWidget;
            auto* h = new QHBoxLayout(w);
            h->setContentsMargins(0, 0, 0, 0);
            h->setSpacing(6);
            auto* lbl = new QLabel(TR("像素时钟 (MHz)"));
            lbl->setFixedWidth(120);
            lbl->setStyleSheet("color:#CCCCCC; font-size:12px;");
            m_pixelClockSpin = new QDoubleSpinBox();
            m_pixelClockSpin->setRange(1.0, 500.0);
            m_pixelClockSpin->setDecimals(3);
            m_pixelClockSpin->setValue(74.250);
            m_pixelClockSpin->setSingleStep(1.0);
            h->addWidget(lbl);
            h->addWidget(m_pixelClockSpin, 1);
            lay->addWidget(w);

            connect(m_pixelClockSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                    this, [this](double val) {
                m_pixelClockMHz = val;
                recalcTiming();
                onUsExposureChanged();
            });
        }

        // Read timing button
        m_readTimingBtn = new QPushButton(TR("读取 HMAX/VMAX (寄存器 0x51)"));
        m_readTimingBtn->setStyleSheet(
            "QPushButton { background:#2D2D2D; border:1px solid #3E3E42; border-radius:4px;"
            "  padding:4px 10px; color:#26C0A6; font-size:12px; font-weight:bold; }"
            "QPushButton:hover { background:#3C3C3C; border-color:#26C0A6; }");
        connect(m_readTimingBtn, &QPushButton::clicked, this, &CameraSettingsWidget::readTimingRegisters);
        lay->addWidget(m_readTimingBtn);

        // Timing info display
        lay->addWidget(makeInfoRow(TR("HMAX:"), m_hmaxLabel));
        lay->addWidget(makeInfoRow(TR("VMAX:"), m_vmaxLabel));
        lay->addWidget(makeInfoRow(TR("T_line (μs):"), m_tlineLabel));
        lay->addWidget(makeInfoRow(TR("T_frame (ms):"), m_tframeLabel));
        lay->addWidget(makeInfoRow(TR("最大帧率 (fps):"), m_fpsLabel));
        lay->addWidget(makeInfoRow(TR("曝光范围 (μs):"), m_expRangeLabel));

        // Separator
        auto* sep = new QFrame;
        sep->setFrameShape(QFrame::HLine);
        sep->setStyleSheet("color:#3E3E42;");
        lay->addWidget(sep);

        // μs → line conversion
        {
            auto* w = new QWidget;
            auto* h = new QHBoxLayout(w);
            h->setContentsMargins(0, 0, 0, 0);
            h->setSpacing(6);
            auto* lbl = new QLabel(TR("输入曝光 (μs):"));
            lbl->setFixedWidth(120);
            lbl->setStyleSheet("color:#CCCCCC; font-size:12px;");
            m_usExpSpin = new QDoubleSpinBox();
            m_usExpSpin->setRange(0.0, 999999.0);
            m_usExpSpin->setDecimals(2);
            m_usExpSpin->setValue(10000.0);
            m_usExpSpin->setSingleStep(100.0);
            m_usExpSpin->setSuffix(" μs");
            h->addWidget(lbl);
            h->addWidget(m_usExpSpin, 1);
            lay->addWidget(w);

            connect(m_usExpSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                    this, [this](double) { onUsExposureChanged(); });
        }

        lay->addWidget(makeInfoRow(TR("→ 行曝光数:"), m_lineResultLabel));
        lay->addWidget(makeInfoRow(TR("→ 实际曝光 (μs):"), m_actualUsLabel));

        // Apply button: write calculated line count to Exp. Time
        m_applyLineBtn = new QPushButton(TR("应用行曝光值 →"));
        m_applyLineBtn->setStyleSheet(
            "QPushButton { background:#26C0A6; border:none; border-radius:4px;"
            "  padding:4px 12px; color:#1E1E1E; font-size:12px; font-weight:bold; }"
            "QPushButton:hover { background:#2DD6BB; }"
            "QPushButton:disabled { background:#3C3C3C; color:#555555; }");
        m_applyLineBtn->setEnabled(false);
        connect(m_applyLineBtn, &QPushButton::clicked, this, [this]() {
            if (!m_timingValid || !m_ctrl) return;
            double fclk = m_pixelClockMHz * 1e6;
            double tLineUs = (double)m_hmax / fclk * 1e6;
            double inputUs = m_usExpSpin->value();
            int lines = (int)std::ceil(inputUs / tLineUs);
            if (lines < 1) lines = 1;
            if (lines > (int)m_vmax) lines = (int)m_vmax;
            m_exposureEdit->setText(QString::number(lines));
            m_ctrl->setExposureAbs((uint32_t)lines);
            LOG_INFO(QString("Applied exposure: %1 lines (%2 μs)")
                .arg(lines).arg(lines * tLineUs, 0, 'f', 2));
        });
        lay->addWidget(m_applyLineBtn);

        main->addWidget(grp);
    }

    main->addStretch();
    setWidget(m_content);
}

void CameraSettingsWidget::setControls(UvcControls* ctrl) {
    m_ctrl = ctrl;
    m_content->setEnabled(ctrl != nullptr);
    if (ctrl) refreshAll();
}

void CameraSettingsWidget::setI2cBridge(FTI2cBridge* bridge) {
    m_i2cBridge = bridge;
}

void CameraSettingsWidget::clearControls() {
    m_ctrl = nullptr;
    m_content->setEnabled(false);
    m_timingValid = false;
}

void CameraSettingsWidget::refreshAll() {
    if (!m_ctrl || !m_ctrl->isValid()) return;
    m_updating = true;

    uint16_t u16; uint32_t u32; uint8_t u8;

    // Gain
    if (m_ctrl->getGain(u16)) { m_gainSlider->setValue(u16); m_gainLabel->setText(QString::number(u16)); }

    // Exposure
    if (m_ctrl->getExposureAbs(u32)) {
        m_exposureEdit->setText(QString::number(u32));
    }
    if (m_ctrl->getAeMode(u8)) {
        for (int i = 0; i < m_aeModeCombo->count(); i++) {
            if (m_aeModeCombo->itemData(i).toUInt() == u8) { m_aeModeCombo->setCurrentIndex(i); break; }
        }
    }
    if (m_ctrl->getAePriority(u8)) m_aePriorityCheck->setChecked(u8 != 0);

    m_updating = false;
}

// ── Read HMAX/VMAX from register 0x51 via I2C ──
void CameraSettingsWidget::readTimingRegisters() {
    if (!m_i2cBridge || !m_i2cBridge->isValid()) {
        LOG_WARNING("I2C bridge not available for reading timing registers");
        return;
    }

    // Register 0x51: 5 bytes, HMAX=[15:0], VMAX=[39:16]
    uint8_t buf[5] = {};
    int ret = m_i2cBridge->readReg(0x51, buf, 5);
    if (ret < 5) {
        LOG_ERROR(QString("Failed to read register 0x51 (ret=%1)").arg(ret));
        m_timingValid = false;
        m_hmaxLabel->setText("读取失败");
        m_vmaxLabel->setText("读取失败");
        return;
    }

    // HMAX = buf[1]<<8 | buf[0]  (little-endian, bytes [0:1])
    m_hmax = (uint16_t)(buf[0] | (buf[1] << 8));
    // VMAX = buf[4]<<16 | buf[3]<<8 | buf[2]  (little-endian, bytes [2:4])
    m_vmax = (uint32_t)(buf[2] | (buf[3] << 8) | (buf[4] << 16));

    LOG_INFO(QString("Timing: HMAX=%1, VMAX=%2").arg(m_hmax).arg(m_vmax));
    m_timingValid = true;

    recalcTiming();
    onUsExposureChanged();
}

// ── Recalculate timing parameters ──
void CameraSettingsWidget::recalcTiming() {
    if (!m_timingValid || m_pixelClockMHz <= 0) {
        m_hmaxLabel->setText(m_timingValid ? QString::number(m_hmax) : "--");
        m_vmaxLabel->setText(m_timingValid ? QString::number(m_vmax) : "--");
        m_tlineLabel->setText("--");
        m_tframeLabel->setText("--");
        m_fpsLabel->setText("--");
        m_expRangeLabel->setText("--");
        m_applyLineBtn->setEnabled(false);
        return;
    }

    double fclk = m_pixelClockMHz * 1e6; // Hz
    double tLineUs = (double)m_hmax / fclk * 1e6;   // μs
    double tFrameMs = tLineUs * m_vmax / 1000.0;     // ms
    double fps = 1000.0 / tFrameMs;                   // Hz
    double expMinUs = tLineUs;                         // 1 line
    double expMaxUs = tLineUs * m_vmax;                // v_max lines (no frame drop)

    m_hmaxLabel->setText(QString::number(m_hmax));
    m_vmaxLabel->setText(QString::number(m_vmax));
    m_tlineLabel->setText(QString::number(tLineUs, 'f', 4));
    m_tframeLabel->setText(QString::number(tFrameMs, 'f', 3));
    m_fpsLabel->setText(QString::number(fps, 'f', 2));
    m_expRangeLabel->setText(QString("%1 ~ %2 μs")
        .arg(expMinUs, 0, 'f', 2)
        .arg(expMaxUs, 0, 'f', 2));

    // Update μs spinbox range
    m_usExpSpin->setRange(expMinUs, expMaxUs);
    m_usExpSpin->setSingleStep(tLineUs);

    // Enable apply button
    m_applyLineBtn->setEnabled(true);
}

// ── Convert μs exposure to line count ──
void CameraSettingsWidget::onUsExposureChanged() {
    if (!m_timingValid || m_pixelClockMHz <= 0) {
        m_lineResultLabel->setText("--");
        m_actualUsLabel->setText("--");
        return;
    }

    double fclk = m_pixelClockMHz * 1e6;
    double tLineUs = (double)m_hmax / fclk * 1e6;
    double inputUs = m_usExpSpin->value();

    // Convert to floating-point line count
    double linesFloat = inputUs / tLineUs;

    // Ceil to ensure actual exposure >= user input
    int lines = (int)std::ceil(linesFloat);

    // Clamp to valid range [1, vmax]
    if (lines < 1) lines = 1;
    if (lines > (int)m_vmax) lines = (int)m_vmax;

    // Reverse-calculate actual μs
    double actualUs = lines * tLineUs;

    m_lineResultLabel->setText(QString("%1 行 (浮点: %2)")
        .arg(lines)
        .arg(linesFloat, 0, 'f', 2));
    m_actualUsLabel->setText(QString("%1 μs (≈%2 ms)")
        .arg(actualUs, 0, 'f', 2)
        .arg(actualUs / 1000.0, 0, 'f', 3));
}
