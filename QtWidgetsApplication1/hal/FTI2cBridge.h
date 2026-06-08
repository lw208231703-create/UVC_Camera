#pragma once

#include <cstdint>

struct libusb_device_handle;

class FTI2cBridge {
public:
    explicit FTI2cBridge(libusb_device_handle* usb_handle, uint8_t i2c_addr = 0x0D);

    bool isValid() const;

    uint8_t i2cAddress() const;
    void   setI2cAddress(uint8_t addr);

    /// 读传感器寄存器，返回实际传输字节数，<0 表示失败
    int readReg(uint16_t regAddr, uint8_t* buf, int len);

    /// 写传感器寄存器，返回实际传输字节数，<0 表示失败
    int writeReg(uint16_t regAddr, const uint8_t* buf, int len);

private:
    libusb_device_handle* m_handle = nullptr;
    uint8_t m_i2cAddr = 0x0D;
};
