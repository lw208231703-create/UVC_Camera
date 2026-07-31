#include "CameraSettingsWidget.h"
#include "hal/UvcControls.h"
#include "hal/FTI2cBridge.h"
#include "hal/ParameterWorker.h"
#include "infra/UiStrings.h"
#include "infra/LogManager.h"
#include <QThread>
#include <QMessageBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QWheelEvent>

class WheelFilter : public QObject {
public:
    using QObject::QObject;
protected:
    bool eventFilter(QObject* obj, QEvent* event) override {
        if (event->type() == QEvent::Wheel) return true;
        return QObject::eventFilter(obj, event);
    }
};
static WheelFilter* s_wheelFilter = nullptr;
static void disableWheel(QComboBox* cb) {
    if (!s_wheelFilter) s_wheelFilter = new WheelFilter;
    cb->installEventFilter(s_wheelFilter);
}
#include <QIntValidator>
#include <QMetaObject>
#include <QTimer>
#include <QFrame>
#include <cmath>

CameraSettingsWidget::CameraSettingsWidget(QWidget* parent)
    : QWidget(parent)
{
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
    edit->setAlignment(Qt::AlignLeft);
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
    disableWheel(combo);
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
    auto* main = new QVBoxLayout(this);
    main->setSpacing(6);
    main->setContentsMargins(0, 0, 0, 0);

    // ── Gain ──
    {
        auto* grp = makeGroup(TR("Gain"));
        auto* lay = qobject_cast<QVBoxLayout*>(grp->layout());

        lay->addWidget(makeInputRow(TR("Gain"), m_gainEdit, "0"));
        m_gainEdit->setValidator(new QIntValidator(0, 420, m_gainEdit));
        connect(m_gainEdit, &QLineEdit::editingFinished, this, [this]() {
            if (m_updating || !m_paramWorker) return;
            bool ok;
            int val = m_gainEdit->text().toInt(&ok);
            if (!ok) return;
            if (val < 0) val = 0;
            if (val > 420) val = 420;
            uint8_t data[2] = { (uint8_t)(val & 0xFF), (uint8_t)(val >> 8) };
            QByteArray d((const char*)data, 2);
            auto* w = m_paramWorker;
            QMetaObject::invokeMethod(w, [w, d]() { w->writeReg(0x4C, d); }, Qt::QueuedConnection);
        });

        main->addWidget(grp);
    }

    // ── Exposure ──
    {
        auto* grp = makeGroup(TR("Exposure"));
        auto* lay = qobject_cast<QVBoxLayout*>(grp->layout());

        lay->addWidget(makeInputRow(TR("Exp. Time (μs)"), m_exposureEdit, "10000"));
        m_exposureEdit->setValidator(new QIntValidator(0, 10000000, m_exposureEdit));

        auto applyExposure = [this]() {
            if (m_updating || !m_ctrl) return;
            bool ok;
            uint32_t val = (uint32_t)m_exposureEdit->text().toUInt(&ok);
            if (!ok) return;

            if (m_paramWorker) {
                double pc = m_pixelClockMHz;
                QMetaObject::invokeMethod(m_paramWorker, [wk = m_paramWorker, val, pc]() {
                    wk->writeExposure(val, pc);
                }, Qt::QueuedConnection);
            }
        };

        connect(m_exposureEdit, &QLineEdit::editingFinished, this, applyExposure);

        lay->addWidget(makeComboRow(TR("AE Mode"), m_aeModeCombo));
        m_aeModeCombo->addItem(TR("Manual"), 1);
        m_aeModeCombo->addItem(TR("Auto"), 2);
        connect(m_aeModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
                [this](int idx) {
            if (m_updating) return;
            if (auto* w = m_paramWorker) {
                uint8_t data[1] = { (uint8_t)(m_aeModeCombo->itemData(idx).toUInt() == 2 ? 0x01 : 0x00) };
                QByteArray d((const char*)data, 1);
                QMetaObject::invokeMethod(w, [w, d]() { w->writeReg(0x40, d); }, Qt::QueuedConnection);
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
            if (m_updating || !m_paramWorker) return;
            bool ok;
            int val = m_fpsEdit->text().toInt(&ok);
            if (!ok) return;
            if (val < 10) { val = 10; m_fpsEdit->setText(QString::number(val)); }
            uint8_t data[2] = { (uint8_t)(val & 0xFF), (uint8_t)(val >> 8) };
            QByteArray d((const char*)data, 2);
            auto* w = m_paramWorker;
            QMetaObject::invokeMethod(w, [w, d]() { w->writeReg(0x10, d); }, Qt::QueuedConnection);
        });

        lay->addWidget(makeComboRow(TR("像素格式"), m_pixelFormatCombo));
        m_pixelFormatCombo->addItem("12bit", 1);
        m_pixelFormatCombo->addItem("8bit", 2);
        connect(m_pixelFormatCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
                [this](int idx) {
            if (m_updating || !m_paramWorker) return;
            uint8_t data[1] = { (uint8_t)m_pixelFormatCombo->itemData(idx).toUInt() };
            QByteArray fmtData((const char*)data, 1);
            uint8_t expHigh[2] = { 0x00, 0x00 };
            uint8_t expLow[2]  = { 0x02, 0x00 };
            QByteArray eh((const char*)expHigh, 2), el((const char*)expLow, 2);
            auto* w = m_paramWorker;
            QMetaObject::invokeMethod(w, [w, fmtData, eh, el]() {
                w->writeReg(0x50, fmtData);
                w->writeReg(0x42, eh);
                w->writeReg(0x43, el);
            }, Qt::QueuedConnection);
        });

        lay->addWidget(makeInputRow(TR("开窗X"), m_roiXEdit, "0"));
        m_roiXEdit->setValidator(new QIntValidator(0, 2560, m_roiXEdit));
        connect(m_roiXEdit, &QLineEdit::editingFinished, this, [this]() {
            if (m_updating || !m_paramWorker) return;
            bool ok;
            int val = m_roiXEdit->text().toInt(&ok);
            if (!ok) return;
            int alignedX = (val / 8) * 8;
            uint16_t regVal = (uint16_t)(alignedX + 112);
            uint8_t data[2] = { (uint8_t)(regVal & 0xFF), (uint8_t)(regVal >> 8) };
            QByteArray d((const char*)data, 2);
            auto* w = m_paramWorker;
            QMetaObject::invokeMethod(w, [w, d]() { w->writeReg(0x47, d); }, Qt::QueuedConnection);
            m_roiXEdit->setText(QString::number(alignedX));
        });

        lay->addWidget(makeInputRow(TR("开窗Y"), m_roiYEdit, "0"));
        m_roiYEdit->setValidator(new QIntValidator(0, 2048, m_roiYEdit));
        connect(m_roiYEdit, &QLineEdit::editingFinished, this, [this]() {
            if (m_updating || !m_paramWorker) return;
            bool ok;
            int val = m_roiYEdit->text().toInt(&ok);
            if (!ok) return;
            int alignedY = (val / 4) * 4;
            uint16_t regVal = (uint16_t)(alignedY + 4);
            uint8_t data[2] = { (uint8_t)(regVal & 0xFF), (uint8_t)(regVal >> 8) };
            QByteArray d((const char*)data, 2);
            auto* w = m_paramWorker;
            QMetaObject::invokeMethod(w, [w, d]() { w->writeReg(0x48, d); }, Qt::QueuedConnection);
            m_roiYEdit->setText(QString::number(alignedY));
        });

        lay->addWidget(makeComboRow(TR("触发模式"), m_triggerModeCombo));
        m_triggerModeCombo->addItem(TR("固定帧频"), 0);
        m_triggerModeCombo->addItem(TR("外触发"), 1);
        connect(m_triggerModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
                [this](int idx) {
            if (m_updating || !m_paramWorker) return;
            uint8_t val = m_triggerModeCombo->itemData(idx).toUInt() ? 0x01 : 0x00;
            QByteArray d(1, val);
            auto* w = m_paramWorker;
            QMetaObject::invokeMethod(w, [w, d]() { w->writeReg(0x4B, d); }, Qt::QueuedConnection);
        });

        main->addWidget(grp);
    }

    main->addStretch();
}

void CameraSettingsWidget::setControls(UvcControls* ctrl) {
    m_ctrl = ctrl;
    setEnabled(ctrl != nullptr);
    if (ctrl) refreshAll();
}

void CameraSettingsWidget::setI2cBridge(FTI2cBridge* bridge) {
    m_i2cBridge = bridge;
    if (bridge && !m_paramWorker)
        readTimingRegisters();
}

void CameraSettingsWidget::setParamWorker(ParameterWorker* worker) {
    m_paramWorker = worker;
    if (!worker) return;

    // 连接信号：实现异步结果 → UI 更新
    connect(worker, &ParameterWorker::timingReady, this, [this](uint16_t hmax, uint32_t vmax) {
        m_hmax = hmax;
        m_vmax = vmax;
        m_timingValid = (hmax > 0);
        if (m_timingValid)
            LOG_INFO(QString("Timing: HMAX=%1, VMAX=%2").arg(hmax).arg(vmax));
    });

    connect(worker, &ParameterWorker::exposureWritten, this, [this](uint64_t actualUs, bool ok) {
        if (ok) {
            m_updating = true;
            m_exposureEdit->setText(QString::number((int)actualUs));
            m_updating = false;
        }
    });

    connect(worker, &ParameterWorker::allReadReady, this, [this](
        uint16_t fps, uint16_t gain, uint8_t pixelFmt, int roiX, int roiY,
        uint32_t exposureLines, uint8_t aeMode, uint8_t triggerMode) {
        m_updating = true;
        m_fpsEdit->setText(QString::number(fps));
        m_gainEdit->setText(QString::number(gain));
        for (int i = 0; i < m_pixelFormatCombo->count(); i++) {
            if (m_pixelFormatCombo->itemData(i).toUInt() == pixelFmt) {
                m_pixelFormatCombo->setCurrentIndex(i); break;
            }
        }
        m_roiXEdit->setText(QString::number(roiX));
        m_roiYEdit->setText(QString::number(roiY));
        int mode = aeMode ? 2 : 1;
        for (int i = 0; i < m_aeModeCombo->count(); i++) {
            if (m_aeModeCombo->itemData(i).toUInt() == (uint)mode) { m_aeModeCombo->setCurrentIndex(i); break; }
        }
        for (int i = 0; i < m_triggerModeCombo->count(); i++) {
            if (m_triggerModeCombo->itemData(i).toUInt() == triggerMode) { m_triggerModeCombo->setCurrentIndex(i); break; }
        }
        // 曝光: 行数 → μs
        if (m_timingValid) {
            double tLineUs = (double)m_hmax / m_pixelClockMHz;
            m_exposureEdit->setText(QString::number((int)(exposureLines * tLineUs)));
        } else {
            m_exposureEdit->setText(QString::number(exposureLines));
        }
        m_updating = false;
    });

    // 异步读取时序 + 全部参数
    QMetaObject::invokeMethod(worker, [worker]() { worker->readTiming(); }, Qt::QueuedConnection);
    QMetaObject::invokeMethod(worker, [worker]() { worker->readAll(); }, Qt::QueuedConnection);
}

void CameraSettingsWidget::clearControls() {
    m_ctrl = nullptr;
    m_i2cBridge = nullptr;
    m_paramWorker = nullptr;
    setEnabled(false);
    m_timingValid = false;
}

void CameraSettingsWidget::refreshAll() {
    if (!m_ctrl || !m_ctrl->isValid()) return;
    m_updating = true;

    // 其余参数通过 ParameterWorker 异步读取（先等 3ms 让传感器稳定）
    if (m_paramWorker) {
        QMetaObject::invokeMethod(m_paramWorker, [this]() {
            QThread::msleep(3);
            m_paramWorker->readAll();
        }, Qt::QueuedConnection);
    }

    m_updating = false;
}

// ── Read HMAX/VMAX（同步备选，仅用于 m_i2cBridge 无 ParameterWorker 时）──
void CameraSettingsWidget::readTimingRegisters() {
    if (!m_i2cBridge || !m_i2cBridge->isValid()) {
        LOG_WARNING("I2C bridge not available for reading timing registers");
        return;
    }

    uint8_t hmaxBuf[2] = {};
    int ret = m_i2cBridge->readReg(0x56, hmaxBuf, 2);
    if (ret < 2) {
        LOG_ERROR(QString("Failed to read HMAX register 0x56 (ret=%1)").arg(ret));
        m_timingValid = false;
        return;
    }
    m_hmax = (uint16_t)(hmaxBuf[0] | (hmaxBuf[1] << 8));

    uint8_t vmaxHBuf[2] = {};
    ret = m_i2cBridge->readReg(0x45, vmaxHBuf, 2);
    if (ret < 2) { m_timingValid = false; return; }
    uint8_t vmaxLBuf[2] = {};
    ret = m_i2cBridge->readReg(0x46, vmaxLBuf, 2);
    if (ret < 2) { m_timingValid = false; return; }
    m_vmax = (((uint32_t)(vmaxHBuf[0] | (vmaxHBuf[1] << 8)) & 0xFF) << 16)
           | (uint32_t)(vmaxLBuf[0] | (vmaxLBuf[1] << 8));

    LOG_INFO(QString("Timing: HMAX=%1, VMAX=%2").arg(m_hmax).arg(m_vmax));
    m_timingValid = true;
    recalcTiming();
}

void CameraSettingsWidget::recalcTiming() {
}


