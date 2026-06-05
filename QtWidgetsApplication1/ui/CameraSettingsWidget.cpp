#include "CameraSettingsWidget.h"
#include "hal/UvcControls.h"
#include "infra/UiStrings.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QIntValidator>
#include <QTimer>

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
        m_exposureEdit->setValidator(new QIntValidator(0, 100000, m_exposureEdit));

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
