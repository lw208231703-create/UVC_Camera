#include "CameraSettingsWidget.h"
#include "hal/UvcControls.h"
#include "hal/FTI2cBridge.h"
#include "infra/UiStrings.h"
#include "infra/LogManager.h"
#include <QMessageBox>
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

        // Exposure time: input in μs, auto-converted to line count
        lay->addWidget(makeInputRow(TR("Exp. Time (μs)"), m_exposureEdit, "10000"));
        m_exposureEdit->setValidator(new QIntValidator(0, 10000000, m_exposureEdit));

        auto applyExposure = [this]() {
            if (m_updating || !m_ctrl) return;
            bool ok;
            uint32_t val = (uint32_t)m_exposureEdit->text().toUInt(&ok);
            if (!ok) return;

            if (m_timingValid) {
                double fclk = m_pixelClockMHz * 1e6;
                double tLineUs = (double)m_hmax / fclk * 1e6;
                int rawLines = (int)std::ceil(val / tLineUs);
                int lines = qBound(1, rawLines, (int)m_vmax);
                double actualUs = lines * tLineUs;
                if (lines != rawLines) {
                    m_exposureEdit->setText(QString::number((int)actualUs));
                }
                QString detail = QString(
                    "HMAX=%1  VMAX=%2\n"
                    "Pixel Clock=%3 MHz\n"
                    "T_line=%4 μs\n\n"
                    "输入: %5 μs → %6 行\n"
                    "实际: %7 μs (%8 行)")
                    .arg(m_hmax).arg(m_vmax)
                    .arg(m_pixelClockMHz, 0, 'f', 3)
                    .arg(tLineUs, 0, 'f', 2)
                    .arg(val).arg(rawLines)
                    .arg(actualUs, 0, 'f', 0).arg(lines);
                QMessageBox::information(this, "曝光换算详情", detail);
                m_ctrl->setExposureAbs((uint32_t)lines);
            } else {
                m_ctrl->setExposureAbs(val);
            }
        };

        // editingFinished fires on Enter and when focus leaves the field
        connect(m_exposureEdit, &QLineEdit::editingFinished, this, applyExposure);

        lay->addWidget(makeComboRow(TR("AE Mode"), m_aeModeCombo));
        m_aeModeCombo->addItem(TR("Manual"), 1);
        m_aeModeCombo->addItem(TR("Auto"), 2);
        connect(m_aeModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
                [this](int idx) {
            if (!m_updating && m_i2cBridge && m_i2cBridge->isValid()) {
                uint8_t data[16] = {};
                if (m_aeModeCombo->itemData(idx).toUInt() == 2) // Auto
                    data[0] = 0x01;
                m_i2cBridge->writeReg(0x40, data, 16);
            }
        });

        main->addWidget(grp);
    }

    // ── Sensor Config (帧率/像素格式) ──
    {
        auto* grp = makeGroup(TR("探测器"));
        auto* lay = qobject_cast<QVBoxLayout*>(grp->layout());

        lay->addWidget(makeInputRow(TR("帧率"), m_fpsEdit, ""));
        m_fpsEdit->setValidator(new QIntValidator(0, 65535, m_fpsEdit));
        connect(m_fpsEdit, &QLineEdit::editingFinished, this, [this]() {
            if (m_updating || !m_i2cBridge || !m_i2cBridge->isValid()) return;
            bool ok;
            int val = m_fpsEdit->text().toInt(&ok);
            if (!ok) return;
            uint8_t data[2] = { (uint8_t)(val & 0xFF), (uint8_t)(val >> 8) };
            m_i2cBridge->writeReg(0x10, data, 2);
        });

        lay->addWidget(makeComboRow(TR("像素格式"), m_pixelFormatCombo));
        m_pixelFormatCombo->addItem("12bit", 1);
        m_pixelFormatCombo->addItem("8bit", 2);
        connect(m_pixelFormatCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
                [this](int idx) {
            if (m_updating || !m_i2cBridge || !m_i2cBridge->isValid()) return;
            uint8_t data[16] = {};
            data[0] = (uint8_t)m_pixelFormatCombo->itemData(idx).toUInt();
            m_i2cBridge->writeReg(0x50, data, 16);
        });

        lay->addWidget(makeInputRow(TR("开窗X"), m_roiXEdit, "0"));
        m_roiXEdit->setValidator(new QIntValidator(0, 2560, m_roiXEdit));
        connect(m_roiXEdit, &QLineEdit::editingFinished, this, [this]() {
            if (m_updating || !m_i2cBridge || !m_i2cBridge->isValid()) return;
            bool ok;
            int val = m_roiXEdit->text().toInt(&ok);
            if (!ok) return;
            // X: 对齐到8 + 112偏置，写入寄存器0x54 (2字节, 小端)
            int alignedX = (val / 8) * 8;
            uint16_t regVal = (uint16_t)(alignedX + 112);
            uint8_t data[2] = { (uint8_t)(regVal & 0xFF), (uint8_t)(regVal >> 8) };
            m_i2cBridge->writeReg(0x54, data, 2);
            m_roiXEdit->setText(QString::number(alignedX));
        });

        lay->addWidget(makeInputRow(TR("开窗Y"), m_roiYEdit, "0"));
        m_roiYEdit->setValidator(new QIntValidator(0, 2048, m_roiYEdit));
        connect(m_roiYEdit, &QLineEdit::editingFinished, this, [this]() {
            if (m_updating || !m_i2cBridge || !m_i2cBridge->isValid()) return;
            bool ok;
            int val = m_roiYEdit->text().toInt(&ok);
            if (!ok) return;
            // Y: 对齐到4 + 4偏置，写入寄存器0x55 (2字节, 小端)
            int alignedY = (val / 4) * 4;
            uint16_t regVal = (uint16_t)(alignedY + 4);
            uint8_t data[2] = { (uint8_t)(regVal & 0xFF), (uint8_t)(regVal >> 8) };
            m_i2cBridge->writeReg(0x55, data, 2);
            m_roiYEdit->setText(QString::number(alignedY));
        });

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
    if (bridge) readTimingRegisters();
}

