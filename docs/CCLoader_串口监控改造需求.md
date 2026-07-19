# CCLoader 串口监控功能改造需求文档

> 文档位置：`d:\VC\diyruz_rt\docs\CCLoader_串口监控改造需求.md`
> 目标读者：负责实施改造的 AI 或开发者
> 项目路径：`D:\VC\CCLoader\`

## 1. 背景与目标

### 1.1 当前痛点

CCLoader 是用于烧录 TI CC2530/CC2540 等 Zigbee SoC 的工具，硬件基于 ESP8266 (NodeMCU) 通过 3 根调试线（RST/DC/DD）与目标芯片通信，走 TI 专有的 CC Debug 协议。

当前工作流存在严重痛点：

1. **烧录与监控无法并行**：CC2530 的串口日志输出（UART0 TX = P0_3）需要单独的 USB-TTL 监听，而 ESP8266 占用了唯一的 USB 串口
2. **频繁切换接线**：每次烧录完成后必须拔掉 ESP8266，把 USB-TTL 的 RX 接到 CC2530 的 P0_3 才能看日志；要看下一次烧录又得换回来
3. **无法观察启动日志**：CC2530 上电/复位瞬间的早期日志（如 `HAL_BOARD_INIT` 完成标记、`zclDIYRuZRT_Init` 入口）无法捕获，因为 USB-TTL 接好时启动已经完成
4. **多次复位才能定位问题**：开发期固件经常卡死在某个初始化阶段，没有早期日志就难以定位

### 1.2 改造目标

在 **不破坏现有烧录功能** 的前提下，让 CCLoader 同时具备 **UART 透传监控能力**，实现"一根线、一个工具、烧录+监控两不误"。

### 1.3 用户场景

典型工作流（改造后）：

1. ESP8266 接好 4 根线到 CC2530：RST、DC、DD、P0_3（新增第 4 根）
2. 运行 `CCLoader.exe 4 firmware.bin 0` 烧录固件（约 3 分钟）
3. 烧录完成后 CC2530 自动复位运行
4. **无需切换接线**，直接运行 `cc_monitor.py COM4 115200` 开始监控 CC2530 的串口日志
5. 随时按 CC2530 的 RESET，能立即捕获从 `main()` 第一行开始的全部日志

---

## 2. 硬件方案

### 2.1 ESP8266 引脚分配

在原有 3 根调试线基础上，新增 1 根 UART 接收线：

| ESP8266 GPIO | NodeMCU 标签 | 方向 | CC2530 引脚 | 用途 |
|---|---|---|---|---|
| GPIO5 | D1 | → | Pin 7 (RESETn) | CC Debug: 复位 |
| GPIO4 | D2 | → | Pin 3 (DC) | CC Debug: 时钟 |
| GPIO12 | D6 | ↔ | Pin 4 (DD) | CC Debug: 数据（双向） |
| **GPIO3** | **RX** | **←** | **P0_3 (UART0 TX)** | **UART 监控：接收 CC2530 日志** |
| GND | GND | — | GND | 共地 |

### 2.2 关键约束

ESP8266 的 **GPIO3 (RX)** 是硬件 UART0 的 RX 引脚，与 ESP8266 和上位机通信用的串口共用。这是本方案的核心难点，需要在固件中做时分复用：

- **烧录模式**：GPIO3 由 ESP8266 串口占用，用于和上位机收发 CC Debug 数据帧
- **监控模式**：GPIO3 切换为输入，直接读取 CC2530 从 P0_3 发过来的 UART 数据

两种模式互斥，不能同时进行。模式切换通过上位机命令触发。

### 2.3 备选方案（如果 GPIO3 复用有问题）

使用 ESP8266 的 **SoftwareSerial** 库，在任意 GPIO（如 GPIO14/D5）上实现软串口接收。优点是不与硬件串口冲突，缺点是：

- ESP8266 SoftwareSerial 在 115200 bps 下不稳定，可能丢字节
- 需要把 CC2530 固件波特率降到 38400 或 57600

**首选方案**仍是 GPIO3 硬件复用，备选方案仅在首选不可行时启用。

### 2.4 电平匹配

ESP8266 和 CC2530 都是 3.3V 逻辑电平，可以直接相连，无需电平转换。但如果连线较长（>20cm），建议在 P0_3 到 ESP8266 RX 之间加一个 4.7kΩ 上拉到 3.3V，提高信号完整性。

---

## 3. 软件架构

### 3.1 整体架构

```
┌──────────────────────────────────────────────────┐
│              上位机 (Windows PC)                  │
│  ┌──────────────────┐  ┌─────────────────────┐  │
│  │ CCLoader.exe     │  │ cc_monitor.py       │  │
│  │ (烧录，已有)      │  │ (监控，新增)         │  │
│  └────────┬─────────┘  └──────────┬──────────┘  │
│           │                        │              │
│           └──────────┬─────────────┘              │
│                      │ USB Serial (COMx)          │
└──────────────────────┼───────────────────────────┘
                       │
