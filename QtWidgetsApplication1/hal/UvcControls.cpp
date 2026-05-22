#include "UvcControls.h"
#include <libuvc/libuvc.h>

// ── Brightness ──
bool UvcControls::getBrightness(int16_t& val)     { return uvc_get_brightness(m_devh, &val, UVC_GET_CUR) >= 0; }
bool UvcControls::setBrightness(int16_t val)       { return uvc_set_brightness(m_devh, val) >= 0; }
bool UvcControls::getBrightnessRange(int16_t& min, int16_t& max) {
    return uvc_get_brightness(m_devh, &min, UVC_GET_MIN) >= 0 && uvc_get_brightness(m_devh, &max, UVC_GET_MAX) >= 0;
}

// ── Contrast ──
bool UvcControls::getContrast(uint16_t& val)       { return uvc_get_contrast(m_devh, &val, UVC_GET_CUR) >= 0; }
bool UvcControls::setContrast(uint16_t val)         { return uvc_set_contrast(m_devh, val) >= 0; }
bool UvcControls::getContrastRange(uint16_t& min, uint16_t& max) {
    return uvc_get_contrast(m_devh, &min, UVC_GET_MIN) >= 0 && uvc_get_contrast(m_devh, &max, UVC_GET_MAX) >= 0;
}
bool UvcControls::getContrastAuto(uint8_t& val)     { return uvc_get_contrast_auto(m_devh, &val, UVC_GET_CUR) >= 0; }
bool UvcControls::setContrastAuto(uint8_t val)       { return uvc_set_contrast_auto(m_devh, val) >= 0; }

// ── Gain ──
bool UvcControls::getGain(uint16_t& val)            { return uvc_get_gain(m_devh, &val, UVC_GET_CUR) >= 0; }
bool UvcControls::setGain(uint16_t val)              { return uvc_set_gain(m_devh, val) >= 0; }
bool UvcControls::getGainRange(uint16_t& min, uint16_t& max) {
    return uvc_get_gain(m_devh, &min, UVC_GET_MIN) >= 0 && uvc_get_gain(m_devh, &max, UVC_GET_MAX) >= 0;
}

// ── Saturation ──
bool UvcControls::getSaturation(uint16_t& val)      { return uvc_get_saturation(m_devh, &val, UVC_GET_CUR) >= 0; }
bool UvcControls::setSaturation(uint16_t val)        { return uvc_set_saturation(m_devh, val) >= 0; }
bool UvcControls::getSaturationRange(uint16_t& min, uint16_t& max) {
    return uvc_get_saturation(m_devh, &min, UVC_GET_MIN) >= 0 && uvc_get_saturation(m_devh, &max, UVC_GET_MAX) >= 0;
}

// ── Sharpness ──
bool UvcControls::getSharpness(uint16_t& val)       { return uvc_get_sharpness(m_devh, &val, UVC_GET_CUR) >= 0; }
bool UvcControls::setSharpness(uint16_t val)         { return uvc_set_sharpness(m_devh, val) >= 0; }
bool UvcControls::getSharpnessRange(uint16_t& min, uint16_t& max) {
    return uvc_get_sharpness(m_devh, &min, UVC_GET_MIN) >= 0 && uvc_get_sharpness(m_devh, &max, UVC_GET_MAX) >= 0;
}

// ── Gamma ──
bool UvcControls::getGamma(uint16_t& val)            { return uvc_get_gamma(m_devh, &val, UVC_GET_CUR) >= 0; }
bool UvcControls::setGamma(uint16_t val)              { return uvc_set_gamma(m_devh, val) >= 0; }
bool UvcControls::getGammaRange(uint16_t& min, uint16_t& max) {
    return uvc_get_gamma(m_devh, &min, UVC_GET_MIN) >= 0 && uvc_get_gamma(m_devh, &max, UVC_GET_MAX) >= 0;
}

