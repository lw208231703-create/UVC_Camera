#pragma once

#include <QObject>
#include <QByteArray>
#include <cstdint>

struct libusb_device_handle;

class ParameterWorker : public QObject {
    Q_OBJECT
public:
    explicit ParameterWorker(libusb_device_handle* handle, uint8_t i2cAddr = 0x0D, QObject* parent = nullptr);

public slots:
    void readReg(uint16_t regAddr, int len);
    void writeReg(uint16_t regAddr, const QByteArray& data);

    /// 读取时序寄存器：HMAX(0x56) + VMAX(0x45+0x46)
    void readTiming();

    /// 读取探测器温度：0x44 → bits[10:3]
    void readTemperature();

    /// 写入曝光：输入μs → 读 HMAX/VMAX → 算行数 → 写 0x42+0x43
    void writeExposure(uint32_t exposureUs, double pixelClockMHz);

    /// 读取曝光行数：读 0x42+0x43
    void readExposure();

    /// 读取所有参数：帧率/像素格式/ROI/AE/曝光/Gain
    void readAll();

signals:
    void regReadReady(uint16_t regAddr, QByteArray data, bool ok);
    void regWritten(uint16_t regAddr, bool ok);

    void timingReady(uint16_t hmax, uint32_t vmax);
    void temperatureReady(uint8_t tempCelsius);
    void exposureWritten(uint64_t actualUs, bool ok);
    void exposureReadReady(uint32_t lineCount);

    /// 全部参数就绪：fps, pixelFmt, roiX, roiY, exposureLines, aeMode
    void allReadReady(uint16_t fps, uint8_t pixelFmt, int roiX, int roiY,
                      uint32_t exposureLines, uint8_t aeMode);

private:
    libusb_device_handle* m_handle = nullptr;
    uint8_t m_i2cAddr = 0x0D;
};
