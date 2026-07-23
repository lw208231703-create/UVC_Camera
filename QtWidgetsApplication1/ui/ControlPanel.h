#pragma once

// I2C调试 & 日志面板开关: 设为 1 开启, 0 隐藏
#define UVC_DEBUG_PANELS 0

#include <QWidget>
#include <QScrollArea>
#include <QComboBox>
#include <QPushButton>
#include <QCheckBox>
#include <QLabel>
#include <QGroupBox>
#include <QTextEdit>
#include <QLineEdit>
#include <QVBoxLayout>

#include "ui/BitShiftSelector.h"

class CameraSettingsWidget;

class ControlPanel : public QWidget {
    Q_OBJECT

public:
    explicit ControlPanel(QWidget* parent = nullptr);

    // Device group
    QComboBox* deviceCombo()   { return m_deviceCombo; }
    QPushButton* openBtn()     { return m_openBtn; }
    QLabel* deviceInfoLabel()  { return m_deviceInfoLabel; }

    // Format group
    QComboBox* formatCombo()   { return m_formatCombo; }
    QComboBox* resolutionCombo(){ return m_resolutionCombo; }
    QPushButton* applyBtn()    { return m_applyBtn; }

    // Capture group
    QPushButton* snapshotBtn() { return m_snapshotBtn; }
    QCheckBox*   denoiseChk()  { return m_denoiseChk; }

    // Log area
    QTextEdit* logView()       { return m_logView; }

    // Camera settings
    CameraSettingsWidget* cameraSettings() { return m_cameraSettings; }

    // 16-bit shift selector
    BitShiftSelector* bitShiftSlider() { return m_bitShiftSelector; }

    // I2C register debug
    QLineEdit*   i2cAddrEdit()   { return m_i2cAddrEdit; }
    QLineEdit*   i2cRegEdit()    { return m_i2cRegEdit; }
    QLineEdit*   i2cLenEdit()    { return m_i2cLenEdit; }
    QLineEdit*   i2cDataEdit()   { return m_i2cDataEdit; }
    QPushButton* i2cReadBtn()    { return m_i2cReadBtn; }
    QPushButton* i2cWriteBtn()   { return m_i2cWriteBtn; }
    QLabel*      i2cResultLabel(){ return m_i2cResultLabel; }

    void setDeviceOpen(bool open);
    void setStreaming(bool streaming);
    void setI2cEnabled(bool enabled);

signals:
    void refreshDevices();

private:
    QGroupBox* makeGroup(const QString& title, QLayout* layout);

    // Device
    QComboBox*  m_deviceCombo;
    QPushButton* m_openBtn;
    QLabel*     m_deviceInfoLabel;

    // Format
    QComboBox*  m_formatCombo;
    QComboBox*  m_resolutionCombo;
    QPushButton* m_applyBtn;

    // Capture
    QPushButton* m_snapshotBtn;
    QCheckBox*   m_denoiseChk;

    // Log
    QTextEdit*  m_logView;

    // Camera settings
    CameraSettingsWidget* m_cameraSettings;

    // Bit-depth shift
    BitShiftSelector* m_bitShiftSelector;
    QLabel*  m_bitShiftLabel;

    // I2C register debug
    QLineEdit*   m_i2cAddrEdit = nullptr;
    QLineEdit*   m_i2cRegEdit = nullptr;
    QLineEdit*   m_i2cLenEdit = nullptr;
    QLineEdit*   m_i2cDataEdit = nullptr;
    QPushButton* m_i2cReadBtn = nullptr;
    QPushButton* m_i2cWriteBtn = nullptr;
    QLabel*      m_i2cResultLabel = nullptr;
    QWidget*     m_i2cGroup = nullptr;
};