// ── White Balance ──
bool UvcControls::getWhiteBalanceTemp(uint16_t& val) { return uvc_get_white_balance_temperature(m_devh, &val, UVC_GET_CUR) >= 0; }
bool UvcControls::setWhiteBalanceTemp(uint16_t val)   { return uvc_set_white_balance_temperature(m_devh, val) >= 0; }
bool UvcControls::getWhiteBalanceTempRange(uint16_t& min, uint16_t& max) {
    return uvc_get_white_balance_temperature(m_devh, &min, UVC_GET_MIN) >= 0
        && uvc_get_white_balance_temperature(m_devh, &max, UVC_GET_MAX) >= 0;
}
bool UvcControls::getWhiteBalanceTempAuto(uint8_t& val)  { return uvc_get_white_balance_temperature_auto(m_devh, &val, UVC_GET_CUR) >= 0; }
bool UvcControls::setWhiteBalanceTempAuto(uint8_t val)    { return uvc_set_white_balance_temperature_auto(m_devh, val) >= 0; }
bool UvcControls::getWhiteBalanceComponent(uint16_t& b, uint16_t& r) { return uvc_get_white_balance_component(m_devh, &b, &r, UVC_GET_CUR) >= 0; }
bool UvcControls::setWhiteBalanceComponent(uint16_t b, uint16_t r)   { return uvc_set_white_balance_component(m_devh, b, r) >= 0; }
bool UvcControls::getWhiteBalanceComponentAuto(uint8_t& val) { return uvc_get_white_balance_component_auto(m_devh, &val, UVC_GET_CUR) >= 0; }
bool UvcControls::setWhiteBalanceComponentAuto(uint8_t val)   { return uvc_set_white_balance_component_auto(m_devh, val) >= 0; }

// ── Exposure ──
bool UvcControls::getExposureAbs(uint32_t& val)      { return uvc_get_exposure_abs(m_devh, &val, UVC_GET_CUR) >= 0; }
bool UvcControls::setExposureAbs(uint32_t val)        { return uvc_set_exposure_abs(m_devh, val) >= 0; }
bool UvcControls::getExposureAbsRange(uint32_t& min, uint32_t& max) {
    return uvc_get_exposure_abs(m_devh, &min, UVC_GET_MIN) >= 0 && uvc_get_exposure_abs(m_devh, &max, UVC_GET_MAX) >= 0;
}
bool UvcControls::getAeMode(uint8_t& val)             { return uvc_get_ae_mode(m_devh, &val, UVC_GET_CUR) >= 0; }
bool UvcControls::setAeMode(uint8_t val)               { return uvc_set_ae_mode(m_devh, val) >= 0; }
bool UvcControls::getAePriority(uint8_t& val)          { return uvc_get_ae_priority(m_devh, &val, UVC_GET_CUR) >= 0; }
bool UvcControls::setAePriority(uint8_t val)            { return uvc_set_ae_priority(m_devh, val) >= 0; }

// ── Focus ──
bool UvcControls::getFocusAbs(uint16_t& val)          { return uvc_get_focus_abs(m_devh, &val, UVC_GET_CUR) >= 0; }
bool UvcControls::setFocusAbs(uint16_t val)            { return uvc_set_focus_abs(m_devh, val) >= 0; }
bool UvcControls::getFocusAbsRange(uint16_t& min, uint16_t& max) {
    return uvc_get_focus_abs(m_devh, &min, UVC_GET_MIN) >= 0 && uvc_get_focus_abs(m_devh, &max, UVC_GET_MAX) >= 0;
}
bool UvcControls::getFocusAuto(uint8_t& val)           { return uvc_get_focus_auto(m_devh, &val, UVC_GET_CUR) >= 0; }
bool UvcControls::setFocusAuto(uint8_t val)             { return uvc_set_focus_auto(m_devh, val) >= 0; }

// ── Zoom ──
bool UvcControls::getZoomAbs(uint16_t& val)            { return uvc_get_zoom_abs(m_devh, &val, UVC_GET_CUR) >= 0; }
bool UvcControls::setZoomAbs(uint16_t val)              { return uvc_set_zoom_abs(m_devh, val) >= 0; }
bool UvcControls::getZoomAbsRange(uint16_t& min, uint16_t& max) {
    return uvc_get_zoom_abs(m_devh, &min, UVC_GET_MIN) >= 0 && uvc_get_zoom_abs(m_devh, &max, UVC_GET_MAX) >= 0;
}