┌──────────────────────▼───────────────────────────┐
│              ESP8266 (NodeMCU)                    │
│  ┌────────────────────────────────────────────┐  │
│  │           CCLoader.ino (改造后)             │  │
│  │  ┌──────────────┐  ┌────────────────────┐ │  │
│  │  │ 烧录状态机    │  │ 透传状态机（新增）  │ │  │
│  │  │ (原有逻辑)    │  │                    │ │  │
│  │  └──────┬───────┘  └─────────┬──────────┘ │  │
│  │         │                     │            │  │
│  │         ▼                     ▼            │  │
│  │   CC Debug 协议           UART 透传        │  │
│  │   (RST/DC/DD)            (GPIO3/RX)        │  │
│  └────────────────────────────────────────────┘  │
└──────────────────────┬───────────────────────────┘
                       │
┌──────────────────────▼───────────────────────────┐
│              CC2530 目标芯片                      │
│   Pin 7 (RSTn) ──────── ESP8266 D1              │
│   Pin 3 (DC)   ──────── ESP8266 D2              │
│   Pin 4 (DD)   ──────── ESP8266 D6              │
│   P0_3 (TX)    ──────── ESP8266 RX (GPIO3) ★新增│
└──────────────────────────────────────────────────┘
```

### 3.2 通信协议设计

#### 3.2.1 命令字（上位机 → ESP8266）

ESP8266 在 `loop()` 空闲时持续监听串口，收到命令字后切换状态机：

| 命令字 | 值 | 含义 | 后续数据 |
|---|---|---|---|
| `CMD_BURN_BEGIN` | 0x01 | 进入烧录模式（原有 SBEGIN） | 1 字节 Verify 标志 |
| `CMD_MONITOR_BEGIN` | 0x10 | **新增**：进入监控模式 | 4 字节波特率（uint32, little-endian） |
| `CMD_MONITOR_STOP` | 0x11 | **新增**：退出监控模式，回到空闲 | 无 |
| `CMD_PING` | 0xFF | 心跳检测（空闲态响应） | 无 |

#### 3.2.2 响应字（ESP8266 → 上位机）

| 响应字 | 值 | 含义 |
|---|---|---|
| `RSP_OK` | 0x01 | 命令接受，开始执行 |
| `RSP_ERROR` | 0x02 | 命令拒绝或执行失败 |
| `RSP_MONITOR_DATA` | 0xA0 | **新增**：监控模式数据帧（后跟长度+数据） |
| `RSP_MONITOR_END` | 0xA1 | **新增**：监控模式结束（用户主动停止或超时） |

#### 3.2.3 监控模式数据帧格式

监控模式下，ESP8266 持续从 GPIO3 (RX) 读取 CC2530 的 UART 数据，并打包发送给上位机：

```
[RSP_MONITOR_DATA] [LEN_HIGH] [LEN_LOW] [DATA...] [CRC8]
       0xA0           1字节      1字节    N字节     1字节
