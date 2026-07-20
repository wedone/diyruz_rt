#!/usr/bin/env python3
"""
Intel HEX -> BIN 转换器（适配 CCLoader 烧录 CC2530）
- 支持记录类型: 00 数据 / 01 结束 / 02 扩展段地址 / 04 线性地址 / 05 起始地址
- 自动校验每行 checksum
- 填充 0xFF 到指定大小（默认 256KB = 0x40000，适配 CC2530F256）
用法:
    python hex2bin.py <input.hex> [output.bin] [size_bytes]
"""
import sys
import os


def parse_ihex_line(line):
    """解析一行 Intel HEX，返回 (addr, rec_type, data) 或 None（跳过/错误）"""
    line = line.strip()
    if not line or not line.startswith(':'):
        return None
    raw = bytes.fromhex(line[1:])
    if len(raw) < 5:
        raise ValueError(f"行过短: {line}")
    byte_count = raw[0]
    addr = (raw[1] << 8) | raw[2]
    rec_type = raw[3]
    data = raw[4:4 + byte_count]
    if len(data) != byte_count:
        raise ValueError(f"数据长度不匹配: 期望 {byte_count}, 实际 {len(data)}")
    # checksum: 对所有字节（含 count/addr/type/data）求和取低 8 位，应为 0
    checksum = raw[-1]
    s = sum(raw[:-1]) & 0xFF
    if (s + checksum) & 0xFF != 0:
        raise ValueError(f"checksum 错误: {line}")
    return addr, rec_type, data


def hex_to_bin(hex_path, bin_path, total_size=0x40000):
    base_addr = 0       # 线性地址高 16 位（type 04）
    seg_base = 0        # 段地址（type 02），左移 4 位
    min_addr = 0xFFFFFFFF
    max_addr = 0
    chunks = {}         # addr -> bytes

    with open(hex_path, 'r', encoding='ascii', errors='replace') as f:
        for line_no, line in enumerate(f, 1):
            try:
                parsed = parse_ihex_line(line)
            except ValueError as e:
                raise ValueError(f"第 {line_no} 行: {e}")
            if parsed is None:
                continue
            addr, rec_type, data = parsed

            if rec_type == 0x00:        # 数据
                full_addr = base_addr + seg_base + addr
                chunks[full_addr] = data
                if full_addr < min_addr:
                    min_addr = full_addr
                if full_addr + len(data) - 1 > max_addr:
                    max_addr = full_addr + len(data) - 1
            elif rec_type == 0x01:      # 结束
                break
            elif rec_type == 0x02:      # 扩展段地址
                seg_base = ((data[0] << 8) | data[1]) << 4
            elif rec_type == 0x04:      # 扩展线性地址
                base_addr = ((data[0] << 8) | data[1]) << 16
            elif rec_type == 0x05:      # 起始地址（忽略）
                pass
            # 其他类型忽略

    if not chunks:
        raise RuntimeError("未找到任何数据记录")

    # 输出缓冲区：默认填充 0xFF
    buf = bytearray(b'\xFF' * total_size)

    for addr, data in chunks.items():
        if addr + len(data) > total_size:
            raise RuntimeError(
                f"地址 0x{addr:X}+{len(data)} 超出输出大小 0x{total_size:X}"
            )
        buf[addr:addr + len(data)] = data

    with open(bin_path, 'wb') as f:
        f.write(buf)

    print(f"[hex2bin] 输入: {hex_path}")
    print(f"[hex2bin] 输出: {bin_path} ({total_size} 字节)")
    print(f"[hex2bin] 数据范围: 0x{min_addr:06X} - 0x{max_addr:06X}")
    print(f"[hex2bin] 数据记录数: {len(chunks)}")
    return bin_path


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(1)
    hex_path = sys.argv[1]
    if len(sys.argv) >= 3:
        bin_path = sys.argv[2]
    else:
        bin_path = os.path.splitext(hex_path)[0] + '.bin'
    size = 0x40000
    if len(sys.argv) >= 4:
        size = int(sys.argv[3], 0)
    hex_to_bin(hex_path, bin_path, size)


if __name__ == '__main__':
    main()
