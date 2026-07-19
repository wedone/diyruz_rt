#!/usr/bin/env python3
"""HEX 转 BIN 工具（适配 CCLoader 烧录）

用法：
    python hex2bin.py input.hex output.bin [--pad-to 0x40000]

特性：
- 支持Intel HEX 全部记录类型（00/01/02/04/05）
- 自动校验每行 checksum
- 默认填充 0xFF 到 256KB（0x40000），适配 CC2530 CCLoader
"""
import argparse
import sys


def parse_hex_line(line):
    """解析一行 Intel HEX，返回 (addr, data_bytes) 或 None（结束）"""
    line = line.strip()
    if not line or not line.startswith(':'):
        return None
    hex_data = line[1:]
    if len(hex_data) < 10 or len(hex_data) % 2 != 0:
        raise ValueError(f"无效行长度: {line}")
    byte_count = int(hex_data[0:2], 16)
    address = int(hex_data[2:6], 16)
    record_type = int(hex_data[6:8], 16)
    data_hex = hex_data[8:8 + byte_count * 2]
    data = bytes.fromhex(data_hex)
    # 校验
    checksum_byte = int(hex_data[8 + byte_count * 2:10 + byte_count * 2], 16)
    all_bytes = bytes.fromhex(hex_data[0:8 + byte_count * 2])
    calc = (-sum(all_bytes)) & 0xFF
    if calc != checksum_byte:
        raise ValueError(f"checksum 错误: 计算={calc:#04x}, 文件={checksum_byte:#04x}: {line}")
    return byte_count, address, record_type, data


def hex_to_bin(hex_path, bin_path, pad_to=0x40000):
    """把 HEX 文件转为 BIN，填充到 pad_to 大小"""
    base_addr = 0  # 扩展线性地址（记录类型 04）
    seg_addr = 0   # 扩展段地址（记录类型 02）
    memory = {}    # addr -> byte
    min_addr = 0xFFFFFFFF
    max_addr = 0
    data_records = 0

    with open(hex_path, 'r', encoding='utf-8', errors='replace') as f:
        for line_no, line in enumerate(f, 1):
            try:
                result = parse_hex_line(line)
                if result is None:
                    continue
                byte_count, address, record_type, data = result
                if record_type == 0x00:  # 数据
                    abs_addr = base_addr + address
                    for i, b in enumerate(data):
                        memory[abs_addr + i] = b
                    min_addr = min(min_addr, abs_addr)
                    max_addr = max(max_addr, abs_addr + byte_count - 1)
                    data_records += 1
                elif record_type == 0x01:  # 结束
                    break
                elif record_type == 0x02:  # 段地址
                    seg_addr = int.from_bytes(data, 'big') * 16
                    base_addr = seg_addr
                elif record_type == 0x04:  # 线性地址
                    base_addr = int.from_bytes(data, 'big') << 16
                elif record_type == 0x03 or record_type == 0x05:  # 起始地址，忽略
                    pass
                else:
                    print(f"警告: 未知记录类型 {record_type:#04x} (行 {line_no})", file=sys.stderr)
            except ValueError as e:
                raise ValueError(f"行 {line_no}: {e}")

    if data_records == 0:
        raise ValueError("HEX 文件无数据记录")

    print(f"数据记录: {data_records}")
    print(f"地址范围: 0x{min_addr:08X} - 0x{max_addr:08X} ({max_addr - min_addr + 1} 字节)")

    # 填充到 pad_to
    if pad_to > 0:
        size = pad_to
        print(f"填充到: 0x{size:X} ({size} 字节)")
    else:
        size = max_addr + 1

    # 生成 BIN
    bin_data = bytearray(b'\xFF' * size)
    for addr, b in memory.items():
        if addr < size:
            bin_data[addr] = b

    with open(bin_path, 'wb') as f:
        f.write(bin_data)

    print(f"已写入: {bin_path} ({len(bin_data)} 字节)")
    return len(bin_data)


def main():
    parser = argparse.ArgumentParser(description='HEX 转 BIN（CCLoader 烧录适配）')
    parser.add_argument('input', help='输入 HEX 文件路径')
    parser.add_argument('output', nargs='?', help='输出 BIN 文件路径（默认同名 .bin）')
    parser.add_argument('--pad-to', type=lambda x: int(x, 0), default=0x40000,
                        help='填充到的目标大小（字节，默认 0x40000=256KB）')
    parser.add_argument('--no-pad', action='store_true', help='不填充，只输出实际数据范围')
    args = parser.parse_args()

    output = args.output
    if not output:
        if args.input.lower().endswith('.hex'):
            output = args.input[:-4] + '.bin'
        else:
            output = args.input + '.bin'

    pad = 0 if args.no_pad else args.pad_to
    try:
        hex_to_bin(args.input, output, pad)
    except Exception as e:
        print(f"错误: {e}", file=sys.stderr)
        sys.exit(1)


if __name__ == '__main__':
    main()
