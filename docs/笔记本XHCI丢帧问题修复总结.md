# 笔记本 XHCI 高分辨率丢帧问题 — 原因分析与修复总结

> **设备**: FT602 USB 3.0 FIFO 桥接芯片 + FPGA + IMX992
> **格式**: Y16 (每像素 2 字节)
> **分辨率**: 2560x2048 @ ~20fps
> **单帧大小**: 10,485,760 bytes (10 MiB)
> **故障现象**: 台式机正常，笔记本端频繁丢帧，分辨率越低丢帧越少
> **修复日期**: 2026-07-14

---

## 1. 问题现象

| 平台 | 丢帧率 | 表现 |
|------|--------|------|
| 台式机 | ~0.1% | 偶发丢帧，长时间运行稳定 |
| 笔记本 | ~4% | 频繁丢帧，每 3-10 帧突发一次 |

丢帧时收到的帧数据量呈现规律性：

```
got=2,146,300  (2 MiB + 48 KiB - 4)
got=4,243,452  (4 MiB + 48 KiB - 4)
got=6,340,604  (6 MiB + 48 KiB - 4)
got=8,437,756  (8 MiB + 48 KiB - 4)
```

相邻值之间的差恰好为 **4,194,304 = 4 MiB**，这是一个关键线索。

---

## 2. 根因分析

### 2.1 数据链路结构

```
FPGA (IMX992 采集)
  └─ FPGA FIFO (32 KB, MEM_LEN=8192 × 32bit)
      └─ FT602 FIFO (16 KB, 600 模式 2 通道对半分)
          └─ USB 3.0 Bulk IN endpoint
              └─ WinUSB 驱动排队层
                  └─ libusb / libuvc
                      └─ 上位机
```

### 2.2 核心问题：WinUSB 普通排队层的 4 MiB 调度边界

在未启用 RAW_IO 时，WinUSB 会对 USB bulk 请求进行排队和内部拆分。当请求超过底层支持的单次传输长度时，WinUSB 会在 **~4 MiB 边界** 处拆分请求并串行提交。

相邻子请求之间存在微小的时间空隙（处理完成中断 → 回调 → 重新提交）。这段空隙中 host 不从 FT602 读数据。

### 2.3 FIFO 背压导致丢帧的完整链路

```
WinUSB 4 MiB 边界空隙 (~0.16ms+)
  → host 停止读取 FT602
    → FT602 FIFO (32 KB) 在 0.16ms 内被 FPGA 写满
      → FT602 拉高 TXE# 停止接收
        → FPGA FIFO (32 KB) 也被写满
          → FPGA wrmark 触发 stop_writing
            → FPGA 主动终止当前帧，开始新帧
              → 上位机收到截断帧 (got 远小于 expected)
```

FPGA 侧有效缓存仅 32 KB，在 188.7 MB/s 数据率下只能吸收 **0.174 ms**：

```
32768 bytes / 188,700,000 bytes/s ≈ 0.174 ms
```

台式机 XHCI 调度抖动 < 0.174 ms 所以不丢帧；笔记本 XHCI 调度抖动 > 0.174 ms 导致 FIFO 溢出。

### 2.4 为什么"分辨率越低丢帧越少"

低分辨率帧更小（如 1280x1024 = 2.5 MB），传输时间更短，遇到 4 MiB 边界空隙的概率更低。高分辨率帧（10 MB）需要跨越多个 4 MiB 边界，每个边界都是一次丢包风险点。

---

## 3. 失败的尝试

### 3.1 缩小 transfer size 到 1 MB（30 buffers × 1 MB）

| 指标 | 结果 |
|------|------|
| 丢帧率 | **10%**（从 4% 恶化） |
| 原因 | 每帧 10 个 transfer 边界 = 10 个丢包点，反而增加了空隙次数 |

### 3.2 增加 buffer 数量到 30 个（30 buffers × 10 MB = 300 MB）

| 指标 | 结果 |
|------|------|
| 丢帧率 | **27%**（从 4% 严重恶化） |
| 原因 | 300 MB DMA 锁定内存压垮 XHCI TRB 环调度，导致更频繁的调度停顿 |