void CameraSettingsWidget::clearControls() {
    m_ctrl = nullptr;
    m_i2cBridge = nullptr;
    m_content->setEnabled(false);
    m_timingValid = false;
}

void CameraSettingsWidget::refreshAll() {
    if (!m_ctrl || !m_ctrl->isValid()) return;
    m_updating = true;

    uint16_t u16; uint32_t u32; uint8_t u8;

    if (m_i2cBridge && m_i2cBridge->isValid()) {
        uint8_t fpsBuf[2] = {};
        if (m_i2cBridge->readReg(0x10, fpsBuf, 2) == 2) {
            uint16_t fps = (fpsBuf[1] << 8) | fpsBuf[0];
            m_fpsEdit->setText(QString::number(fps));
        }
        uint8_t fmtBuf[16] = {};
        if (m_i2cBridge->readReg(0x50, fmtBuf, 16) == 16) {
            for (int i = 0; i < m_pixelFormatCombo->count(); i++) {
                if (m_pixelFormatCombo->itemData(i).toUInt() == fmtBuf[0]) {
                    m_pixelFormatCombo->setCurrentIndex(i); break;
        }
        uint8_t roiXBuf[2] = {};
        if (m_i2cBridge->readReg(0x54, roiXBuf, 2) == 2) {
            uint16_t regVal = (roiXBuf[1] << 8) | roiXBuf[0];
            int roiX = (int)regVal - 112;
            if (roiX < 0) roiX = 0;
            m_roiXEdit->setText(QString::number(roiX));
        }
        uint8_t roiYBuf[2] = {};
        if (m_i2cBridge->readReg(0x55, roiYBuf, 2) == 2) {
            uint16_t regVal = (roiYBuf[1] << 8) | roiYBuf[0];
            int roiY = (int)regVal - 4;
            if (roiY < 0) roiY = 0;
            m_roiYEdit->setText(QString::number(roiY));
        }
    }
        }
    }

    // Gain
    if (m_ctrl->getGain(u16)) { m_gainSlider->setValue(u16); m_gainLabel->setText(QString::number(u16)); }

    // Exposure (camera returns line count, convert to μs for display)
    if (m_ctrl->getExposureAbs(u32)) {
        if (m_timingValid) {
            double fclk = m_pixelClockMHz * 1e6;
            double tLineUs = (double)m_hmax / fclk * 1e6;
            m_exposureEdit->setText(QString::number((int)(u32 * tLineUs)));
        } else {
            m_exposureEdit->setText(QString::number(u32));
        }
    }
    if (m_i2cBridge && m_i2cBridge->isValid()) {
        uint8_t aeBuf[16] = {};
        if (m_i2cBridge->readReg(0x40, aeBuf, 16) == 16) {
            int mode = aeBuf[0] ? 2 : 1;
            for (int i = 0; i < m_aeModeCombo->count(); i++) {
                if (m_aeModeCombo->itemData(i).toUInt() == (uint)mode) { m_aeModeCombo->setCurrentIndex(i); break; }
            }
        }
    }
    m_updating = false;
}

// ── Read HMAX from 0x56 / VMAX from 0x57 via I2C (2 bytes each, little-endian) ──
void CameraSettingsWidget::readTimingRegisters() {
    if (!m_i2cBridge || !m_i2cBridge->isValid()) {
        LOG_WARNING("I2C bridge not available for reading timing registers");
        return;
    }

    // HMAX: register 0x56, 2 bytes, little-endian
    uint8_t hmaxBuf[2] = {};
    int ret = m_i2cBridge->readReg(0x56, hmaxBuf, 2);
    if (ret < 2) {
        LOG_ERROR(QString("Failed to read HMAX register 0x56 (ret=%1)").arg(ret));
        m_timingValid = false;
        return;
    }
    m_hmax = (uint16_t)(hmaxBuf[0] | (hmaxBuf[1] << 8));

    // VMAX: register 0x57, 2 bytes, little-endian
    uint8_t vmaxBuf[2] = {};
    ret = m_i2cBridge->readReg(0x57, vmaxBuf, 2);
    if (ret < 2) {
        LOG_ERROR(QString("Failed to read VMAX register 0x57 (ret=%1)").arg(ret));
        m_timingValid = false;
        return;
    }
    m_vmax = (uint32_t)(vmaxBuf[0] | (vmaxBuf[1] << 8));

    LOG_INFO(QString("Timing: HMAX=%1, VMAX=%2").arg(m_hmax).arg(m_vmax));
    m_timingValid = true;

    recalcTiming();
}

// ── Recalculate timing parameters ──
void CameraSettingsWidget::recalcTiming() {
}


