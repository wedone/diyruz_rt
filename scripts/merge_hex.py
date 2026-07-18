"""
合并 Bootloader hex 和 OTA 应用 hex 为一个完整的固件 hex

用法：
    python scripts\merge_hex.py <bootloader.hex> <app.hex> <output.hex>

输出：
    合并后的 hex 文件，可直接烧录到 CC2530

Flash 布局：
    0x0000~0x07FF  Bootloader（2KB）
    0x0800~0x3DFFF 应用固件（含 OTA 缓存区）
    0x3E000~0x3FFFF NV + Lock Bits
"""
import sys
import os

def parse_hex_line(line):
    """解析 Intel HEX 格式的一行"""
    if not line.startswith(':'):
        return None
    line = line[1:].strip()
    byte_count = int(line[0:2], 16)
    address = int(line[2:6], 16)
    record_type = int(line[6:8], 16)
    data = line[8:8+byte_count*2]
    return byte_count, address, record_type, data

def read_hex_records(hex_file):
    """读取 hex 文件，返回 (address, data) 列表"""
    records = []
    base_addr = 0
    with open(hex_file, 'r') as f:
        for line in f:
            if not line.startswith(':'):
                continue
            parsed = parse_hex_line(line)
            if parsed is None:
                continue
            byte_count, addr, record_type, data = parsed

            if record_type == 0x00:  # Data
                full_addr = base_addr + addr
                records.append((full_addr, data))
            elif record_type == 0x01:  # EOF
                break
            elif record_type == 0x04:  # Extended Linear Address
                base_addr = int(data, 16) << 16
            elif record_type == 0x02:  # Extended Segment Address
                base_addr = int(data, 16) << 4
    return records

def write_hex_record(addr, data_str):
    """生成 Intel HEX 数据记录"""
    byte_count = len(data_str) // 2
    # 8051 地址回绕到 64KB 内
    addr_low = addr & 0xFFFF
    record = f":{byte_count:02X}{addr_low:04X}00{data_str}"

    # 计算校验和
    checksum = byte_count + (addr_low >> 8) + (addr_low & 0xFF) + 0x00
    for i in range(0, len(data_str), 2):
        checksum += int(data_str[i:i+2], 16)
    checksum = (-checksum) & 0xFF
    record += f"{checksum:02X}"
    return record

def write_hex_file(records, output_file):
    """写入 hex 文件"""
    # 按 address 排序
    records.sort(key=lambda x: x[0])

    with open(output_file, 'w') as f:
        current_base = 0
        for addr, data in records:
            # 检查是否需要切换 base address
            base = (addr >> 16) & 0xFFFF
            if base != current_base:
                if base > 0:
                    f.write(f":02000004{base:04X}")
                    checksum = (0x02 + 0x00 + 0x00 + 0x04 + (base >> 8) + (base & 0xFF)) & 0xFF
                    checksum = (-checksum) & 0xFF
                    f.write(f"{checksum:02X}\n")
                current_base = base

            # 写入数据记录
            low_addr = addr & 0xFFFF
            byte_count = len(data) // 2

            # 如果数据超过 64KB 边界，需要分段
            if low_addr + byte_count > 0x10000:
                # 分割
                split = 0x10000 - low_addr
                data1 = data[:split*2]
                data2 = data[split*2:]
                f.write(write_hex_record(addr, data1) + "\n")
                # 递归处理剩余
                # 简化：直接写入下一段
                new_base = (addr + split) >> 16
                f.write(f":02000004{new_base:04X}")
                checksum = (0x02 + 0x00 + 0x00 + 0x04 + (new_base >> 8) + (new_base & 0xFF)) & 0xFF
                checksum = (-checksum) & 0xFF
                f.write(f"{checksum:02X}\n")
                current_base = new_base
                f.write(write_hex_record(addr + split, data2) + "\n")
            else:
                f.write(write_hex_record(addr, data) + "\n")

        # EOF
        f.write(":00000001FF\n")

def merge_hex(bootloader_hex, app_hex, output_hex):
    """合并两个 hex 文件"""
    print(f"读取 Bootloader: {bootloader_hex}")
    boot_records = read_hex_records(bootloader_hex)
    print(f"  {len(boot_records)} 条记录")

    print(f"读取应用固件: {app_hex}")
    app_records = read_hex_records(app_hex)
    print(f"  {len(app_records)} 条记录")

    # 检查地址范围
    if boot_records:
        boot_min = min(r[0] for r in boot_records)
        boot_max = max(r[0] + len(r[1])//2 for r in boot_records)
        print(f"  Bootloader 范围: 0x{boot_min:04X} ~ 0x{boot_max:04X}")

    if app_records:
        app_min = min(r[0] for r in app_records)
        app_max = max(r[0] + len(r[1])//2 for r in app_records)
        print(f"  应用固件范围: 0x{app_min:04X} ~ 0x{app_max:04X}")

    # 合并记录
    all_records = boot_records + app_records

    # 检查重叠
    addr_set = set()
    overlap = False
    for addr, data in all_records:
        for i in range(0, len(data), 2):
            a = addr + i//2
            if a in addr_set:
                print(f"  警告: 地址 0x{a:04X} 重叠")
                overlap = True
            addr_set.add(a)

    if not overlap:
        print("  无地址重叠")

    print(f"写入合并文件: {output_hex}")
    write_hex_file(all_records, output_hex)
    print(f"  完成，共 {len(all_records)} 条记录")

if __name__ == "__main__":
    if len(sys.argv) != 4:
        print("用法: python merge_hex.py <bootloader.hex> <app.hex> <output.hex>")
        sys.exit(1)

    merge_hex(sys.argv[1], sys.argv[2], sys.argv[3])
