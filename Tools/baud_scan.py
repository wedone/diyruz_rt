#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
baud_scan.py - 扫描所有常见波特率，捕获 CC2530 UART 输出

用法:
    python baud_scan.py --port COM4

工作流程:
    1. 按顺序尝试每个波特率
    2. 每个波特率监听 3 秒
    3. 如果收到任何数据，立即输出并停止扫描
    4. 如果没数据，切换到下一个波特率
    5. 完成一轮后等待用户按 RESET，再开始下一轮

提示: 扫描期间随时按 RESET 触发固件输出
"""

import sys
import time
import argparse
import serial

# CC2530 常见波特率（按可能性排序）
BAUD_RATES = [
    115200,  # 当前固件配置
    38400,   # Z-Stack 默认
    9600,    # 低速默认
    57600,
    19200,
    4800,
    2400,
    76800,   # 误配置可能值
    230400,
    460800,
]


def scan_baud(port, rounds=3, duration_per_baud=2.0):
    """扫描所有波特率"""

    print("=" * 60)
    print("波特率扫描工具")
    print("=" * 60)
    print(f"端口: {port}")
    print(f"每波特率监听: {duration_per_baud} 秒")
    print(f"扫描轮数: {rounds}")
    print(f"波特率列表: {BAUD_RATES}")
    print()
    print("提示: 扫描期间随时按 RESET 触发固件输出")
    print("=" * 60)

    for round_num in range(1, rounds + 1):
        print(f"\n===== 第 {round_num}/{rounds} 轮扫描 =====")
        print(">>> 请按 CC2530 的 RESET <<<")
        time.sleep(2)
        print("开始扫描...")

        for baud in BAUD_RATES:
            try:
                print(f"\n[{time.strftime('%H:%M:%S')}] 尝试 {baud} bps...", end=" ", flush=True)

                ser = serial.Serial(
                    port=port,
                    baudrate=baud,
                    bytesize=serial.EIGHTBITS,
                    parity=serial.PARITY_NONE,
                    stopbits=serial.STOPBITS_ONE,
                    timeout=0.1,  # 100ms 读超时
                )

                # 清空缓冲区
                ser.reset_input_buffer()

                # 监听 duration_per_baud 秒
                received = bytearray()
                start_time = time.time()
                while time.time() - start_time < duration_per_baud:
                    n = ser.in_waiting
                    if n > 0:
                        data = ser.read(n)
                        received.extend(data)
                        # 收到数据立即继续读 0.5 秒，看是否还有更多
                        end_time = time.time() + 0.5
                        while time.time() < end_time:
                            n = ser.in_waiting
                            if n > 0:
                                received.extend(ser.read(n))
                            time.sleep(0.05)
                        break
                    time.sleep(0.05)

                ser.close()

                if len(received) > 0:
                    print(f"收到 {len(received)} 字节!")
                    print()
                    print("=" * 60)
                    print(f"!!! 找到数据 !!! 波特率: {baud} bps")
                    print("=" * 60)
                    print()
                    print("原始数据 (hex):")
                    print(" ".join(f"{b:02x}" for b in received[:128]))
                    if len(received) > 128:
                        print(f"... (共 {len(received)} 字节)")
                    print()
                    print("ASCII 解读:")
                    try:
                        text = received.decode('ascii', errors='replace')
                        print(repr(text[:200]))
                    except Exception:
                        print("(无法解码为 ASCII)")
                    print()
                    print("可打印字符:")
                    printable = ''.join(
                        chr(b) if 32 <= b < 127 else '.'
                        for b in received[:128]
                    )
                    print(printable)
                    print()
                    print("=" * 60)
                    print(f"建议: 用 {baud} bps 监听")
                    print("=" * 60)
                    return baud
                else:
                    print("无数据")

            except Exception as e:
                print(f"错误: {e}")
                continue

        print(f"\n第 {round_num} 轮扫描完成，未找到数据")

    print("\n" + "=" * 60)
    print("所有波特率扫描完成，均未收到数据")
    print("=" * 60)
    print()
    print("可能原因:")
    print("1. 接线问题: USB-TTL 的 RX 未接到 CC2530 的 P0_3 (TX)")
    print("2. GND 未连接")
    print("3. CC2530 未正常运行")
    print("4. 固件未烧录成功")
    print("5. P0_3 被其他功能占用（如 LED3）")
    print()
    print("建议排查:")
    print("- 检查接线: USB-TTL.RX -> CC2530.P0_3, USB-TTL.GND -> CC2530.GND")
    print("- 用万用表测量 P0_3 是否有 3.3V 高电平（空闲时应为高）")
    print("- 确认固件中 DIY_DEBUG_UART 宏已启用")
    return None


def main():
    parser = argparse.ArgumentParser(description='CC2530 波特率扫描工具')
    parser.add_argument('--port', required=True, help='串口端口 (如 COM4)')
    parser.add_argument('--rounds', type=int, default=3, help='扫描轮数 (默认 3)')
    parser.add_argument('--duration', type=float, default=2.0,
                        help='每波特率监听秒数 (默认 2.0)')
    args = parser.parse_args()

    scan_baud(args.port, args.rounds, args.duration)


if __name__ == '__main__':
    main()
