#pragma once

#include <QWidget>
#include <QScrollArea>
#include <QSlider>
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QGroupBox>

class UvcControls;

class CameraSettingsWidget : public QScrollArea {
    Q_OBJECT

public:
    explicit CameraSettingsWidget(QWidget* parent = nullptr);

    void setControls(UvcControls* ctrl);
    void clearControls();
    void refreshAll();

private:
    void setupUi();
    QGroupBox* makeGroup(const QString& title);

    // Helper: creates a slider + label row
    QWidget* makeSliderRow(const QString& name, QSlider*& slider, QLabel*& valueLabel,
                           int min, int max, int val);
    QWidget* makeCheckRow(const QString& name, QCheckBox*& check, bool checked);
    QWidget* makeComboRow(const QString& name, QComboBox*& combo);

    void connectSlider(QSlider* slider, QLabel* label, double scale,
                       std::function<bool(int)> setter);

    UvcControls* m_ctrl = nullptr;
    QWidget* m_content;
    bool m_updating = false;

    // ── Brightness ──
    QSlider* m_brightnessSlider;
    QLabel*  m_brightnessLabel;

    // ── Contrast ──
    QSlider*  m_contrastSlider;
    QLabel*   m_contrastLabel;
    QCheckBox* m_contrastAuto;

    // ── Gain ──
    QSlider* m_gainSlider;
    QLabel*  m_gainLabel;

    // ── Saturation ──
    QSlider* m_saturationSlider;
    QLabel*  m_saturationLabel;

    // ── Sharpness ──
    QSlider* m_sharpnessSlider;
    QLabel*  m_sharpnessLabel;

    // ── Gamma ──
    QSlider* m_gammaSlider;
    QLabel*  m_gammaLabel;

    // ── White Balance ──
    QSlider*  m_wbTempSlider;
    QLabel*   m_wbTempLabel;
    QCheckBox* m_wbAuto;

    // ── Exposure ──
    QSlider*  m_exposureSlider;
    QLabel*   m_exposureLabel;
    QComboBox* m_aeModeCombo;
    QCheckBox* m_aePriorityCheck;

    // ── Focus ──
    QSlider*  m_focusSlider;
    QLabel*   m_focusLabel;
    QCheckBox* m_focusAuto;

    // ── Hue ──
    QSlider*  m_hueSlider;
    QLabel*   m_hueLabel;
    QCheckBox* m_hueAuto;

    // ── Backlight ──
    QSlider* m_backlightSlider;
    QLabel*  m_backlightLabel;

    // ── Power Line Freq ──
    QComboBox* m_powerLineCombo;

    // ── Zoom ──
    QSlider* m_zoomSlider;
    QLabel*  m_zoomLabel;

    // ── Iris ──
    QSlider* m_irisSlider;
    QLabel*  m_irisLabel;

    // ── Pan/Tilt ──
    QSlider* m_panSlider;
    QLabel*  m_panLabel;
    QSlider* m_tiltSlider;
    QLabel*  m_tiltLabel;
};
