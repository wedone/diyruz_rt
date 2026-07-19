#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
串口日志监听脚本（带自动重连）
用法: python serial_listen.py [--port COM4] [--baud 38400] [--secs 60]

特性：
- 上电瞬间 USB-TTL 可能因电流冲击重新枚举，脚本会自动等待 COM 口恢复后重连
- 每行日志前打印毫秒级时间戳
"""
import argparse
import sys
import time
from datetime import datetime

try:
    import serial
    from serial.tools import list_ports
except ImportError:
    print("ERROR: pyserial not installed. run: pip install pyserial")
    sys.exit(1)


def port_available(name):
    try:
        for p in list_ports.comports():
            if p.device.upper() == name.upper():
                return True
    except Exception:
        pass
    return False


def open_port(name, baud):
    try:
        ser = serial.Serial(name, baud, bytesize=8, parity='N',
                            stopbits=1, timeout=0.3)
        return ser
    except Exception as e:
        return e


def flush_line(buf):
    """从 buf 中按 \\n 分行输出，返回剩余 buffer"""
    while True:
        idx = buf.find(b'\n')
        if idx < 0:
            break
        line = buf[:idx]
        del buf[:idx + 1]
        if line.endswith(b'\r'):
            line = line[:-1]
        try:
            text = line.decode('utf-8', errors='replace')
        except Exception:
            text = repr(line)
        ts = datetime.now().strftime('%H:%M:%S.%f')[:-3]
        print("[{}] {}".format(ts, text))
        sys.stdout.flush()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", default="COM4")
    parser.add_argument("--baud", type=int, default=38400)
    parser.add_argument("--secs", type=int, default=60)
    args = parser.parse_args()

    print("[{}] target {} @ {} 8N1, total {}s".format(
        datetime.now().strftime('%H:%M:%S'), args.port, args.baud, args.secs))
    sys.stdout.flush()

    deadline = time.time() + args.secs
    ser = None
    buf = bytearray()
    last_status = None

    while time.time() < deadline:
        if ser is None:
            # 等待 COM 口可用
            if not port_available(args.port):
                if last_status != "wait":
                    print("[{}] waiting for {} to appear...".format(
                        datetime.now().strftime('%H:%M:%S'), args.port))
                    sys.stdout.flush()
                    last_status = "wait"
                time.sleep(0.5)
                continue
            time.sleep(0.3)  # 等驱动稳定
            r = open_port(args.port, args.baud)
            if isinstance(r, Exception):
                if last_status != "busy":
                    print("[{}] {} busy: {}".format(
                        datetime.now().strftime('%H:%M:%S'), args.port, r))
                    sys.stdout.flush()
                    last_status = "busy"
                time.sleep(0.5)
                continue
            ser = r
            print("[{}] {} opened {} 8N1, listening...".format(
                datetime.now().strftime('%H:%M:%S'), args.port, args.baud))
            sys.stdout.flush()
            last_status = "open"

        # 已打开，读数据
        try:
            data = ser.read(256)
            if data:
                buf.extend(data)
                flush_line(buf)
            # 检查 buffer 是否过长（无换行符的乱码）
            if len(buf) > 1024:
                ts = datetime.now().strftime('%H:%M:%S.%f')[:-3]
                try:
                    text = buf.decode('utf-8', errors='replace')
                except Exception:
                    text = repr(buf)
                print("[{}] {}<no LF, flush>".format(ts, text))
                sys.stdout.flush()
                buf.clear()
        except Exception as e:
            print("[{}] read error: {}; will reopen".format(
                datetime.now().strftime('%H:%M:%S'), e))
            sys.stdout.flush()
            try:
                ser.close()
            except Exception:
                pass
            ser = None
            buf.clear()
            last_status = None
            time.sleep(0.5)

    if ser is not None:
        # 输出残留 buffer
        if buf:
            ts = datetime.now().strftime('%H:%M:%S.%f')[:-3]
            try:
                text = buf.decode('utf-8', errors='replace')
            except Exception:
                text = repr(buf)
            print("[{}] {}<no LF>".format(ts, text))
        try:
            ser.close()
        except Exception:
            pass
    print("[{}] close {}".format(datetime.now().strftime('%H:%M:%S'), args.port))
    sys.stdout.flush()


if __name__ == "__main__":
    main()
