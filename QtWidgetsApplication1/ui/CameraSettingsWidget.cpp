#include "CameraSettingsWidget.h"
#include "hal/UvcControls.h"
#include "infra/UiStrings.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QIntValidator>

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
    Q_UNUSED(scale); // kept for future use (e.g. float-value sliders)
}

void CameraSettingsWidget::setupUi() {
    m_content = new QWidget;
    auto* main = new QVBoxLayout(m_content);
    main->setSpacing(6);
    main->setContentsMargins(0, 0, 0, 0);

    // ── Image Adjustments ──
    {
        auto* grp = makeGroup(TR("Image Adjustments"));
        auto* lay = qobject_cast<QVBoxLayout*>(grp->layout());

        lay->addWidget(makeSliderRow(TR("Brightness"), m_brightnessSlider, m_brightnessLabel, 0, 255, 128));
        connectSlider(m_brightnessSlider, m_brightnessLabel, 1.0,
                      [this](int v) { return m_ctrl->setBrightness((int16_t)v); });

        lay->addWidget(makeSliderRow(TR("Contrast"), m_contrastSlider, m_contrastLabel, 0, 255, 128));
        connectSlider(m_contrastSlider, m_contrastLabel, 1.0,
                      [this](int v) { return m_ctrl->setContrast((uint16_t)v); });
        lay->addWidget(makeCheckRow(TR("Contrast Auto"), m_contrastAuto, false));
        connect(m_contrastAuto, &QCheckBox::toggled, this, [this](bool on) {
            if (!m_updating && m_ctrl) m_ctrl->setContrastAuto(on ? 1 : 0);
        });

        lay->addWidget(makeSliderRow(TR("Gain"), m_gainSlider, m_gainLabel, 0, 255, 0));
        connectSlider(m_gainSlider, m_gainLabel, 1.0,
                      [this](int v) { return m_ctrl->setGain((uint16_t)v); });

        lay->addWidget(makeSliderRow(TR("Saturation"), m_saturationSlider, m_saturationLabel, 0, 255, 128));
        connectSlider(m_saturationSlider, m_saturationLabel, 1.0,
                      [this](int v) { return m_ctrl->setSaturation((uint16_t)v); });

        lay->addWidget(makeSliderRow(TR("Sharpness"), m_sharpnessSlider, m_sharpnessLabel, 0, 255, 128));
        connectSlider(m_sharpnessSlider, m_sharpnessLabel, 1.0,
                      [this](int v) { return m_ctrl->setSharpness((uint16_t)v); });

        lay->addWidget(makeSliderRow(TR("Gamma"), m_gammaSlider, m_gammaLabel, 100, 500, 200));
        connectSlider(m_gammaSlider, m_gammaLabel, 1.0,
                      [this](int v) { return m_ctrl->setGamma((uint16_t)v); });

        lay->addWidget(makeSliderRow(TR("Hue"), m_hueSlider, m_hueLabel, -180, 180, 0));
        connectSlider(m_hueSlider, m_hueLabel, 1.0,
                      [this](int v) { return m_ctrl->setHue((int16_t)v); });
        lay->addWidget(makeCheckRow(TR("Hue Auto"), m_hueAuto, false));
        connect(m_hueAuto, &QCheckBox::toggled, this, [this](bool on) {
            if (!m_updating && m_ctrl) m_ctrl->setHueAuto(on ? 1 : 0);
        });

        main->addWidget(grp);
    }

    // ── White Balance ──
    {
        auto* grp = makeGroup(TR("White Balance"));
        auto* lay = qobject_cast<QVBoxLayout*>(grp->layout());

        lay->addWidget(makeSliderRow(TR("Temperature"), m_wbTempSlider, m_wbTempLabel, 2000, 6500, 4000));
        connectSlider(m_wbTempSlider, m_wbTempLabel, 1.0,
                      [this](int v) { return m_ctrl->setWhiteBalanceTemp((uint16_t)v); });
        lay->addWidget(makeCheckRow(TR("Auto White Balance"), m_wbAuto, true));
        connect(m_wbAuto, &QCheckBox::toggled, this, [this](bool on) {
            if (!m_updating && m_ctrl) m_ctrl->setWhiteBalanceTempAuto(on ? 1 : 0);
        });

        main->addWidget(grp);
    }

    // ── Exposure ──
    {
        auto* grp = makeGroup(TR("Exposure"));
        auto* lay = qobject_cast<QVBoxLayout*>(grp->layout());

        lay->addWidget(makeSliderRow(TR("Exp. Time"), m_exposureSlider, m_exposureLabel, 0, 10000, 156));
        connectSlider(m_exposureSlider, m_exposureLabel, 1.0,
                      [this](int v) { return m_ctrl->setExposureAbs((uint32_t)v); });

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

    // ── Focus ──
    {
        auto* grp = makeGroup(TR("Focus"));
        auto* lay = qobject_cast<QVBoxLayout*>(grp->layout());

        lay->addWidget(makeSliderRow(TR("Focus"), m_focusSlider, m_focusLabel, 0, 255, 0));
        connectSlider(m_focusSlider, m_focusLabel, 1.0,
                      [this](int v) { return m_ctrl->setFocusAbs((uint16_t)v); });
        lay->addWidget(makeCheckRow(TR("Auto Focus"), m_focusAuto, true));
        connect(m_focusAuto, &QCheckBox::toggled, this, [this](bool on) {
            if (!m_updating && m_ctrl) m_ctrl->setFocusAuto(on ? 1 : 0);
        });

        main->addWidget(grp);
    }

    // ── Lens (Zoom / Iris / Pan-Tilt) ──
    {
        auto* grp = makeGroup(TR("Lens"));
        auto* lay = qobject_cast<QVBoxLayout*>(grp->layout());

        lay->addWidget(makeSliderRow(TR("Zoom"), m_zoomSlider, m_zoomLabel, 0, 100, 0));
        connectSlider(m_zoomSlider, m_zoomLabel, 1.0,
                      [this](int v) { return m_ctrl->setZoomAbs((uint16_t)v); });

        lay->addWidget(makeSliderRow(TR("Iris"), m_irisSlider, m_irisLabel, 0, 255, 128));
        connectSlider(m_irisSlider, m_irisLabel, 1.0,
                      [this](int v) { return m_ctrl->setIrisAbs((uint16_t)v); });

        lay->addWidget(makeSliderRow(TR("Pan"), m_panSlider, m_panLabel, -3600, 3600, 0));
        connectSlider(m_panSlider, m_panLabel, 1.0,
                      [this](int v) { return m_ctrl->setPanTiltAbs(v, m_tiltSlider->value()); });

        lay->addWidget(makeSliderRow(TR("Tilt"), m_tiltSlider, m_tiltLabel, -3600, 3600, 0));
        connectSlider(m_tiltSlider, m_tiltLabel, 1.0,
                      [this](int v) { return m_ctrl->setPanTiltAbs(m_panSlider->value(), v); });

        main->addWidget(grp);
    }

    // ── Other ──
    {
        auto* grp = makeGroup(TR("Other"));
        auto* lay = qobject_cast<QVBoxLayout*>(grp->layout());

        lay->addWidget(makeSliderRow(TR("Backlight"), m_backlightSlider, m_backlightLabel, 0, 255, 0));
        connectSlider(m_backlightSlider, m_backlightLabel, 1.0,
                      [this](int v) { return m_ctrl->setBacklightComp((uint16_t)v); });

        lay->addWidget(makeComboRow(TR("Power Line"), m_powerLineCombo));
        m_powerLineCombo->addItem(TR("Disabled"), 0);
        m_powerLineCombo->addItem(TR("50 Hz"), 1);
        m_powerLineCombo->addItem(TR("60 Hz"), 2);
        connect(m_powerLineCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
                [this](int idx) {
            if (!m_updating && m_ctrl)
                m_ctrl->setPowerLineFreq((uint8_t)m_powerLineCombo->itemData(idx).toUInt());
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

void CameraSettingsWidget::clearControls() {
    m_ctrl = nullptr;
    m_content->setEnabled(false);
}

void CameraSettingsWidget::refreshAll() {
    if (!m_ctrl || !m_ctrl->isValid()) return;
    m_updating = true;

    int16_t i16; uint16_t u16; uint32_t u32; uint8_t u8;

    // Brightness
    if (m_ctrl->getBrightness(i16)) {
        int16_t mn, mx;
        if (m_ctrl->getBrightnessRange(mn, mx)) {
            m_brightnessSlider->setRange(mn, mx);
        }
        m_brightnessSlider->setValue(i16);
        m_brightnessLabel->setText(QString::number(i16));
    }

    // Contrast
    if (m_ctrl->getContrast(u16)) { m_contrastSlider->setValue(u16); m_contrastLabel->setText(QString::number(u16)); }
    if (m_ctrl->getContrastAuto(u8)) m_contrastAuto->setChecked(u8 != 0);

    // Gain
    if (m_ctrl->getGain(u16)) { m_gainSlider->setValue(u16); m_gainLabel->setText(QString::number(u16)); }

    // Saturation
    if (m_ctrl->getSaturation(u16)) { m_saturationSlider->setValue(u16); m_saturationLabel->setText(QString::number(u16)); }

    // Sharpness
    if (m_ctrl->getSharpness(u16)) { m_sharpnessSlider->setValue(u16); m_sharpnessLabel->setText(QString::number(u16)); }

    // Gamma
    if (m_ctrl->getGamma(u16)) { m_gammaSlider->setValue(u16); m_gammaLabel->setText(QString::number(u16)); }

    // Hue
    if (m_ctrl->getHue(i16)) { m_hueSlider->setValue(i16); m_hueLabel->setText(QString::number(i16)); }
    if (m_ctrl->getHueAuto(u8)) m_hueAuto->setChecked(u8 != 0);

    // White Balance
    if (m_ctrl->getWhiteBalanceTemp(u16)) { m_wbTempSlider->setValue(u16); m_wbTempLabel->setText(QString::number(u16)); }
    if (m_ctrl->getWhiteBalanceTempAuto(u8)) m_wbAuto->setChecked(u8 != 0);

    // Exposure
    if (m_ctrl->getExposureAbs(u32)) { m_exposureSlider->setValue((int)u32); m_exposureLabel->setText(QString::number(u32)); }
    if (m_ctrl->getAeMode(u8)) {
        for (int i = 0; i < m_aeModeCombo->count(); i++) {
            if (m_aeModeCombo->itemData(i).toUInt() == u8) { m_aeModeCombo->setCurrentIndex(i); break; }
        }
    }
    if (m_ctrl->getAePriority(u8)) m_aePriorityCheck->setChecked(u8 != 0);

    // Focus
    if (m_ctrl->getFocusAbs(u16)) { m_focusSlider->setValue(u16); m_focusLabel->setText(QString::number(u16)); }
    if (m_ctrl->getFocusAuto(u8)) m_focusAuto->setChecked(u8 != 0);

    // Zoom
    if (m_ctrl->getZoomAbs(u16)) { m_zoomSlider->setValue(u16); m_zoomLabel->setText(QString::number(u16)); }

    // Iris
    if (m_ctrl->getIrisAbs(u16)) { m_irisSlider->setValue(u16); m_irisLabel->setText(QString::number(u16)); }

    // Pan/Tilt
    int32_t pan, tilt;
    if (m_ctrl->getPanTiltAbs(pan, tilt)) {
        m_panSlider->setValue((int)pan); m_panLabel->setText(QString::number(pan));
        m_tiltSlider->setValue((int)tilt); m_tiltLabel->setText(QString::number(tilt));
    }

    // Backlight
    if (m_ctrl->getBacklightComp(u16)) { m_backlightSlider->setValue(u16); m_backlightLabel->setText(QString::number(u16)); }

    // Power Line
    if (m_ctrl->getPowerLineFreq(u8)) {
        for (int i = 0; i < m_powerLineCombo->count(); i++) {
            if (m_powerLineCombo->itemData(i).toUInt() == u8) { m_powerLineCombo->setCurrentIndex(i); break; }
        }
    }

    m_updating = false;
}