```

- `LEN`：DATA 部分字节数（1~64 字节，避免单帧过大）
- `CRC8`：对 LEN+DATA 计算（多项式 0x07，初值 0x00），用于检测传输错误
- 上位机收到后剥离帧头，把 DATA 直接写入日志文件或控制台

#### 3.2.4 监控模式超时

- ESP8266 监控模式默认运行 600 秒后自动退出（避免长时间占用）
- 上位机可随时发送 `CMD_MONITOR_STOP` 主动退出
- 退出时发送 `RSP_MONITOR_END` 给上位机

---

## 4. ESP8266 固件改造规格

### 4.1 文件结构

改造后的 `CCLoader.ino` 结构：

```cpp
// === 原有部分（保持不变） ===
// - CC Debug 协议函数（write_debug_byte, read_debug_byte, debug_command...）
// - Flash 读写函数（write_flash_memory_block, read_flash_memory_block...）
// - 烧录状态机（loop 中的 WAITING/RECEIVING 状态）

// === 新增部分 ===
// - 监控模式状态机
// - UART 透传逻辑
// - 命令分发器（区分烧录命令和监控命令）

// 全局状态
enum CCLoaderState {
  STATE_IDLE,           // 空闲，等待命令
  STATE_BURNING,        // 烧录中（原有逻辑）
  STATE_MONITORING      // 监控中（新增）
};
CCLoaderState g_state = STATE_IDLE;
```

### 4.2 setup() 改造

```cpp
void setup() {
  ProgrammerInit();           // 原有：初始化 DD/DC/RESET/LED
  Serial.begin(115200);       // 原有：上位机通信
  // 注意：GPIO3 (RX) 默认就是 Serial 的 RX，不需要额外初始化
  // 监控模式下，CC2530 的 P0_3 会通过 GPIO3 进入 ESP8266 的硬件 UART
  g_state = STATE_IDLE;
}
```

### 4.3 loop() 改造

```cpp
void loop() {
  switch (g_state) {
    case STATE_IDLE:
      handle_idle();
      break;
    case STATE_BURNING:
      handle_burning();   // 原有的烧录逻辑
      break;
    case STATE_MONITORING:
      handle_monitoring(); // 新增
      break;
  }
}

void handle_idle() {
  if (Serial.available() >= 1) {
    uint8_t cmd = Serial.read();
    switch (cmd) {
      case CMD_BURN_BEGIN:        // 0x01
        enter_burn_mode();
        break;
      case CMD_MONITOR_BEGIN:     // 0x10
        enter_monitor_mode();
        break;
      case CMD_PING:              // 0xFF
        Serial.write(RSP_OK);
        break;
      default:
        // 未知命令，忽略
        break;
    }
  }
}
```

### 4.4 监控模式实现

```cpp
void enter_monitor_mode() {
  // 等待 4 字节波特率（little-endian uint32）
  uint32_t baud = 0;
  uint8_t baud_bytes[4];
  unsigned long start = millis();
  for (int i = 0; i < 4; i++) {
    while (!Serial.available()) {
      if (millis() - start > 1000) {
        Serial.write(RSP_ERROR);
        return;
      }
    }
    baud_bytes[i] = Serial.read();
  }
  baud = baud_bytes[0] | (baud_bytes[1] << 8) |
         (baud_bytes[2] << 16) | (baud_bytes[3] << 24);

  // 重新初始化串口到目标波特率
  // 注意：这会改变 ESP8266 和上位机的通信波特率
  // 上位机必须同步切换！
  Serial.flush();
  Serial.updateBaudRate(baud);   // ESP8266 Arduino core 支持

  // 发送确认（用新波特率）
  delay(100);
  Serial.write(RSP_OK);

  g_state = STATE_MONITORING;
  g_monitor_start = millis();
  g_monitor_timeout = 600000UL;  // 600 秒
}