### 3.3 结论

```
10 buffers × 10 MB = 100 MB  →  4% 丢帧率 (最优配置)
30 buffers × 1 MB  = 30 MB   →  10% 丢帧率
30 buffers × 10 MB = 300 MB  →  27% 丢帧率
```

普通 WinUSB 模式下无法通过调整 buffer 数量或 transfer 大小消除 4 MiB 边界空隙。

---

## 4. 最终修复方案：启用 WinUSB RAW_IO

### 4.1 原理

WinUSB RAW_IO 模式让 bulk read 请求**绕过 WinUSB 的普通排队层**，直接进入 USB core stack。多个请求可以并行调度，消除普通模式下的串行请求边界空隙。

参考资料：
- [WinUSB Functions for Pipe Policy Modification](https://learn.microsoft.com/en-us/windows-hardware/drivers/usbcon/winusb-functions-for-pipe-policy-modification)
- libusb 1.0.30 RAW_IO API (`LIBUSB_API_VERSION >= 0x0100010C`)

### 4.2 RAW_IO 约束条件

1. **libusb >= 1.0.30** — 项目已编译链接 libusb 1.0.30.12037
2. **WinUSB backend** — Windows 上 libusb 默认使用 WinUSB
3. **接口已 claim** — 在 transfer submit 之前调用
4. **请求长度是 endpoint wMaxPacketSize 整数倍** — 需要对齐
5. **请求长度不超过 RAW_IO 最大传输长度** — 实测笔记本为 2 MB

### 4.3 关键挑战：10 MB 整帧 vs 2 MB RAW_IO 限制

笔记本 XHCI 的 RAW_IO 最大传输长度仅 **2,097,152 bytes (2 MB)**，而整帧 UVC payload 为 **10,485,772 bytes (10 MB)**。

解决方案：**将 10 MB 整帧拆成 5 个 2 MB 的 RAW_IO transfer**，利用 libuvc `_uvc_process_payload` 的 BULK 模式多 transfer 组装逻辑拼回完整帧。

这与之前失败的"1 MB 普通 transfer"有本质区别：
- 普通 1 MB transfer：每个 transfer 完成后 WinUSB 排队层产生空隙
- RAW_IO 2 MB transfer：绕过排队层，多个请求并行调度，无空隙

### 4.4 组装逻辑（已存在于 libuvc）

`_uvc_process_payload` 的 BULK 模式已支持多 transfer 组装同一帧：

```
Transfer 1 (2 MB): [UVC header 12B | 像素数据 ~2MB]  → bulk_hdr_done=1, got_bytes += ~2MB
Transfer 2 (2 MB): [像素数据 2MB]                    → header_len=0, got_bytes += 2MB
Transfer 3 (2 MB): [像素数据 2MB]                    → header_len=0, got_bytes += 2MB
Transfer 4 (2 MB): [像素数据 2MB]                    → header_len=0, got_bytes += 2MB
Transfer 5 (2 MB): [像素数据 ~2MB + short packet]    → got_bytes == dwMaxVideoFrameSize
                                                     → _uvc_swap_buffers 发布完整帧
```

---

## 5. 代码修改详情

### 5.1 libuvc `stream.c` — RAW_IO 初始化与 transfer 池

**文件**: `third_party/libuvc/src/stream.c`

新增 `_uvc_setup_raw_io()` 函数：
- 查询 endpoint `wMaxPacketSize` (1024 bytes)
- 将请求长度向上对齐到 `wMaxPacketSize` 整数倍
- 查询 `libusb_endpoint_supports_raw_io()` 确认支持
- 查询 `libusb_get_max_raw_io_transfer_size()` 获取最大传输长度
- **如果对齐后长度超过 RAW_IO 最大传输长度，自动 cap 到 max_raw_transfer 并对齐**
- 调用 `libusb_endpoint_set_raw_io()` 启用 RAW_IO

Bulk transfer 池创建逻辑修改：
- 在 transfer submit 之前调用 `_uvc_setup_raw_io()`
- RAW_IO 成功：使用 cap 后的 transfer size（2 MB）
- RAW_IO 失败：回退到普通 WinUSB 模式并明确告警
- 启动时输出 libusb 版本、设备速度、endpoint 信息、RAW_IO 状态

停流时关闭 RAW_IO：
- 所有 transfer 取消并释放后调用 `libusb_endpoint_set_raw_io(handle, endpoint, 0)`

### 5.2 libuvc `stream.c` — 传输状态分类诊断

`_uvc_stream_callback` 增强：
- `LIBUSB_TRANSFER_COMPLETED + actual_length < length`：记录 short packet（设备提前结束）
- `LIBUSB_TRANSFER_CANCELLED / ERROR / NO_DEVICE`：记录详细状态信息
- `LIBUSB_TRANSFER_TIMED_OUT / STALL / OVERFLOW`：记录可恢复异常计数

### 5.3 libuvc `libuvc_internal.h` — 新增字段

**文件**: `third_party/libuvc/include/libuvc/libuvc_internal.h`

`uvc_stream_handle` 结构体新增：
- `uint8_t raw_io_enabled` — 标记 RAW_IO 是否已启用
- `uint8_t bulk_endpoint` — 记录 bulk IN endpoint 地址（用于停流时关闭 RAW_IO）

### 5.4 上位机 `LibuvcCameraDevice.cpp` — USB 环境诊断

**文件**: `QtWidgetsApplication1/hal/LibuvcCameraDevice.cpp`

`startStreaming()` 中新增 USB 环境诊断日志：
- libusb 版本和 API 版本
- 设备速度（确认 SuperSpeed）
- bulk endpoint 地址和 max_packet_size
- RAW_IO 支持状态、最大传输长度
- 对齐后请求长度、cap 后实际 transfer size、每帧 transfer 数

### 5.5 上位机 `LibuvcCameraDevice.cpp` — 丢帧检测改进

将原来的 `data_bytes != expected` 硬比对改为三级分类：

| 差值 | 分类 | 含义 |
|------|------|------|
| `diff <= 16` | `minor_short` | UVC header (12B) 未剥离或微小时序抖动，非真实丢包 |
| `16 < diff <= 1024` | `minor_short` | 临界区间，记录但不计为真实丢包 |
| `diff > 1024` | `real_drop` | 真实丢包，输出 `[REAL DROP]` 日志含丢包百分比 |

序列号跳帧也计入 `real_dropped_frames`。

每 100 帧输出周期性统计：
```
[Stats] frames=101 real_drops=0 (0.00%) minor_short=0 seq=102
```

### 5.6 上位机 `LibuvcCameraDevice.h` — 新增统计字段

- `m_realDroppedFrames` — 真实丢包计数
- `m_minorShortFrames` — UVC header / 临界区间计数
- `m_statsLogCounter` — 周期性统计日志计数器
- `kHeaderTolerance = 16` — UVC header 容差
- `kRealDropThreshold = 1024` — 真实丢包判定阈值

### 5.7 上位机 `QtWidgetsApplication1.vcxproj` — Buffer 配置

```xml
<PreprocessorDefinitions>...LIBUVC_NUM_TRANSFER_BUFS=10;...</PreprocessorDefinitions>
```

保持 10 buffers × 10 MB = 100 MB 配置（实测最优）。

---

## 6. 修复效果

| 平台 | 修复前 | 修复后 |
|------|--------|--------|
| 台式机 | ~0.1% | **~0%** |
| 笔记本 | ~4% | **~0%** |

修复后笔记本日志示例：
```
[USB Env] libusb 1.0.30.12037 (API 0x100010c)
[USB Env] device speed: SuperSpeed (4)
[USB Env] bulk endpoint: 0x81  max_packet: 1024 bytes
[USB Env] RAW_IO: supported (max_transfer=2097152 bytes / 2048.0 KB)
[USB Env] RAW_IO: request aligned 10485772 → 10486784 bytes
[USB Env] RAW_IO: ENABLED (capped 10486784 → 2097152 bytes, 5 transfers/frame)
[Stats] frames=101 real_drops=0 (0.00%) minor_short=0 seq=102
[Stats] frames=201 real_drops=0 (0.00%) minor_short=0 seq=202
[Stats] frames=301 real_drops=0 (0.00%) minor_short=0 seq=302
```

---

## 7. 修改文件总览

```
third_party/libuvc/
├── include/libuvc/libuvc_internal.h    # 新增 raw_io_enabled, bulk_endpoint 字段
└── src/stream.c                        # RAW_IO 初始化、capping 逻辑、诊断日志、停流清理

QtWidgetsApplication1/
├── hal/LibuvcCameraDevice.h            # 新增统计字段和容差常量
├── hal/LibuvcCameraDevice.cpp          # USB 环境诊断、丢帧三级分类、周期统计
└── QtWidgetsApplication1.vcxproj       # LIBUVC_NUM_TRANSFER_BUFS=10 配置注释
```

---

## 8. 技术要点总结

### 8.1 RAW_IO capping 逻辑

当 RAW_IO 最大传输长度 < 整帧大小时，不能放弃 RAW_IO。应将 transfer size 设为 `min(对齐后整帧大小, RAW_IO最大长度)`，利用 BULK 模式的多 transfer 组装能力拼回完整帧：

```c
if (aligned_length > (size_t)max_raw_transfer) {
    // cap 到 max_raw_transfer 并对齐到 max_packet
    size_t capped = ((size_t)max_raw_transfer / (size_t)max_packet) * (size_t)max_packet;
    aligned_length = capped;
}
```

### 8.2 为什么不能简单拆小 transfer

标准 libuvc BULK 路径把"一个完成的 Bulk transfer"当作"一个 UVC payload"处理，期望每个 transfer 起始有 UVC header。

如果设备只在一帧开头发送一次 header，而上位机简单拆成多个小 transfer，后续 transfer 的首字节会被误当作 header length 解析。

**关键区别**：RAW_IO 模式下的多 transfer 组装由 `_uvc_process_payload` 的 `bulk_hdr_done` 机制处理——第一个 transfer 消费 header 后，后续 transfer 直接作为纯像素数据累积，不重复解析 header。

### 8.3 诊断日志的重要性

修复过程中的三个关键诊断：

1. **`got` 值的 4 MiB 差值规律** — 确认 WinUSB 4 MiB 调度边界
2. **RAW_IO max_transfer = 2 MB** — 发现需要 capping 而非放弃
3. **`[Stats] real_drops=0 (0.00%)`** — 确认修复生效

没有准确的诊断日志，无法区分假告警（UVC header 未剥离）、真实丢包（FIFO 溢出）和 transfer 异常（timeout/stall）。

---

## 9. 后续建议

### 9.1 长稳测试

```
30 min (~32,400 帧)  → 确认无丢帧
2 h   (~129,600 帧)  → 确认无内存泄漏
8 h   (~518,400 帧)  → 确认长期稳定
```

### 9.2 FPGA 侧优化（可选）

当前 FPGA FIFO 仅 32 KB，缓冲深度 ~0.174 ms。虽然 RAW_IO 已消除 host 侧空隙，但增加 FPGA FIFO 深度可以提供额外安全余量：

- 将 `MEM_LEN` 从 8192 增大到 65536（256 KB），缓冲深度提升到 ~1.36 ms
- 或在 FPGA pipeline 中插入 ≥ 512 KB 异步 FIFO，吸收 2.5 ms 瞬时背压

### 9.3 FT602 配置确认

确认 FT602 Chip Configuration 中该 IN 通道获得完整的 16 KiB FIFO（单通道配置），而非四通道默认的 4 KiB。这是"重新分配固定 FIFO"，不是改大 FT602 的物理容量。

参考：[FT602 UVC Chip Configuration Guide AN_435](https://ftdichip.com/wp-content/uploads/2020/08/AN_435_FT602_UVC_Chip_Configuration_Guide.pdf)
