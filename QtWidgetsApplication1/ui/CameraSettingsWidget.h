#pragma once

#include <QWidget>
#include <QScrollArea>
#include <QSlider>
#include <QCheckBox>
#include <QComboBox>
#include <QLineEdit>
#include <QLabel>
#include <QGroupBox>

#include <functional>

class UvcControls;
class FTI2cBridge;

class CameraSettingsWidget : public QScrollArea {
    Q_OBJECT

public:
    explicit CameraSettingsWidget(QWidget* parent = nullptr);

    void setControls(UvcControls* ctrl);
    void setI2cBridge(FTI2cBridge* bridge);
    void clearControls();
    void refreshAll();

private:
    void setupUi();
    QGroupBox* makeGroup(const QString& title);

    QWidget* makeSliderRow(const QString& name, QSlider*& slider, QLabel*& valueLabel,
                           int min, int max, int val);
    QWidget* makeInputRow(const QString& name, QLineEdit*& edit, const QString& initialText);
    QWidget* makeComboRow(const QString& name, QComboBox*& combo);
    QWidget* makeCheckRow(const QString& name, QCheckBox*& check, bool checked);

    void connectSlider(QSlider* slider, QLabel* label, double scale,
                       std::function<bool(int)> setter);

    UvcControls* m_ctrl = nullptr;
    FTI2cBridge* m_i2cBridge = nullptr;
    QWidget* m_content;
    bool m_updating = false;

    // ── Gain ──
    QSlider* m_gainSlider;
    QLabel*  m_gainLabel;

    // ── Exposure ──
    QLineEdit* m_exposureEdit;
    QComboBox* m_aeModeCombo;
    QLineEdit* m_fpsEdit;
    QComboBox* m_pixelFormatCombo;

    // ── Exposure Timing Conversion ──
    bool m_timingValid = false;
    uint16_t m_hmax = 0;
    uint32_t m_vmax = 0;
    double   m_pixelClockMHz = 74.25;

    void recalcTiming();               // recalculate T_line, range, etc.
    void readTimingRegisters();        // read 0x51 for HMAX/VMAX
};