void handle_monitoring() {
  // 检查超时
  if (millis() - g_monitor_start > g_monitor_timeout) {
    send_monitor_end();
    restore_serial();
    g_state = STATE_IDLE;
    return;
  }

  // 检查上位机是否发送 STOP 命令
  if (Serial.available()) {
    uint8_t ch = Serial.read();
    if (ch == CMD_MONITOR_STOP) {
      send_monitor_end();
      restore_serial();
      g_state = STATE_IDLE;
      return;
    }
    // 其他字节：忽略（或作为透传到 CC2530 的数据，本版本不支持）
  }

  // 读取 CC2530 通过 GPIO3 (RX) 发来的 UART 数据
  // 此时 Serial 的 RX 就是 GPIO3，直接读取即可
  const int MAX_DATA_LEN = 64;
  uint8_t buf[MAX_DATA_LEN];
  int len = 0;

  // 攒够一批或超时 50ms 就发送
  unsigned long batch_start = millis();
  while (len < MAX_DATA_LEN) {
    if (Serial.available()) {
      buf[len++] = Serial.read();
      batch_start = millis();  // 重置计时
    } else {
      if (len > 0 && millis() - batch_start > 50) {
        break;  // 50ms 没有新数据，发送这批
      }
      if (len == 0 && millis() - batch_start > 100) {
        break;  // 100ms 无数据，空转一次
      }
      delay(1);  // 让出 CPU
    }
  }

  if (len > 0) {
    send_monitor_frame(buf, len);
  }
}

void send_monitor_frame(uint8_t *data, int len) {
  Serial.write(RSP_MONITOR_DATA);    // 0xA0
  Serial.write((len >> 8) & 0xFF);   // LEN_HIGH
  Serial.write(len & 0xFF);          // LEN_LOW
  Serial.write(data, len);           // DATA
  Serial.write(crc8(data, len));     // CRC8
}

void send_monitor_end() {
  Serial.write(RSP_MONITOR_END);     // 0xA1
}

void restore_serial() {
  Serial.flush();
  Serial.updateBaudRate(115200);     // 恢复默认波特率
  delay(100);
}

uint8_t crc8(uint8_t *data, int len) {
  uint8_t crc = 0x00;
  for (int i = 0; i < len; i++) {
    crc ^= data[i];
    for (int j = 0; j < 8; j++) {
      if (crc & 0x80) {
        crc = (crc << 1) ^ 0x07;
      } else {
        crc <<= 1;
      }
    }
  }
  return crc;
}
```

### 4.5 烧录模式兼容性

原有的烧录逻辑（`SBEGIN`/`SDATA`/`SRSP`/`SEND`）保持不变，只是把入口从 `loop()` 直接执行改为 `handle_burning()` 状态机内执行。烧录完成后回到 `STATE_IDLE`。

### 4.6 关键注意事项

1. **波特率切换的同步问题**：
   - ESP8266 调用 `Serial.updateBaudRate(baud)` 后，必须等上位机也切换到相同波特率才能通信
   - 方案：ESP8266 先发送 `RSP_OK`（用新波特率），上位机收到后认为切换成功
   - 风险：如果上位机切换慢，ESP8266 发的 `RSP_OK` 会丢失
   - 缓解：ESP8266 在切换前先 `delay(100)`，给上位机时间切换；上位机切换后等待 200ms 再读

2. **GPIO3 复用冲突**：
   - 烧录模式下，GPIO3 是上位机→ESP8266 的命令通道
   - 监控模式下，GPIO3 是 CC2530→ESP8266 的日志通道
   - 两者互斥，必须通过状态机确保不会同时使用
   - 进入监控模式前，必须确保 CC2530 的 P0_3 已物理连接到 ESP8266 的 RX

3. **CC2530 复位时机**：
   - 监控模式下，用户可能按 CC2530 的 RESET 触发重启日志
   - ESP8266 不主动控制 CC2530 的 RESET（避免干扰监控）
   - 如果需要远程复位，可扩展 `CMD_CC2530_RESET` 命令（脉冲 RESET 引脚）

---

## 5. 上位机改造规格

### 5.1 新增工具：cc_monitor

新增一个独立的 Python 脚本 `cc_monitor.py`，用于通过 ESP8266 监控 CC2530 的串口日志。

#### 5.1.1 命令行接口

```bash
# 基本用法
python cc_monitor.py COM4 115200