// ── Pan/Tilt ──
bool UvcControls::getPanTiltAbs(int32_t& pan, int32_t& tilt) { return uvc_get_pantilt_abs(m_devh, &pan, &tilt, UVC_GET_CUR) >= 0; }
bool UvcControls::setPanTiltAbs(int32_t pan, int32_t tilt)    { return uvc_set_pantilt_abs(m_devh, pan, tilt) >= 0; }
bool UvcControls::getPanTiltAbsRange(int32_t& pMin, int32_t& pMax, int32_t& tMin, int32_t& tMax) {
    return uvc_get_pantilt_abs(m_devh, &pMin, &tMin, UVC_GET_MIN) >= 0
        && uvc_get_pantilt_abs(m_devh, &pMax, &tMax, UVC_GET_MAX) >= 0;
}

// ── Backlight ──
bool UvcControls::getBacklightComp(uint16_t& val)     { return uvc_get_backlight_compensation(m_devh, &val, UVC_GET_CUR) >= 0; }
bool UvcControls::setBacklightComp(uint16_t val)       { return uvc_set_backlight_compensation(m_devh, val) >= 0; }
bool UvcControls::getBacklightCompRange(uint16_t& min, uint16_t& max) {
    return uvc_get_backlight_compensation(m_devh, &min, UVC_GET_MIN) >= 0
        && uvc_get_backlight_compensation(m_devh, &max, UVC_GET_MAX) >= 0;
}

// ── Power Line Frequency ──
bool UvcControls::getPowerLineFreq(uint8_t& val)      { return uvc_get_power_line_frequency(m_devh, &val, UVC_GET_CUR) >= 0; }
bool UvcControls::setPowerLineFreq(uint8_t val)        { return uvc_set_power_line_frequency(m_devh, val) >= 0; }

// ── Hue ──
bool UvcControls::getHue(int16_t& val)                { return uvc_get_hue(m_devh, &val, UVC_GET_CUR) >= 0; }
bool UvcControls::setHue(int16_t val)                  { return uvc_set_hue(m_devh, val) >= 0; }
bool UvcControls::getHueRange(int16_t& min, int16_t& max) {
    return uvc_get_hue(m_devh, &min, UVC_GET_MIN) >= 0 && uvc_get_hue(m_devh, &max, UVC_GET_MAX) >= 0;
}
bool UvcControls::getHueAuto(uint8_t& val)             { return uvc_get_hue_auto(m_devh, &val, UVC_GET_CUR) >= 0; }
bool UvcControls::setHueAuto(uint8_t val)               { return uvc_set_hue_auto(m_devh, val) >= 0; }

// ── Scanning Mode ──
bool UvcControls::getScanningMode(uint8_t& val)       { return uvc_get_scanning_mode(m_devh, &val, UVC_GET_CUR) >= 0; }
bool UvcControls::setScanningMode(uint8_t val)         { return uvc_set_scanning_mode(m_devh, val) >= 0; }

// ── Iris ──
bool UvcControls::getIrisAbs(uint16_t& val)           { return uvc_get_iris_abs(m_devh, &val, UVC_GET_CUR) >= 0; }
bool UvcControls::setIrisAbs(uint16_t val)             { return uvc_set_iris_abs(m_devh, val) >= 0; }
bool UvcControls::getIrisAbsRange(uint16_t& min, uint16_t& max) {
    return uvc_get_iris_abs(m_devh, &min, UVC_GET_MIN) >= 0 && uvc_get_iris_abs(m_devh, &max, UVC_GET_MAX) >= 0;
}

// ── Digital Multiplier ──
bool UvcControls::getDigitalMultiplier(uint16_t& val) { return uvc_get_digital_multiplier(m_devh, &val, UVC_GET_CUR) >= 0; }
bool UvcControls::setDigitalMultiplier(uint16_t val)   { return uvc_set_digital_multiplier(m_devh, val) >= 0; }

// ── Privacy ──
bool UvcControls::getPrivacy(uint8_t& val)             { return uvc_get_privacy(m_devh, &val, UVC_GET_CUR) >= 0; }
bool UvcControls::setPrivacy(uint8_t val)               { return uvc_set_privacy(m_devh, val) >= 0; }

// ── Power Mode ──
bool UvcControls::getPowerMode(uint8_t& mode)          { return uvc_get_power_mode(m_devh, (uvc_device_power_mode*)&mode, UVC_GET_CUR) >= 0; }
bool UvcControls::setPowerMode(uint8_t mode)            { return uvc_set_power_mode(m_devh, (uvc_device_power_mode)mode) >= 0; }
