#pragma once

#include <QWidget>
#include <QScrollArea>
#include <QComboBox>
#include <QPushButton>
#include <QCheckBox>
#include <QLabel>
#include <QGroupBox>
#include <QTextEdit>
#include <QVBoxLayout>

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
    QPushButton* recordBtn()   { return m_recordBtn; }

    // Log area
    QTextEdit* logView()       { return m_logView; }

    // Camera settings
    CameraSettingsWidget* cameraSettings() { return m_cameraSettings; }

    void setDeviceOpen(bool open);
    void setStreaming(bool streaming);

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
    QPushButton* m_recordBtn;

    // Log
    QTextEdit*  m_logView;

    // Camera settings
    CameraSettingsWidget* m_cameraSettings;
};
