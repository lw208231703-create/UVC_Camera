#pragma once

#include <QWidget>
#include <QScrollArea>
#include <QSlider>
#include <QCheckBox>
#include <QComboBox>
#include <QLineEdit>
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

    QWidget* makeSliderRow(const QString& name, QSlider*& slider, QLabel*& valueLabel,
                           int min, int max, int val);
    QWidget* makeInputRow(const QString& name, QLineEdit*& edit, const QString& initialText);
    QWidget* makeComboRow(const QString& name, QComboBox*& combo);
    QWidget* makeCheckRow(const QString& name, QCheckBox*& check, bool checked);

    void connectSlider(QSlider* slider, QLabel* label, double scale,
                       std::function<bool(int)> setter);

    UvcControls* m_ctrl = nullptr;
    QWidget* m_content;
    bool m_updating = false;

    // ── Gain ──
    QSlider* m_gainSlider;
    QLabel*  m_gainLabel;

    // ── Exposure ──
    QLineEdit* m_exposureEdit;
    QComboBox* m_aeModeCombo;
    QCheckBox* m_aePriorityCheck;
};
