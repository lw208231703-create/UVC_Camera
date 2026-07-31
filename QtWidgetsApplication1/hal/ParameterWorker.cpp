#include "ParameterWorker.h"
#include <QThread>
#include <libusb.h>
#include <cmath>

ParameterWorker::ParameterWorker(libusb_device_handle* handle, uint8_t i2cAddr, QObject* parent)
    : QObject(parent), m_handle(handle), m_i2cAddr(i2cAddr) {}

void ParameterWorker::readReg(uint16_t regAddr, int len) {
    if (!m_handle || len <= 0 || len > 256) {
        emit regReadReady(regAddr, QByteArray(), false);
        return;
    }
    QByteArray buf(len, 0);
    int ret = libusb_control_transfer(m_handle,
        0xC0, 0x05, regAddr,
        (uint16_t)((len << 8) | m_i2cAddr),
        (uint8_t*)buf.data(), len, 1000);
    if (ret > 0) {
        buf.resize(ret);
        emit regReadReady(regAddr, buf, true);
    } else {
        emit regReadReady(regAddr, QByteArray(), false);
    }
}

void ParameterWorker::writeReg(uint16_t regAddr, const QByteArray& data) {
    if (!m_handle || data.isEmpty() || data.size() > 256) {
        emit regWritten(regAddr, false);
        return;
    }
    QByteArray writable = data;
    int ret = libusb_control_transfer(m_handle,
        0x40, 0x05, regAddr,
        (uint16_t)((data.size() << 8) | m_i2cAddr),
        (uint8_t*)writable.data(), data.size(), 1000);
    emit regWritten(regAddr, ret > 0);
}

void ParameterWorker::readTiming() {
    if (!m_handle) { emit timingReady(0, 0); return; }

    uint8_t hmaxBuf[2] = {};
    int ret = libusb_control_transfer(m_handle, 0xC0, 0x05, 0x56,
        (2 << 8) | m_i2cAddr, hmaxBuf, 2, 1000);
    if (ret < 2) { emit timingReady(0, 0); return; }
    uint16_t hmax = hmaxBuf[0] | (hmaxBuf[1] << 8);

    uint8_t vmaxHBuf[2] = {};
    ret = libusb_control_transfer(m_handle, 0xC0, 0x05, 0x45,
        (2 << 8) | m_i2cAddr, vmaxHBuf, 2, 1000);
    if (ret < 2) { emit timingReady(hmax, 0); return; }
    uint8_t vmaxLBuf[2] = {};
    ret = libusb_control_transfer(m_handle, 0xC0, 0x05, 0x46,
        (2 << 8) | m_i2cAddr, vmaxLBuf, 2, 1000);
    if (ret < 2) { emit timingReady(hmax, 0); return; }

    uint32_t vmax = (((uint32_t)(vmaxHBuf[0] | (vmaxHBuf[1] << 8)) & 0xFF) << 16)
                  | (uint32_t)(vmaxLBuf[0] | (vmaxLBuf[1] << 8));
    emit timingReady(hmax, vmax);
}

void ParameterWorker::readTemperature() {
    if (!m_handle) { emit temperatureReady(0); return; }
    uint8_t buf[2] = {};
    int ret = libusb_control_transfer(m_handle, 0xC0, 0x05, 0x44,
        (2 << 8) | m_i2cAddr, buf, 2, 1000);
    if (ret < 2) { emit temperatureReady(0); return; }
    uint16_t raw = (uint16_t)(buf[0] | (buf[1] << 8));
    uint8_t temp = ((raw & 0x0FFF) >> 3) & 0xFF;
    emit temperatureReady(temp);
}

void ParameterWorker::writeExposure(uint32_t exposureUs, double pixelClockMHz) {
    if (!m_handle) { emit exposureWritten(0, false); return; }

    uint8_t hmaxBuf[2] = {};
    int ret = libusb_control_transfer(m_handle, 0xC0, 0x05, 0x56,
        (2 << 8) | m_i2cAddr, hmaxBuf, 2, 1000);
    if (ret < 2) { emit exposureWritten(0, false); return; }
    uint16_t hmax = hmaxBuf[0] | (hmaxBuf[1] << 8);

    uint8_t vmaxHBuf[2] = {};
    ret = libusb_control_transfer(m_handle, 0xC0, 0x05, 0x45,
        (2 << 8) | m_i2cAddr, vmaxHBuf, 2, 1000);
    if (ret < 2) { emit exposureWritten(0, false); return; }
    uint8_t vmaxLBuf[2] = {};
    ret = libusb_control_transfer(m_handle, 0xC0, 0x05, 0x46,
        (2 << 8) | m_i2cAddr, vmaxLBuf, 2, 1000);
    if (ret < 2) { emit exposureWritten(0, false); return; }

    uint32_t vmax = (((uint32_t)(vmaxHBuf[0] | (vmaxHBuf[1] << 8)) & 0xFF) << 16)
                  | (uint32_t)(vmaxLBuf[0] | (vmaxLBuf[1] << 8));

    double tLineUs = (double)hmax / pixelClockMHz;
    int rawLines = (int)std::ceil((double)exposureUs / tLineUs);
    int maxLines = vmax > 52 ? (int)vmax - 52 : 1;
    int lines = qBound(1, rawLines, maxLines);
    double actualUs = (double)lines * tLineUs;

    uint32_t expVal = (uint32_t)lines;
    uint8_t highBuf[2] = { (uint8_t)(expVal >> 16), (uint8_t)(expVal >> 24) };
    uint8_t lowBuf[2]  = { (uint8_t)(expVal & 0xFF), (uint8_t)((expVal >> 8) & 0xFF) };

    ret = libusb_control_transfer(m_handle, 0x40, 0x05, 0x42,
        (2 << 8) | m_i2cAddr, highBuf, 2, 1000);
    if (ret < 2) { emit exposureWritten(0, false); return; }

    ret = libusb_control_transfer(m_handle, 0x40, 0x05, 0x43,
        (2 << 8) | m_i2cAddr, lowBuf, 2, 1000);
    emit exposureWritten((uint64_t)actualUs, ret >= 2);
}