# 完整参数
python cc_monitor.py \
  --port COM4 \              # ESP8266 所在 COM 口
  --baud 115200 \            # CC2530 UART 波特率
  --timeout 600 \            # 监控时长（秒），默认 600
  --output log.txt \         # 日志输出文件（可选，默认只打印控制台）
  --timestamp                # 每行加时间戳
```

#### 5.1.2 工作流程

```
1. 打开 COM4 @ 115200（ESP8266 默认波特率）
2. 发送 CMD_PING，等待 RSP_OK（确认 ESP8266 在线且空闲）
3. 发送 CMD_MONITOR_BEGIN + 4 字节波特率
4. 等待 ESP8266 回复 RSP_OK（用新波特率）
   - 上位机同步切换 COM4 到新波特率
5. 循环读取串口数据：
   - 解析 RSP_MONITOR_DATA 帧
   - 校验 CRC8
   - 把 DATA 部分写入控制台/文件
   - 收到 RSP_MONITOR_END 则退出
6. 用户按 Ctrl+C 退出时，发送 CMD_MONITOR_STOP
7. 恢复 COM4 到 115200，关闭
```

#### 5.1.3 Python 实现要点

```python
import serial
import struct
import time
import sys
import argparse

CMD_PING = 0xFF
CMD_MONITOR_BEGIN = 0x10
CMD_MONITOR_STOP = 0x11

RSP_OK = 0x01
RSP_ERROR = 0x02
RSP_MONITOR_DATA = 0xA0
RSP_MONITOR_END = 0xA1

def crc8(data):
    crc = 0x00
    for b in data:
        crc ^= b
        for _ in range(8):
            if crc & 0x80:
                crc = ((crc << 1) ^ 0x07) & 0xFF
            else:
                crc = (crc << 1) & 0xFF
    return crc

def monitor(port, baud, timeout, output, timestamp):
    # 1. 打开串口 @ 115200
    ser = serial.Serial(port, 115200, timeout=0.5)
    
    # 2. PING
    ser.write(bytes([CMD_PING]))
    rsp = ser.read(1)
    if rsp != bytes([RSP_OK]):
        print(f"ESP8266 未响应或忙，收到: {rsp.hex()}")
        return
    
    # 3. 发送 MONITOR_BEGIN
    baud_bytes = struct.pack('<I', baud)
    ser.write(bytes([CMD_MONITOR_BEGIN]) + baud_bytes)
    
    # 4. 切换波特率
    time.sleep(0.1)
    ser.baudrate = baud
    
    # 等待 RSP_OK
    rsp = ser.read(1)
    if rsp != bytes([RSP_OK]):
        print(f"进入监控模式失败，收到: {rsp.hex()}")
        return
    
    print(f"监控中... {port} @ {baud} bps，按 Ctrl+C 退出")
    
    # 5. 循环读取
    outfile = open(output, 'wb') if output else None
    start_time = time.time()
    buffer = bytearray()
    
    try:
        while time.time() - start_time < timeout:
            if ser.in_waiting > 0:
                ch = ser.read(1)[0]
                if ch == RSP_MONITOR_DATA:
                    # 读 LEN_HIGH, LEN_LOW
                    len_hi = ser.read(1)[0]
                    len_lo = ser.read(1)[0]
                    data_len = (len_hi << 8) | len_lo
                    # 读 DATA + CRC8
                    data = ser.read(data_len)
                    crc = ser.read(1)[0]
                    # 校验
                    if crc8(data) != crc:
                        print(f"[CRC 错误] 丢弃 {data_len} 字节", file=sys.stderr)
                        continue
                    # 输出
                    buffer.extend(data)
                    while b'\n' in buffer:
                        line, buffer = buffer.split(b'\n', 1)
                        line = line + b'\n'
                        if timestamp:
                            ts = time.strftime('%H:%M:%S.') + f'{int((time.time() % 1)*1000):03d}'
                            sys.stdout.buffer.write(f"[{ts}] ".encode() + line)
                        else:
                            sys.stdout.buffer.write(line)
                        sys.stdout.flush()
                        if outfile:
                            outfile.write(line)
                            outfile.flush()
                elif ch == RSP_MONITOR_END:
                    print("ESP8266 退出监控模式")
                    break
    except KeyboardInterrupt:
        print("\n用户中断，发送 STOP...")
        ser.baudrate = 115200
        ser.write(bytes([CMD_MONITOR_STOP]))
    finally:
        if outfile:
            outfile.close()
        ser.baudrate = 115200
        ser.close()
