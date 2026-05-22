#pragma once

#include <QString>
#include <unordered_map>

// ── Language enum ──
enum class UiLang { EN, ZH };
inline UiLang g_uiLang = UiLang::EN;

inline void setUiLanguage(UiLang lang) { g_uiLang = lang; }

// ── String lookup ──
// Keys are English strings; values are the translation.
// To add a new language, add a new map below and extend the tr() function.

inline QString uiTr(const char* key) {
    if (g_uiLang == UiLang::EN) return QString::fromUtf8(key);

    static const std::unordered_map<std::string, const char*> zh = {
        // ── Menu ──
        {"&File",              "文件(&F)"},
        {"&Load Config...",    "加载配置(&L)..."},
        {"&Save Config...",    "保存配置(&S)..."},
        {"E&xit",              "退出(&X)"},
        {"&View",              "视图(&V)"},
        {"&Fit to Window",     "适应窗口(&F)"},
        {"&Capture",           "采集(&C)"},
        {"&Snapshot",          "抓拍(&S)"},
        {"&Help",              "帮助(&H)"},
        {"&About",             "关于(&A)"},

        // ── Status bar ──
        {"FPS: --",            "帧率：--"},
        {"Device: Disconnected","设备：未连接"},
        {"Rx: -- MB/s",        "接收：-- MB/s"},
        {"Dropped: 0",         "丢帧：0"},
        {"Device: %1",         "设备：%1"},

        // ── Control panels ──
        {"Device Connection",   "设备连接"},
        {"Refresh",             "刷新"},
        {"Open Camera",         "打开相机"},
        {"Close Camera",        "关闭相机"},
        {"No device selected",  "未选择设备"},
        {"VID:%1 PID:%2\n%3",   "VID:%1 PID:%2\n%3"},
        {"Format & Stream",     "格式与推流"},
        {"Format:",             "格式："},
        {"Resolution:",         "分辨率："},
        {"Apply & Start Streaming","应用并开始推流"},
        {"Stop Streaming",      "停止推流"},
        {"Image Processing (OpenCV)","图像处理 (OpenCV)"},
        {"Gaussian Blur",       "高斯滤波"},
        {"Histogram Equalization","直方图均衡化"},
        {"Capture",             "采集"},
        {"Snapshot",            "抓拍"},
        {"Record Sequence",     "连续录制"},
        {"Stop Recording",      "停止录制"},
        {"System Log",          "系统日志"},
        {"-- Select --",        "-- 请选择 --"},
        {"-- No UVC devices found --","-- 未发现 UVC 设备 --"},

        // ── Camera settings groups ──
        {"Image Adjustments",   "图像调节"},
        {"Brightness",          "亮度"},
        {"Contrast",            "对比度"},
        {"Contrast Auto",       "对比度自动"},
        {"Gain",                "增益"},
        {"Saturation",          "饱和度"},
        {"Sharpness",           "锐度"},
        {"Gamma",               "伽马"},
        {"Hue",                 "色调"},
        {"Hue Auto",            "色调自动"},
        {"White Balance",       "白平衡"},
        {"Temperature",         "色温"},
        {"Auto White Balance",  "自动白平衡"},
        {"Exposure",            "曝光"},
        {"Exp. Time",           "曝光时间"},
        {"AE Mode",             "自动曝光模式"},
        {"Manual",              "手动"},
        {"Auto",                "自动"},
        {"Shutter Priority",    "快门优先"},
        {"Aperture Priority",   "光圈优先"},
        {"AE Priority",         "曝光优先"},
        {"Focus",               "对焦"},
        {"Auto Focus",          "自动对焦"},
        {"Lens",                "镜头"},
        {"Zoom",                "变焦"},
        {"Iris",                "光圈"},
        {"Pan",                 "水平"},
        {"Tilt",                "垂直"},
        {"Other",               "其他"},
        {"Backlight",           "背光补偿"},
        {"Power Line",          "电源频率"},
        {"Disabled",            "禁用"},
        {"50 Hz",               "50 Hz"},
        {"60 Hz",               "60 Hz"},

        // ── Viewport ──
        {"Device Offline",      "设备离线"},

        // ── Error dialogs ──
        {"Error",               "错误"},
        {"Device Lost",         "设备丢失"},
        {"Invalid device selection","无效的设备选择"},
        {"Failed to start streaming.","启动推流失败。"},
        {"Failed to initialize libuvc.\nCheck USB driver installation.",
         "libuvc 初始化失败。\n请检查 USB 驱动安装。"},
        {"Failed to open camera device.\n\n%1"
         "\n\nOn Windows, libuvc requires a WinUSB/libusbK driver.\n"
         "Use Zadig (https://zadig.akeo.ie) to replace the driver.",
         "打开摄像头设备失败。\n\n%1"
         "\n\n在 Windows 上, libuvc 需要 WinUSB/libusbK 驱动。\n"
         "请使用 Zadig (https://zadig.akeo.ie) 替换驱动。"},
        {"The camera device was disconnected.\nAll controls have been released.",
         "摄像头设备已断开。\n所有控制已释放。"},
    };

    auto it = zh.find(key);
    if (it != zh.end()) return QString::fromUtf8(it->second);
    return QString::fromUtf8(key); // fallback: return key as-is
}

// ── Macro for convenience ──
#define TR(key) uiTr(key)
#define TR_ARG(key, arg) tr(key).arg(arg)