void ParameterWorker::readExposure() {
    if (!m_handle) { emit exposureReadReady(0); return; }

    uint8_t expHBuf[2] = {};
    int ret = libusb_control_transfer(m_handle, 0xC0, 0x05, 0x42,
        (2 << 8) | m_i2cAddr, expHBuf, 2, 1000);
    if (ret < 2) { emit exposureReadReady(0); return; }
    uint8_t expLBuf[2] = {};
    ret = libusb_control_transfer(m_handle, 0xC0, 0x05, 0x43,
        (2 << 8) | m_i2cAddr, expLBuf, 2, 1000);
    if (ret < 2) { emit exposureReadReady(0); return; }

    uint32_t expVal = ((uint32_t)(expHBuf[0] | (expHBuf[1] << 8)) << 16)
                    | (uint32_t)(expLBuf[0] | (expLBuf[1] << 8));
    emit exposureReadReady(expVal);
}

void ParameterWorker::readAll() {
    if (!m_handle) return;
    auto ms = []() { QThread::msleep(3); };

    auto read2 = [&](uint16_t addr) -> uint16_t {
        uint8_t buf[2] = {};
        int r = libusb_control_transfer(m_handle, 0xC0, 0x05, addr,
            (2 << 8) | m_i2cAddr, buf, 2, 1000);
        return (r >= 2) ? (uint16_t)(buf[0] | (buf[1] << 8)) : 0;
    };

    ms(); uint16_t fps = read2(0x10);
    ms(); uint16_t gain = read2(0x4C);
    ms(); uint16_t roiXVal = read2(0x47);
    ms(); uint16_t roiYVal = read2(0x48);
    ms(); uint8_t expHBuf[2] = {};
    int ret = libusb_control_transfer(m_handle, 0xC0, 0x05, 0x42,
        (2 << 8) | m_i2cAddr, expHBuf, 2, 1000);
    ms(); uint8_t expLBuf[2] = {};
    ret = libusb_control_transfer(m_handle, 0xC0, 0x05, 0x43,
        (2 << 8) | m_i2cAddr, expLBuf, 2, 1000);
    ms(); uint8_t fmtBuf[1] = {};
    ret = libusb_control_transfer(m_handle, 0xC0, 0x05, 0x50,
        (1 << 8) | m_i2cAddr, fmtBuf, 1, 1000);
    ms(); uint8_t aeBuf[1] = {};
    ret = libusb_control_transfer(m_handle, 0xC0, 0x05, 0x40,
        (1 << 8) | m_i2cAddr, aeBuf, 1, 1000);
    ms(); uint8_t triggerBuf[2] = {};
    ret = libusb_control_transfer(m_handle, 0xC0, 0x05, 0x4B,
        (2 << 8) | m_i2cAddr, triggerBuf, 2, 1000);
    uint8_t triggerMode = (ret >= 1) ? triggerBuf[0] : 0;

    int roiX = (int)roiXVal - 112;
    if (roiX < 0) roiX = 0;
    int roiY = (int)roiYVal - 4;
    if (roiY < 0) roiY = 0;
    uint8_t pixelFmt = (ret >= 1) ? fmtBuf[0] : 0;
    uint8_t aeMode = (ret >= 1) ? aeBuf[0] : 0;
    uint32_t expLines = 0;
    if (ret >= 2)
        expLines = ((uint32_t)(expHBuf[0] | (expHBuf[1] << 8)) << 16)
                 | (uint32_t)(expLBuf[0] | (expLBuf[1] << 8));

    emit allReadReady(fps, gain, pixelFmt, roiX, roiY, expLines, aeMode, triggerMode);
}
