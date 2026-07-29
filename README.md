# UVC Camera

基于 libuvc 的 USB 3.0 UVC 相机上位机，支持 FT602Q 桥接芯片，提供视频预览、参数控制、图像抓拍与连拍等功能。

## 硬件平台

- **桥接芯片**: FTDI FT602Q (USB 3.0 to UVC)
- **传感器接口**: I²C (通过 FT602 直通)
- **视频格式**: Y16 / Y800 / YUYV / MJPEG
- **分辨率**: 最大 2560×2048（取决于传感器）

## 功能

- 多通道 UVC 设备枚举与切换
- 实时视频预览（16-bit 灰度/彩色）
- 图像参数控制（增益、曝光、帧率、像素格式、ROI、AE 模式）
- 16-bit 显示位深调节（bits 截断选择）
- 单张抓拍（TIFF，保留原始位深）
- 连拍（独立线程 + 无界队列，不丢帧）
- 触发模式切换（固定帧频 / 外触发）
- I²C 寄存器调试面板
- 一键 WinUSB 驱动安装（集成 libwdi）
- 探测器温度读取
- I²C 参数读写（独立线程）
- 多语言支持（中文/英文）
- 降噪（中值滤波）

## 依赖

| 库 | 许可证 | 用途 |
|---|---|---|
| Qt 6.12 | LGPL-3.0 | GUI 框架 |
| libusb 1.0 | LGPL-2.1 | USB 通信 |
| libuvc 0.0.7 | BSD 3-Clause | UVC 协议 |
| libwdi | LGPL-3.0 | 驱动安装 |
| libusbK 3.1.0 | LGPL-2.1 | 备选 USB 驱动 |
| libusb-win32 1.4.0.2 | LGPL-3.0 | 备选 USB 驱动 |
| OpenCV 4.14 | Apache 2.0 | 图像处理 |
| spdlog | MIT | 日志 |
| nlohmann/json | MIT | JSON 解析 |

## 构建

### 环境要求

- Visual Studio 2022
- Qt 6.12.0 for MSVC 2022
- Windows SDK 10.0.26100.0+
- CMake (可选)

### 编译步骤

1. 克隆仓库并初始化子模块：
   ```
   git clone https://github.com/lw208231703-create/UVC_Camera.git
   cd UVC_Camera
   git submodule update --init --recursive
   ```

2. 用 Visual Studio 打开 `QtWidgetsApplication1/QtWidgetsApplication1.sln`

3. 选择 Release x64 配置，编译运行

4. 首次使用请先安装 WinUSB 驱动（点击相机打开失败弹窗中的「安装驱动」按钮）

## 驱动安装

首次连接 FT602Q 设备时，需要将 USB 驱动替换为 WinUSB，libuvc 才能正常工作。

程序内置驱动安装功能（基于 libwdi）：
- 相机打开失败时自动检测驱动状态
- 点击「安装驱动」按钮，自动为设备所有接口安装 WinUSB
- 安装过程中会触发 UAC 提权

## 许可证

GNU General Public License v3.0 (GPL-3.0)
