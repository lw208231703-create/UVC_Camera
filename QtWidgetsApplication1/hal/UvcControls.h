#pragma once

#include <cstdint>
#include <QString>

struct uvc_device_handle;

class UvcControls {
public:
    explicit UvcControls(uvc_device_handle* devh) : m_devh(devh) {}

    bool isValid() const { return m_devh != nullptr; }

    // ── Brightness ──
    bool getBrightness(int16_t& val);
    bool setBrightness(int16_t val);
    bool getBrightnessRange(int16_t& min, int16_t& max);

    // ── Contrast ──
    bool getContrast(uint16_t& val);
    bool setContrast(uint16_t val);
    bool getContrastRange(uint16_t& min, uint16_t& max);
    bool getContrastAuto(uint8_t& val);
    bool setContrastAuto(uint8_t val);

    // ── Gain ──
    bool getGain(uint16_t& val);
    bool setGain(uint16_t val);
    bool getGainRange(uint16_t& min, uint16_t& max);

    // ── Saturation ──
    bool getSaturation(uint16_t& val);
    bool setSaturation(uint16_t val);
    bool getSaturationRange(uint16_t& min, uint16_t& max);

    // ── Sharpness ──
    bool getSharpness(uint16_t& val);
    bool setSharpness(uint16_t val);
    bool getSharpnessRange(uint16_t& min, uint16_t& max);

    // ── Gamma ──
    bool getGamma(uint16_t& val);
    bool setGamma(uint16_t val);
    bool getGammaRange(uint16_t& min, uint16_t& max);

    // ── White Balance ──
    bool getWhiteBalanceTemp(uint16_t& val);
    bool setWhiteBalanceTemp(uint16_t val);
    bool getWhiteBalanceTempRange(uint16_t& min, uint16_t& max);
    bool getWhiteBalanceTempAuto(uint8_t& val);
    bool setWhiteBalanceTempAuto(uint8_t val);
    bool getWhiteBalanceComponent(uint16_t& blue, uint16_t& red);
    bool setWhiteBalanceComponent(uint16_t blue, uint16_t red);
    bool getWhiteBalanceComponentAuto(uint8_t& val);
    bool setWhiteBalanceComponentAuto(uint8_t val);

    // ── Exposure ──
    bool getExposureAbs(uint32_t& val);
    bool setExposureAbs(uint32_t val);
    bool getExposureAbsRange(uint32_t& min, uint32_t& max);
    bool getAeMode(uint8_t& val);
    bool setAeMode(uint8_t val);
    bool getAePriority(uint8_t& val);
    bool setAePriority(uint8_t val);

    // ── Focus ──
    bool getFocusAbs(uint16_t& val);
    bool setFocusAbs(uint16_t val);
    bool getFocusAbsRange(uint16_t& min, uint16_t& max);
    bool getFocusAuto(uint8_t& val);
    bool setFocusAuto(uint8_t val);

    // ── Zoom ──
    bool getZoomAbs(uint16_t& val);
    bool setZoomAbs(uint16_t val);
    bool getZoomAbsRange(uint16_t& min, uint16_t& max);

    // ── Pan / Tilt ──
    bool getPanTiltAbs(int32_t& pan, int32_t& tilt);
    bool setPanTiltAbs(int32_t pan, int32_t tilt);
    bool getPanTiltAbsRange(int32_t& panMin, int32_t& panMax, int32_t& tiltMin, int32_t& tiltMax);

    // ── Backlight Compensation ──
    bool getBacklightComp(uint16_t& val);
    bool setBacklightComp(uint16_t val);
    bool getBacklightCompRange(uint16_t& min, uint16_t& max);

    // ── Power Line Frequency ──
    bool getPowerLineFreq(uint8_t& val);
    bool setPowerLineFreq(uint8_t val);

    // ── Hue ──
    bool getHue(int16_t& val);
    bool setHue(int16_t val);
    bool getHueRange(int16_t& min, int16_t& max);
    bool getHueAuto(uint8_t& val);
    bool setHueAuto(uint8_t val);

    // ── Scanning Mode ──
    bool getScanningMode(uint8_t& val);
    bool setScanningMode(uint8_t val);

    // ── Iris ──
    bool getIrisAbs(uint16_t& val);
    bool setIrisAbs(uint16_t val);
    bool getIrisAbsRange(uint16_t& min, uint16_t& max);

    // ── Digital Multiplier ──
    bool getDigitalMultiplier(uint16_t& val);
    bool setDigitalMultiplier(uint16_t val);

    // ── Privacy ──
    bool getPrivacy(uint8_t& val);
    bool setPrivacy(uint8_t val);

    // ── Power Mode ──
    bool getPowerMode(uint8_t& mode);
    bool setPowerMode(uint8_t mode);

private:
    uvc_device_handle* m_devh = nullptr;
};
