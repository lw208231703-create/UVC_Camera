#pragma once

#include <QWidget>
#include <QScrollArea>
#include <QSlider>
#include <QCheckBox>
#include <QComboBox>
#include <QLineEdit>
#include <QLabel>
#include <QGroupBox>
#include <QDoubleSpinBox>
#include <QPushButton>
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
    QCheckBox* m_aePriorityCheck;

    // ── Exposure Timing Conversion ──
    QDoubleSpinBox* m_pixelClockSpin;   // f_clk input (MHz)
    QPushButton*    m_readTimingBtn;    // read HMAX/VMAX from register
    QLabel* m_hmaxLabel;
    QLabel* m_vmaxLabel;
    QLabel* m_tlineLabel;              // T_line (μs)
    QLabel* m_tframeLabel;             // T_frame (ms)
    QLabel* m_fpsLabel;                // max FPS
    QLabel* m_expRangeLabel;           // exposure range in μs
    QDoubleSpinBox* m_usExpSpin;       // input exposure in μs
    QLabel* m_lineResultLabel;         // converted line count
    QLabel* m_actualUsLabel;           // actual μs after rounding
    QPushButton* m_applyLineBtn;       // apply calculated lines to Exp. Time
    bool m_timingValid = false;
    uint16_t m_hmax = 0;
    uint32_t m_vmax = 0;
    double   m_pixelClockMHz = 74.25;

    void recalcTiming();               // recalculate T_line, range, etc.
    void onUsExposureChanged();        // convert μs → lines
    void readTimingRegisters();        // read 0x51 for HMAX/VMAX
};
