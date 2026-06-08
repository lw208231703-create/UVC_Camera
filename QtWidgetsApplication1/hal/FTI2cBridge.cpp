#include "FTI2cBridge.h"
#include <libusb.h>

FTI2cBridge::FTI2cBridge(libusb_device_handle* usb_handle, uint8_t i2c_addr)
    : m_handle(usb_handle), m_i2cAddr(i2c_addr) {}

bool FTI2cBridge::isValid() const { return m_handle != nullptr; }

uint8_t FTI2cBridge::i2cAddress() const { return m_i2cAddr; }
void   FTI2cBridge::setI2cAddress(uint8_t addr) { m_i2cAddr = addr; }

int FTI2cBridge::readReg(uint16_t regAddr, uint8_t* buf, int len) {
    if (!m_handle || !buf || len <= 0 || len > 256) return -1;
    return libusb_control_transfer(m_handle,
        0xC0,                              // Device-to-Host, Vendor, Device
        0x05,                              // FT602 I2C vendor command
        regAddr,                           // wValue: 寄存器地址
        (uint16_t)((len << 8) | m_i2cAddr), // wIndex: (长度<<8) | I2C从设备地址
        buf, len, 1000);
}

int FTI2cBridge::writeReg(uint16_t regAddr, const uint8_t* buf, int len) {
    if (!m_handle || !buf || len <= 0 || len > 256) return -1;
    return libusb_control_transfer(m_handle,
        0x40,                              // Host-to-Device, Vendor, Device
        0x05,                              // FT602 I2C vendor command
        regAddr,                           // wValue: 寄存器地址
        (uint16_t)((len << 8) | m_i2cAddr), // wIndex: (长度<<8) | I2C从设备地址
        const_cast<uint8_t*>(buf), len, 1000);
}