```

### 5.2 现有 CCLoader.exe 兼容性

现有 `CCLoader.exe` 不需要修改，它发送的 `SBEGIN` (0x01) 命令仍然被识别为烧录模式入口。改造后的 ESP8266 固件完全向后兼容。

---

## 6. 验收标准

### 6.1 功能验收

1. **烧录功能不回归**：
   - 用改造后的 ESP8266 固件 + 现有 CCLoader.exe 烧录 256KB BIN，成功率和速度与改造前一致
   - 烧录后 CC2530 能正常运行

2. **监控功能可用**：
   - 烧录完成后，运行 `python cc_monitor.py COM4 115200`
   - 按 CC2530 的 RESET，能在控制台看到从 `main()` 第一行开始的日志
   - 日志内容与直接用 USB-TTL 监听 P0_3 的结果一致

3. **模式切换可靠**：
   - 监控模式下按 Ctrl+C 退出后，能立即用 CCLoader.exe 重新烧录
   - 烧录完成后能立即用 cc_monitor.py 重新监控
   - 连续切换 10 次不卡死

4. **波特率支持**：
   - 支持 9600、19200、38400、57600、115200、230400 六种常见波特率
   - 波特率切换后数据无错乱

### 6.2 性能验收

1. **数据完整性**：
   - CC2530 发送 1KB 文本日志，cc_monitor.py 接收到的内容与原文逐字节一致
   - CRC8 校验通过率 > 99.9%

2. **延迟**：
   - CC2530 发送一行日志到 cc_monitor.py 显示的延迟 < 100ms
   - 按 RESET 到看到第一行日志的延迟 < 200ms

3. **稳定性**：
   - 连续监控 10 分钟无丢线、无卡死
   - CC2530 频繁复位（每 2 秒一次）不导致 ESP8266 崩溃

---

## 7. 交付物

### 7.1 ESP8266 固件

- 改造后的 `CCLoader.ino`（保持原有烧录功能，新增监控状态机）
- 对应的 `platformio.ini`（如需调整 build_flags）
- 编译后的固件 `.bin`（可直接烧录到 NodeMCU）

### 7.2 上位机工具

- `cc_monitor.py`：Python 监控脚本（依赖 pyserial）
- 可选：编译为 `cc_monitor.exe`（用 PyInstaller）

### 7.3 文档

- 接线说明（4 根线，含新增的 P0_3 → ESP8266 RX）
- 使用说明（烧录命令、监控命令、模式切换）
- 改造说明（与原版的差异点）

---

## 8. 风险与备选方案

### 8.1 风险 1：GPIO3 复用导致 ESP8266 不稳定

**现象**：ESP8266 在监控模式下频繁重启或丢数据

**原因**：GPIO3 是 ESP8266 的硬件 UART RX，CC2530 的 P0_3 输出可能电平不稳或波特率不匹配

**备选方案**：
- 改用 SoftwareSerial 在 GPIO14 (D5) 上接收，降低波特率到 38400
- 在 CC2530 和 ESP8266 之间加一个 74HC125 缓冲器隔离

### 8.2 风险 2：波特率切换不同步

**现象**：进入监控模式后收不到数据或乱码

**原因**：ESP8266 和上位机的波特率切换时序不一致

**缓解**：
- ESP8266 在 `Serial.updateBaudRate()` 前后加足够的 delay
- 上位机切换后发一个 `CMD_PING` 探测，收不到响应则重试

### 8.3 风险 3：CC2530 P0_3 驱动能力不足

**现象**：ESP8266 收到的数据电平不稳

**原因**：CC2530 的 P0_3 直接驱动 ESP8266 的 GPIO3（高阻输入），理论上没问题，但如果线长或有干扰可能出错

**缓解**：缩短 P0_3 到 ESP8266 RX 的连线，或加 4.7kΩ 上拉到 3.3V

---

## 9. 开发环境

### 9.1 ESP8266 固件

- PlatformIO（推荐）或 Arduino IDE
- ESP8266 Arduino Core 3.x 或 4.x
- 已有 `D:\VC\CCLoader\platformio.ini`，可直接 `pio run -t upload`

### 9.2 上位机

- Python 3.8+
- pyserial >= 3.5
- 可选 PyInstaller（打包为 exe）

### 9.3 测试环境

- NodeMCU ESP8266 (ESP-12E)
- CC2530 商业模块（已有 UART0_TX/UART0_RX 引出）
- Windows 10/11
- 现有 CCLoader.exe（用于回归测试）

---

## 10. 参考资料

- CCLoader 原项目：https://github.com/RedBearLab/CCLoader
- CCLib（另一套工具，协议类似）：https://github.com/wavesoft/CCLib
- ESP8266 Arduino Core Serial 文档：https://arduino-esp8266.readthedocs.io/en/latest/reference.html#serial
- CC2530 UART 寄存器：CC2530 User Guide (SWRU191)

---

## 附录 A：现有 CCLoader.ino 关键代码位置

文件：`D:\VC\CCLoader\Arduino\CCLoader\CCLoader.ino`

| 函数 | 行号 | 用途 |
|---|---|---|
| `setup()` | 523 | 初始化引脚 + Serial.begin(115200) |
| `loop()` | 532 | 烧录状态机主循环 |
| `debug_init()` | 234 | 进入 CC Debug 模式 |
| `read_chip_id()` | 261 | 读取芯片 ID |
| `chip_erase()` | 323 | 全片擦除 |
| `write_flash_memory_block()` | 462 | 写 Flash 块 |
| `RunDUP()` | 496 | 复位 CC2530 并运行 |

改造时需要在 `loop()` 开头加状态判断，原有烧录逻辑整体放入 `STATE_BURNING` 分支。

## 附录 B：通信时序示例

### B.1 烧录模式（原有，保持不变）

```
上位机: [0x01] [Verify]                  # SBEGIN
ESP8266: (执行烧录流程)
上位机: [0x02] [512字节] [校验2字节]      # SDATA + 数据
ESP8266: [0x03]                          # SRSP
... (重复 512 次)
上位机: [0x04]                           # SEND
ESP8266: (复位 CC2530)
```

### B.2 监控模式（新增）

```
上位机: [0xFF]                           # CMD_PING
ESP8266: [0x01]                          # RSP_OK

上位机: [0x10] [00 C2 01 00]             # CMD_MONITOR_BEGIN, baud=115200
ESP8266: (切换波特率到 115200)
ESP8266: [0x01]                          # RSP_OK (用新波特率)

(CC2530 开始输出日志)
ESP8266: [0xA0] [00 08] [RAW BOOT\r\n] [CRC8]   # RSP_MONITOR_DATA
ESP8266: [0xA0] [00 04] [M1\r\n] [CRC8]
... 

上位机: [0x11]                           # CMD_MONITOR_STOP
ESP8266: [0xA1]                          # RSP_MONITOR_END
ESP8266: (恢复波特率到 115200)
```
