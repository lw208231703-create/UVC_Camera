import math

# 硬件常量
PIXEL_CLOCK_HZ = 74250000  # 74.25 MHz
H_MAX = 1736
V_MAX = 2140

# 预计算行周期 (单位: 微秒 us)
LINE_PERIOD_US = (H_MAX / PIXEL_CLOCK_HZ) * 1e6  # 结果约为 23.3744 us

def convert_us_to_lines(exposure_us):
    """
    将输入的绝对曝光时间(us)转化为行曝光值
    """
    # 1. 换算为浮点行数
    calc_lines = exposure_us / LINE_PERIOD_US
    
    # 2. 向上取整 (确保曝光量充足)
    # 使用 math.ceil，如果正好是整数则不变
    target_lines = math.ceil(calc_lines)
    
    # 3. 边界限制检查
    # 最小限制
    if target_lines < 1:
        target_lines = 1
        print(f"警告: 输入时间过短，已强制设为最小 1 行")
        
    # 最大限制 (假设要保持 20Hz 帧率)
    max_lines = V_MAX
    if target_lines > max_lines:
        target_lines = max_lines
        print(f"警告: 输入时间超过帧周期，已强制设为最大 {max_lines} 行 (帧率可能下降)")
        
    # 4. (可选) 反算实际执行的绝对时间，方便日志打印或回显给用户
    actual_exposure_us = target_lines * LINE_PERIOD_US
    
    return target_lines, actual_exposure_us

# --- 测试用例 ---
test_values_us = [10, 5000, 10000, 50000, 60000]

for us in test_values_us:
    lines, actual_us = convert_us_to_lines(us)
    print(f"输入: {us:>5} μs -> 设置行数: {lines:>4} 行 -> 实际时间: {actual_us:.2f} μs")
