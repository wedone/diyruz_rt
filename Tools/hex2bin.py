#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
hex2bin.py - 将 IAR 输出的 Intel HEX 转换为 CCLib 可烧录的 raw binary

用法:
    python hex2bin.py <input.hex> [output.bin] [--pad-to <addr>]

说明:
    - 解析 Intel HEX 文件（含 type 04 扩展线性地址记录，支持 banked code）
    - 按物理地址写入 BIN 文件，空隙用 0xFF 填充
    - CCLib 的 CCHEXFile._loadBin 会把整个 BIN 作为一个从地址 0 开始的 memBlock
    - 因此 BIN 必须从 0x00000 开始，连续到最高数据地址
    - --pad-to <addr>：填充到指定地址（如 0x40000 = 256KB），CCLoader 要求

注意:
    CCLib 烧录 BIN 时会写入所有 2KB 块（包括 0xFF 填充块），可能比 HEX 慢。
    CCLoader 要求 256KB 完整尺寸 BIN，请用 --pad-to 0x40000。
"""

import sys
import os


def parse_hex_line(line):
    """解析一行 Intel HEX，返回 (addr, type, data) 或 None（结束记录）"""
    line = line.strip()
    if not line or line[0] != ':':
        return None

    raw = bytes.fromhex(line[1:])
    if len(raw) < 5:
        return None

    bCount = raw[0]
    bAddr = (raw[1] << 8) | raw[2]
    bType = raw[3]
    data = raw[4:4 + bCount]

    # 校验和验证
    csum = raw[-1]
    if (sum(raw[:-1]) + csum) & 0xFF != 0:
        raise ValueError("Checksum error: %s" % line)

    return (bAddr, bType, data)


def hex_to_bin(hex_path, bin_path, pad_to=None):
    """将 HEX 文件转换为 BIN 文件

    pad_to: 如果指定，填充到该地址（如 0x40000 = 256KB）
    """

    # 收集所有数据记录
    data_map = {}  # addr -> bytes
    base_address = 0x00000
    min_addr = None
    max_addr = None
    type_counts = {0: 0, 1: 0, 2: 0, 4: 0, 5: 0, 'other': 0}

    with open(hex_path, 'r') as f:
        for line_no, line in enumerate(f, 1):
            result = parse_hex_line(line)
            if result is None:
                continue

            bAddr, bType, data = result

            if bType in type_counts:
                type_counts[bType] += 1
            else:
                type_counts['other'] += 1

            # 结束记录
            if bType == 0x01:
                break
            # 扩展段地址记录（type 02）
            elif bType == 0x02:
                base_address = ((data[0] << 8) | data[1]) << 4
            # 扩展线性地址记录（type 04）
            elif bType == 0x04:
                base_address = ((data[0] << 8) | data[1]) << 16
            # 起始线性地址记录（type 05），忽略
            elif bType == 0x05:
                pass
            # 数据记录（type 00）
            elif bType == 0x00:
                phys_addr = base_address + bAddr
                data_map[phys_addr] = data
                if min_addr is None or phys_addr < min_addr:
                    min_addr = phys_addr
                end_addr = phys_addr + len(data) - 1
                if max_addr is None or end_addr > max_addr:
                    max_addr = end_addr
            else:
                print("WARNING: line %d: unknown record type 0x%02x" % (line_no, bType))

    if min_addr is None:
        raise ValueError("No data records found in HEX file")

    print("HEX file: %s" % hex_path)
    print("  Data records (type 00): %d" % type_counts[0])
    print("  Ext linear addr (type 04): %d" % type_counts[4])
    print("  Address range: 0x%05x - 0x%05x" % (min_addr, max_addr))
    print("  Data span: %d bytes (%.1f KB)" % (
        max_addr - min_addr + 1, (max_addr - min_addr + 1) / 1024.0))

    # 决定 BIN 大小
    data_size = max_addr + 1
    if pad_to is not None and pad_to > data_size:
        bin_size = pad_to
        pad_bytes = bin_size - data_size
    else:
        bin_size = data_size
        pad_bytes = 0

    print("  BIN file size: %d bytes (%.1f KB)" % (bin_size, bin_size / 1024.0))
    if pad_bytes > 0:
        print("  Tail pad (0xFF fill): %d bytes (%.1f KB)" % (pad_bytes, pad_bytes / 1024.0))

    # 统计空隙
    gap_bytes = data_size - sum(len(d) for d in data_map.values())
    print("  Gap (0xFF fill): %d bytes (%.1f KB)" % (gap_bytes, gap_bytes / 1024.0))

    # 写入 BIN
    with open(bin_path, 'wb') as f:
        # 一次性分配 buffer
        buffer = bytearray(b'\xff' * bin_size)
        for addr, data in data_map.items():
            buffer[addr:addr + len(data)] = data
        f.write(buffer)

    print("  Output: %s" % bin_path)
    print("  Done.")
    return bin_size


def main():
    if len(sys.argv) < 2:
        print("Usage: python hex2bin.py <input.hex> [output.bin] [--pad-to <addr>]")
        print("  --pad-to <addr>  填充到指定地址，如 0x40000 (256KB) 适配 CCLoader")
        sys.exit(1)

    # 解析参数
    hex_path = None
    bin_path = None
    pad_to = None
    i = 1
    while i < len(sys.argv):
        arg = sys.argv[i]
        if arg == '--pad-to':
            i += 1
            if i >= len(sys.argv):
                print("ERROR: --pad-to requires a value")
                sys.exit(1)
            v = sys.argv[i]
            if v.lower().startswith('0x'):
                pad_to = int(v, 16)
            else:
                pad_to = int(v)
        elif hex_path is None:
            hex_path = arg
        elif bin_path is None:
            bin_path = arg
        i += 1

    if hex_path is None:
        print("ERROR: HEX file path required")
        sys.exit(1)

    if not os.path.exists(hex_path):
        print("ERROR: HEX file not found: %s" % hex_path)
        sys.exit(1)

    if bin_path is None:
        bin_path = os.path.splitext(hex_path)[0] + '.bin'

    hex_to_bin(hex_path, bin_path, pad_to)


if __name__ == '__main__':
    main()
