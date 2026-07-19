# CCLoader WebUI 改造需求文档

> 文档位置：`d:\VC\diyruz_rt\docs\CCLoader_WebUI_改造需求.md`
> 目标读者：负责实施改造的 AI 或开发者
> 项目路径：`D:\VC\CCLoader\`（源码） / `D:\VC\CCLoader\Arduino\CCLoader\CCLoader.ino`（固件主文件）

## 1. 背景与目标

### 1.1 当前痛点

CCLoader 当前是基于命令行的 CC2530 烧录工具，存在以下痛点：

1. **平台限制**：`CCLoader.exe` 仅支持 Windows，Mac/Linux 用户需自行编译
2. **无监控能力**：烧录后想看 CC2530 串口日志，必须拔掉 ESP8266，换接 USB-TTL 到 P0_3，频繁切换
3. **看不到启动日志**：USB-TTL 接好后，CC2530 早已跑过 `main()` 早期阶段，无法定位启动卡死问题
4. **命令行不友好**：每次烧录要敲命令，BIN 路径容易出错，进度只有百分比文字
5. **无固件管理**：BIN 文件散落在各处，无法在工具内管理多个版本

### 1.2 改造目标

把 NodeMCU ESP8266 升级为 **独立的烧录+监控一体机**，用户只需用浏览器访问 ESP8266 的 IP，即可完成：

- 上传 BIN 固件文件
- 一键烧录到 CC2530（实时进度条）
- 实时监控 CC2530 串口日志（按 RESET 即可看启动日志）
- 烧录与监控无缝切换，**无需拔插任何线**
- 跨平台支持（Win/Mac/Linux/手机浏览器）

### 1.3 用户场景

典型工作流：

1. ESP8266 接好 4 根线到 CC2530（RST/DC/DD/P0_3→RX），USB 供电
2. ESP8266 上电后自动连接 WiFi（或开 AP 模式），IP 显示在串口监视器或 OLED（可选）
3. 浏览器访问 `http://192.168.x.x/`
4. 在"烧录"页上传 `DIYRuZRT.bin`，点"开始烧录"，进度条实时显示
5. 烧录完成后自动跳到"监控"页，CC2530 已复位运行
6. 按 CC2530 的 RESET，立即看到从 `main()` 第一行开始的日志
7. 日志可暂停/清空/下载/搜索

---

## 2. 硬件方案

### 2.1 ESP8266 引脚分配

| ESP8266 GPIO | NodeMCU 标签 | 方向 | CC2530 引脚 | 用途 |
|---|---|---|---|---|
| GPIO5 | D1 | → | Pin 7 (RESETn) | CC Debug: 复位 |
| GPIO4 | D2 | → | Pin 3 (DC) | CC Debug: 时钟 |
| GPIO12 | D6 | ↔ | Pin 4 (DD) | CC Debug: 数据（双向） |
| **GPIO3** | **RX** | **←** | **P0_3 (UART0 TX)** | **UART 监控：接收 CC2530 日志** |
| GND | GND | — | GND | 共地 |

### 2.2 GPIO3 复用说明

GPIO3 (RX) 在不同模式下用途不同，由固件状态机保证互斥：

- **空闲/烧录模式**：GPIO3 是上位机 ↔ ESP8266 的串口命令通道（USB 串口）
- **监控模式**：GPIO3 是 CC2530 P0_3 → ESP8266 的日志接收通道

由于 WebUI 方案下，上位机通过 WiFi（而非串口）与 ESP8266 通信，烧录命令走 WiFi，GPIO3 可以专门用于监控。但 ESP8266 启动时 Serial 仍占用 GPIO3，所以进入监控模式前需要 `Serial.end()` 释放 GPIO3，改用 `SoftwareSerial` 或直接寄存器读取。

**推荐方案**：监控模式时直接用硬件 UART0（GPIO3）接收，关闭 Serial 对上位机的输出（上位机走 WiFi）。退出监控模式时恢复 Serial。

### 2.3 电源

- NodeMCU 通过 USB 供电（5V → 板载 3.3V LDO）
- CC2530 模块从 NodeMCU 的 3.3V 引脚取电（电流 < 50mA，足够）
- 共地必须连接

### 2.4 可选：状态指示 LED

NodeMCU 板载 LED（GPIO2 / D4）用于状态指示：

| LED 状态 | 含义 |
|---|---|
| 常灭 | 空闲 |
| 慢闪（1Hz） | WiFi 连接中 |
| 常亮 | WiFi 已连接，等待操作 |
| 快闪（5Hz） | 烧录中 |
| 短闪（10Hz，0.1s on / 1s off） | 监控中 |

---

## 3. 软件架构

### 3.1 整体架构

```
┌──────────────────────────────────────────────────────┐
│              浏览器 (任意 OS)                          │
│   ┌──────────────────────────────────────────────┐  │
│   │  WebUI (HTML/JS/CSS，嵌入 ESP8266 Flash)      │  │
│   │  - 烧录页：上传 BIN + 进度条                   │  │
│   │  - 监控页：实时日志 + 暂停/清空/下载            │  │
│   │  - 设置页：WiFi 配置 + 波特率                  │  │
│   └────────────┬─────────────────────────────────┘  │
└────────────────┼─────────────────────────────────────┘
                 │ HTTP + WebSocket (WiFi)
┌────────────────▼─────────────────────────────────────┐
│              NodeMCU ESP8266                          │
│  ┌────────────────────────────────────────────────┐  │
│  │           CCLoader.ino (改造后)                 │  │
│  │  ┌──────────────┐  ┌────────────────────────┐ │  │
│  │  │ HTTP 服务器   │  │ WebSocket 服务器        │ │  │
│  │  │ (WebServer)  │  │ (实时日志推送)          │ │  │
│  │  └──────┬───────┘  └──────────┬─────────────┘ │  │
│  │         │                     │               │  │
│  │  ┌──────▼─────────────────────▼─────────────┐ │  │
│  │  │           状态机 (IDLE/BURN/MONITOR)      │ │  │
│  │  └──────┬─────────────────────┬─────────────┘ │  │
│  │         │                     │               │  │
│  │         ▼                     ▼               │  │
│  │   CC Debug 协议           UART 透传           │  │
│  │   (RST/DC/DD)            (GPIO3/RX)           │  │
│  │                                              │  │
│  │  ┌──────────────────────────────────────────┐ │  │
│  │  │  LittleFS 文件系统                        │ │  │
│  │  │  - /firmware.bin (待烧录)                 │ │  │
│  │  │  - /config.json (WiFi/波特率配置)         │ │  │
│  │  │  - /webui.html (前端页面)                 │ │  │
│  │  └──────────────────────────────────────────┘ │  │
│  └────────────────────────────────────────────────┘  │
└──────────────────────┬───────────────────────────────┘
                       │
┌──────────────────────▼───────────────────────────────┐
│              CC2530 目标芯片                          │
│   Pin 7 (RSTn) ──────── ESP8266 D1                  │
│   Pin 3 (DC)   ──────── ESP8266 D2                  │
│   Pin 4 (DD)   ──────── ESP8266 D6                  │
│   P0_3 (TX)    ──────── ESP8266 RX (GPIO3)          │
└──────────────────────────────────────────────────────┘
```

### 3.2 状态机设计

ESP8266 固件核心是三态状态机，保证烧录和监控互斥：

```
        ┌──────────────────────────────────┐
        │                                  │
        ▼                                  │
    ┌────────┐  HTTP /api/burn   ┌────────┴┐
    │  IDLE  │ ─────────────────▶│ BURNING │
    └────────┘                   └─────────┘
        │                              │
        │ HTTP /api/monitor            │ 烧录完成/失败
        ▼                              │
    ┌──────────┐                       │
    │ MONITOR  │◀──────────────────────┘
    └──────────┘
        │
        │ HTTP /api/stop
        ▼
    ┌────────┐
    │  IDLE  │
    └────────┘
```

| 状态 | GPIO3 用途 | Serial 串口 | WiFi | 响应的 HTTP API |
|---|---|---|---|---|
| IDLE | 上位机串口（可选） | 开启 | 开启 | 所有 API |
| BURNING | 不使用（CC Debug 走 DD） | 关闭 | 开启 | 仅 /api/status |
| MONITOR | 接收 CC2530 日志 | 关闭（或重配为 CC2530 波特率） | 开启 | 仅 /api/status + /api/stop |

### 3.3 技术栈

| 模块 | 库 / 技术 | 说明 |
|---|---|---|
| WiFi 连接 | ESP8266WiFi | 内置 |
| HTTP 服务器 | ESP8266WebServer | 内置，处理文件上传、API |
| WebSocket 服务器 | WebSocketsServer（Markus Sattler） | 需安装，实时推送日志和进度 |
| 文件系统 | LittleFS | 内置，存储 BIN 和 WebUI 静态文件 |
| JSON 解析 | ArduinoJson 6.x | 需安装，配置文件读写 |
| 前端 | 原生 HTML/JS/CSS | 无框架依赖，体积小 |
| 前端图表 | 不使用 | 进度条用原生 div 实现 |

---

## 4. ESP8266 固件改造规格

### 4.1 文件结构

```
D:\VC\CCLoader\
├── Arduino\
│   └── CCLoader\
│       └── CCLoader.ino          # 改造后的主固件
├── data\                         # LittleFS 上传目录（PlatformIO 约定）
│   ├── index.html                # WebUI 主页面
│   ├── style.css                 # 样式
│   ├── app.js                    # 前端逻辑
│   └── config.json               # 默认配置
├── platformio.ini                # PlatformIO 配置（需修改）
└── README.md                     # 使用说明
```

### 4.2 platformio.ini 配置

```ini
[platformio]
default_envs = nodemcuv2

[env:nodemcuv2]
platform = espressif8266@4.2.0
board = nodemcuv2
framework = arduino
monitor_speed = 115200
board_build.filesystem = littlefs
lib_deps =
    WebSockets@2.4.0
    ArduinoJson@6.21.5
build_flags =
    -DLED_BUILTIN=2
```

### 4.3 CCLoader.ino 主结构

```cpp
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WebSocketsServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <FS.h>

// === 原有 CC Debug 协议函数（保持不变） ===
// write_debug_byte, read_debug_byte, debug_command,
// debug_init, read_chip_id, chip_erase,
// write_flash_memory_block, read_flash_memory_block, RunDUP...

// === 状态机 ===
enum CCLoaderState {
  STATE_IDLE,
  STATE_BURNING,
  STATE_MONITORING
};
CCLoaderState g_state = STATE_IDLE;

// === 全局对象 ===
ESP8266WebServer server(80);
WebSocketsServer webSocket(81);

// === 配置 ===
struct Config {
  String wifi_ssid;
  String wifi_password;
  uint32_t monitor_baud;
  uint8_t verify;
};
Config g_config;

// === 烧录状态 ===
struct BurnState {
  uint32_t total_blocks;
  uint32_t current_block;
  uint8_t percent;
  String error;
  bool done;
};
BurnState g_burn;

// === 监控缓冲 ===
#define MONITOR_BUF_SIZE 256
uint8_t g_monitor_buf[MONITOR_BUF_SIZE];
uint16_t g_monitor_len = 0;

// === 引脚定义（保持原有） ===
int DD = 6;
int DC = 5;
int RESET = 4;
int LED = LED_BUILTIN;
```

### 4.4 setup() 改造

```cpp
void setup() {
  ProgrammerInit();
  Serial.begin(115200);
  
  // 挂载 LittleFS
  if (!LittleFS.begin()) {
    Serial.println("LittleFS mount failed");
    return;
  }
  
  // 加载配置
  loadConfig();
  
  // 连接 WiFi（或开 AP）
  initWiFi();
  
  // 初始化 HTTP 路由
  initHttpRoutes();
  
  // 启动 WebSocket
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
  
  // 启动 HTTP 服务器
  server.begin();
  
  Serial.println("CCLoader WebUI ready");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
  
  g_state = STATE_IDLE;
  updateLED();
}
```

### 4.5 loop() 改造

```cpp
void loop() {
  server.handleClient();
  webSocket.loop();
  
  switch (g_state) {
    case STATE_IDLE:
      // 空闲，等待 HTTP 命令
      break;
    case STATE_BURNING:
      // 烧录在 HTTP handler 中同步执行
      // 完成后自动回到 IDLE
      break;
    case STATE_MONITORING:
      handle_monitoring();
      break;
  }
  
  updateLED();
}

void handle_monitoring() {
  // 从 Serial (GPIO3) 读取 CC2530 日志
  while (Serial.available()) {
    uint8_t ch = Serial.read();
    if (g_monitor_len < MONITOR_BUF_SIZE) {
      g_monitor_buf[g_monitor_len++] = ch;
    }
  }
  
  // 攒够 64 字节或每 50ms 推送一次
  static unsigned long last_push = 0;
  if (g_monitor_len >= 64 || (g_monitor_len > 0 && millis() - last_push > 50)) {
    pushMonitorData();
    g_monitor_len = 0;
    last_push = millis();
  }
}

void pushMonitorData() {
  if (g_monitor_len == 0) return;
  
  // 构造 JSON 消息
  DynamicJsonDocument doc(512 + g_monitor_len * 2);
  doc["type"] = "monitor_data";
  doc["data"] = base64_encode(g_monitor_buf, g_monitor_len);
  
  String json;
  serializeJson(doc, json);
  webSocket.broadcastTXT(json);
}

// Base64 编码（避免二进制数据在 JSON 中出问题）
String base64_encode(const uint8_t *data, size_t len) {
  static const char *alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  String result;
  result.reserve((len + 2) / 3 * 4);
  for (size_t i = 0; i < len; i += 3) {
    uint32_t n = ((uint32_t)data[i]) << 16;
    if (i + 1 < len) n |= ((uint32_t)data[i+1]) << 8;
    if (i + 2 < len) n |= data[i+2];
    result += alphabet[(n >> 18) & 0x3F];
    result += alphabet[(n >> 12) & 0x3F];
    result += (i + 1 < len) ? alphabet[(n >> 6) & 0x3F] : '=';
    result += (i + 2 < len) ? alphabet[n & 0x3F] : '=';
  }
  return result;
}
```

### 4.6 HTTP 路由定义

| 方法 | 路径 | 功能 | 请求体 | 响应 |
|---|---|---|---|---|
| GET | `/` | 返回 WebUI 主页 | - | `index.html` |
| GET | `/style.css` | 样式文件 | - | CSS |
| GET | `/app.js` | 前端脚本 | - | JS |
| GET | `/api/status` | 获取当前状态 | - | JSON |
| GET | `/api/config` | 获取配置 | - | JSON |
| POST | `/api/config` | 保存配置 | JSON | JSON |
| POST | `/api/upload` | 上传 BIN 文件 | multipart/form-data | JSON |
| POST | `/api/burn` | 开始烧录 | JSON (verify) | JSON |
| POST | `/api/monitor` | 开始监控 | JSON (baud) | JSON |
| POST | `/api/stop` | 停止当前操作 | - | JSON |
| GET | `/api/files` | 列出已上传固件 | - | JSON |
| DELETE | `/api/files/{name}` | 删除固件 | - | JSON |

#### 4.6.1 `/api/status` 响应示例

```json
{
  "state": "idle",
  "burn": {
    "percent": 0,
    "current_block": 0,
    "total_blocks": 0,
    "done": false,
    "error": ""
  },
  "monitor": {
    "active": false,
    "baud": 115200,
    "bytes_received": 0
  },
  "wifi": {
    "ssid": "MyWiFi",
    "ip": "192.168.1.100",
    "rssi": -55
  },
  "uptime": 3600
}
```

#### 4.6.2 上传 BIN 文件

```cpp
void handleUpload() {
  HTTPUpload &upload = server.upload();
  
  if (upload.status == UPLOAD_FILE_START) {
    String filename = upload.filename;
    if (filename == "") filename = "firmware.bin";
    // 保存到 LittleFS
    fs::File f = LittleFS.open("/" + filename, "w");
    if (!f) {
      server.send(500, "application/json", "{\"error\":\"cannot open file\"}");
      return;
    }
    Serial.printf("Upload start: %s\n", filename.c_str());
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    // 追加写入
    fs::File f = LittleFS.open("/firmware.bin", "a");
    f.write(upload.buf, upload.currentSize);
    f.close();
  } else if (upload.status == UPLOAD_FILE_END) {
    Serial.printf("Upload end: %u bytes\n", upload.totalSize);
    server.send(200, "application/json", 
                "{\"success\":true,\"size\":" + String(upload.totalSize) + "}");
  }
}
```

#### 4.6.3 烧录 API

```cpp
void handleBurn() {
  if (g_state != STATE_IDLE) {
    server.send(409, "application/json", "{\"error\":\"busy\"}");
    return;
  }
  
  // 解析请求
  String body = server.arg("plain");
  DynamicJsonDocument doc(256);
  deserializeJson(doc, body);
  bool verify = doc["verify"] | false;
  String filename = doc["filename"] | "firmware.bin";
  
  // 打开 BIN 文件
  fs::File f = LittleFS.open("/" + filename, "r");
  if (!f) {
    server.send(404, "application/json", "{\"error\":\"file not found\"}");
    return;
  }
  
  uint32_t fileSize = f.size();
  uint32_t totalBlocks = (fileSize + 511) / 512;
  
  // 立即返回响应（烧录在后台同步执行，HTTP 会阻塞，但 WebSocket 实时推送进度）
  server.send(200, "application/json", 
              "{\"success\":true,\"total_blocks\":" + String(totalBlocks) + "}");
  
  g_state = STATE_BURNING;
  g_burn.total_blocks = totalBlocks;
  g_burn.current_block = 0;
  g_burn.percent = 0;
  g_burn.done = false;
  g_burn.error = "";
  
  // 执行烧录（同步阻塞 loop，但 WebSocket 在 burn 间隙被调用）
  burnFromLittleFS(&f, verify);
  
  f.close();
  
  // 烧录完成，复位 CC2530
  RunDUP();
  
  g_burn.done = true;
  g_state = STATE_IDLE;
  
  // 推送完成事件
  pushBurnProgress();
}

void burnFromLittleFS(fs::File *f, bool verify) {
  debug_init();
  uint8_t chip_id = read_chip_id();
  if (chip_id == 0) {
    g_burn.error = "chip not detected";
    return;
  }
  
  RunDUP();
  debug_init();
  chip_erase();
  RunDUP();
  debug_init();
  
  // 切换到外部晶振
  write_xdata_memory(DUP_CLKCONCMD, 0x80);
  while (read_xdata_memory(DUP_CLKCONSTA) != 0x80);
  
  uint8_t debug_config = 0x22;
  debug_command(CMD_WR_CONFIG, &debug_config, 1);
  
  uint32_t addr = 0;
  uint8_t buf[512];
  uint32_t blockIndex = 0;
  
  while (f->available()) {
    // 读 512 字节
    size_t read = f->read(buf, 512);
    if (read < 512) {
      // 不足 512，用 0xFF 填充
      memset(buf + read, 0xFF, 512 - read);
    }
    
    // 烧录
    write_flash_memory_block(buf, addr, 512);
    
    // 可选校验
    if (verify) {
      uint8_t bank = addr / (512 * 16);
      uint16_t offset = (addr % (512 * 16)) * 4;
      uint8_t read_data[512];
      read_flash_memory_block(bank, offset, 512, read_data);
      for (int i = 0; i < 512; i++) {
        if (read_data[i] != buf[i]) {
          g_burn.error = "verify failed at block " + String(blockIndex);
          return;
        }
      }
    }
    
    addr += 128;
    blockIndex++;
    g_burn.current_block = blockIndex;
    g_burn.percent = (blockIndex * 100) / g_burn.total_blocks;
    
    // 推送进度
    if (blockIndex % 4 == 0 || blockIndex == g_burn.total_blocks) {
      pushBurnProgress();
    }
    
    // 处理 WebSocket（避免前端断连）
    webSocket.loop();
    server.handleClient();
  }
}

void pushBurnProgress() {
  DynamicJsonDocument doc(256);
  doc["type"] = "burn_progress";
  doc["percent"] = g_burn.percent;
  doc["current_block"] = g_burn.current_block;
  doc["total_blocks"] = g_burn.total_blocks;
  doc["done"] = g_burn.done;
  doc["error"] = g_burn.error;
  
  String json;
  serializeJson(doc, json);
  webSocket.broadcastTXT(json);
}
```

#### 4.6.4 监控 API

```cpp
void handleMonitor() {
  if (g_state != STATE_IDLE) {
    server.send(409, "application/json", "{\"error\":\"busy\"}");
    return;
  }
  
  String body = server.arg("plain");
  DynamicJsonDocument doc(256);
  deserializeJson(doc, body);
  uint32_t baud = doc["baud"] | 115200;
  
  // 释放 Serial 给 CC2530 监控用
  Serial.flush();
  Serial.end();
  delay(100);
  Serial.begin(baud);  // 用新波特率，GPIO3 接收 CC2530 日志
  
  g_state = STATE_MONITORING;
  g_monitor_len = 0;
  
  server.send(200, "application/json", "{\"success\":true,\"baud\":" + String(baud) + "}");
  
  // 通知前端监控已开始
  DynamicJsonDocument notif(128);
  notif["type"] = "monitor_start";
  notif["baud"] = baud;
  String json;
  serializeJson(notif, json);
  webSocket.broadcastTXT(json);
}

void handleStop() {
  if (g_state == STATE_MONITORING) {
    // 恢复 Serial
    Serial.flush();
    Serial.end();
    delay(100);
    Serial.begin(115200);
  }
  
  g_state = STATE_IDLE;
  g_monitor_len = 0;
  
  server.send(200, "application/json", "{\"success\":true}");
  
  DynamicJsonDocument notif(128);
  notif["type"] = "monitor_stop";
  String json;
  serializeJson(notif, json);
  webSocket.broadcastTXT(json);
}
```

### 4.7 WiFi 连接策略

```cpp
void initWiFi() {
  if (g_config.wifi_ssid.length() == 0) {
    // 无配置，开 AP 模式
    WiFi.mode(WIFI_AP);
    WiFi.softAP("CCLoader-Setup", "12345678");
    Serial.println("AP mode: CCLoader-Setup, password: 12345678");
    Serial.print("AP IP: ");
    Serial.println(WiFi.softAPIP());
  } else {
    // 连接已配置的 WiFi
    WiFi.mode(WIFI_STA);
    WiFi.begin(g_config.wifi_ssid, g_config.wifi_password);
    Serial.printf("Connecting to %s", g_config.wifi_ssid.c_str());
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 30) {
      delay(500);
      Serial.print(".");
      attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println();
      Serial.print("Connected, IP: ");
      Serial.println(WiFi.localIP());
    } else {
      Serial.println("\nWiFi connect failed, switching to AP mode");
      WiFi.mode(WIFI_AP);
      WiFi.softAP("CCLoader-Fallback", "12345678");
      Serial.print("AP IP: ");
      Serial.println(WiFi.softAPIP());
    }
  }
}
```

### 4.8 配置文件

`/config.json` 格式：

```json
{
  "wifi_ssid": "MyWiFi",
  "wifi_password": "mypassword",
  "monitor_baud": 115200,
  "verify": false
}
```

### 4.9 WebSocket 事件

```cpp
void webSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      Serial.printf("WS[%u] disconnected\n", num);
      break;
    case WStype_CONNECTED:
      Serial.printf("WS[%u] connected\n", num);
      // 发送当前状态
      sendStatusToClient(num);
      break;
    case WStype_TEXT:
      // 处理前端命令（可选，主要用 HTTP）
      break;
  }
}
```

### 4.10 关键注意事项

1. **烧录时 WiFi 中断**：
   - CC Debug 协议对时序敏感，WiFi 中断可能导致烧录失败
   - 缓解：烧录时定期调用 `webSocket.loop()` 和 `server.handleClient()` 保持连接
   - 极端情况：烧录关键阶段（write_flash_memory_block）期间不调用任何 WiFi 相关函数

2. **GPIO3 串口复用**：
   - 空闲模式：Serial @ 115200（用于调试输出到 USB 串口监视器）
   - 监控模式：Serial @ CC2530 波特率（GPIO3 接收 CC2530 日志）
   - 切换时必须 `Serial.end()` → `Serial.begin(new_baud)`

3. **LittleFS 空间**：
   - NodeMCU 4MB Flash 默认 LittleFS 区域 1MB，可存 4 个 256KB BIN
   - 如需更多空间，在 platformio.ini 调整 `board_build.ldscript` 和 `board_build.f_flash`

4. **内存管理**：
   - 256KB BIN 不能一次性读入 RAM，必须分块（512 字节）从 LittleFS 读取
   - WebSocket 推送时缓冲区不超过 256 字节，避免内存溢出

5. **HTTP 上传大文件**：
   - ESP8266WebServer 默认上传缓冲区 1KB，需在 `handleUpload` 中流式写入 LittleFS
   - 上传超时设为 60 秒（256KB @ 100KB/s 约 2.5 秒，余量充足）

---

## 5. 前端 WebUI 规格

### 5.1 页面结构

单页应用（SPA），通过标签页切换功能：

```
┌─────────────────────────────────────────────────┐
│  CCLoader WebUI          [状态: 空闲] [IP: x.x] │
├─────────────────────────────────────────────────┤
│  [烧录]  [监控]  [设置]                          │
├─────────────────────────────────────────────────┤
│                                                 │
│  （根据选中标签显示不同内容）                     │
│                                                 │
└─────────────────────────────────────────────────┘
```

### 5.2 烧录页

```
┌─────────────────────────────────────────────────┐
│  烧录 CC2530                                     │
├─────────────────────────────────────────────────┤
│                                                 │
│  固件文件: [选择文件...]  DIYRuZRT.bin (256KB)   │
│                                                 │
│  [上传]                                          │
│                                                 │
│  已上传固件:                                     │
│  ○ DIYRuZRT.bin       256KB  2026-07-20  [删除] │
│  ○ Router_v1.hex      598KB  2026-07-19  [删除] │
│                                                 │
│  ─────────────────────────────────────────────  │
│                                                 │
│  选中: DIYRuZRT.bin                              │
│  [✓] 烧录后校验                                  │
│                                                 │
│  [开始烧录]                                      │
│                                                 │
│  ─────────────────────────────────────────────  │
│                                                 │
│  进度: [████████████░░░░░░░░] 60% (308/512)     │
│                                                 │
│  日志:                                           │
│  [10:30:01] Chip ID: 0xA5CC                     │
│  [10:30:02] Erasing chip...                     │
│  [10:30:05] Writing block 1/512                 │
│  [10:30:06] Writing block 2/512                 │
│  ...                                            │
│  [10:35:20] Burn complete!                      │
│                                                 │
└─────────────────────────────────────────────────┘
```

### 5.3 监控页

```
┌─────────────────────────────────────────────────┐
│  串口监控                                        │
├─────────────────────────────────────────────────┤
│                                                 │
│  波特率: [115200 ▼]    [开始监控]                │
│                                                 │
│  [暂停] [清空] [下载] [搜索: ___________]       │
│                                                 │
│  ─────────────────────────────────────────────  │
│  | [10:40:01.234] M1                            │
│  | [10:40:01.235] M2                            │
│  | [10:40:01.237] M3                            │
│  | [10:40:01.240] RAW BOOT                      │
│  | [10:40:01.242] [DIY] zclDIYRuZRT_Init done   │
│  | [10:40:01.245] [DIY] relay_state=0x00        │
│  | [10:40:02.100] [DIY] ZDO_STATE_CHANGE        │
│  | ...                                          │
│  |                                               │
│  | (自动滚动到最新)                              │
│  ─────────────────────────────────────────────  │
│                                                 │
│  接收: 1234 字节  |  连接: 已建立  |  [停止]     │
│                                                 │
└─────────────────────────────────────────────────┘
```

### 5.4 设置页

```
┌─────────────────────────────────────────────────┐
│  设置                                            │
├─────────────────────────────────────────────────┤
│                                                 │
│  WiFi 配置:                                      │
│    SSID:     [MyWiFi_____________________]      │
│    密码:     [********____________________]     │
│    [保存] [测试连接]                              │
│                                                 │
│  监控默认:                                       │
│    波特率:   [115200 ▼]                         │
│    [保存]                                        │
│                                                 │
│  烧录默认:                                       │
│    [✓] 烧录后校验                                │
│    [保存]                                        │
│                                                 │
│  设备信息:                                       │
│    固件版本: CCLoader-WebUI v1.0                 │
│    运行时长: 1小时23分                            │
│    WiFi 信号: -55 dBm (良好)                     │
│    IP 地址: 192.168.1.100                        │
│    Flash 使用: 1.2MB / 4MB                       │
│                                                 │
│  [重启 ESP8266]                                  │
│                                                 │
└─────────────────────────────────────────────────┘
```

### 5.5 前端技术要点

```javascript
// app.js 核心逻辑

let ws = null;
let monitorPaused = false;
let burnLogs = [];

// WebSocket 连接
function connectWebSocket() {
  const wsUrl = `ws://${location.host}:81`;
  ws = new WebSocket(wsUrl);
  
  ws.onopen = () => {
    console.log('WebSocket connected');
    updateStatus('已连接');
  };
  
  ws.onmessage = (event) => {
    const msg = JSON.parse(event.data);
    switch (msg.type) {
      case 'burn_progress':
        updateBurnProgress(msg);
        break;
      case 'monitor_data':
        if (!monitorPaused) {
          appendMonitorData(atob(msg.data));
        }
        break;
      case 'monitor_start':
        onMonitorStart(msg.baud);
        break;
      case 'monitor_stop':
        onMonitorStop();
        break;
      case 'status':
        updateStatus(msg);
        break;
    }
  };
  
  ws.onclose = () => {
    console.log('WebSocket disconnected, reconnecting...');
    setTimeout(connectWebSocket, 2000);
  };
}

// 上传 BIN
async function uploadFirmware(file) {
  const formData = new FormData();
  formData.append('file', file);
  
  const response = await fetch('/api/upload', {
    method: 'POST',
    body: formData
  });
  
  const result = await response.json();
  if (result.success) {
    refreshFileList();
  } else {
    alert('上传失败: ' + result.error);
  }
}

// 开始烧录
async function startBurn(filename, verify) {
  const response = await fetch('/api/burn', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ filename, verify })
  });
  
  const result = await response.json();
  if (!result.success) {
    alert('烧录启动失败: ' + result.error);
  }
  // 进度通过 WebSocket 推送
}

// 开始监控
async function startMonitor(baud) {
  const response = await fetch('/api/monitor', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ baud })
  });
  
  const result = await response.json();
  if (!result.success) {
    alert('监控启动失败: ' + result.error);
  }
  // 数据通过 WebSocket 推送
}

// 更新烧录进度
function updateBurnProgress(msg) {
  document.getElementById('burn-progress-bar').style.width = msg.percent + '%';
  document.getElementById('burn-progress-text').textContent = 
    `${msg.percent}% (${msg.current_block}/${msg.total_blocks})`;
  
  if (msg.error) {
    appendBurnLog(`[错误] ${msg.error}`);
  }
  if (msg.done) {
    appendBurnLog('[完成] 烧录成功');
    // 自动切换到监控页
    setTimeout(() => switchTab('monitor'), 1000);
  }
}

// 追加监控数据
function appendMonitorData(data) {
  const logArea = document.getElementById('monitor-log');
  const lines = data.split('\n');
  
  lines.forEach((line, i) => {
    if (line || i < lines.length - 1) {
      const time = new Date().toLocaleTimeString('zh-CN', { hour12: false }) + 
                   '.' + String(Date.now() % 1000).padStart(3, '0');
      const div = document.createElement('div');
      div.className = 'log-line';
      div.textContent = `[${time}] ${line}`;
      logArea.appendChild(div);
    }
  });
  
  // 自动滚动到底部
  logArea.scrollTop = logArea.scrollHeight;
  
  // 限制日志行数（避免内存溢出）
  while (logArea.children.length > 5000) {
    logArea.removeChild(logArea.firstChild);
  }
}
```

### 5.6 样式要求

- 响应式布局，适配桌面和手机屏幕
- 暗色主题（适合长时间监控）
- 进度条用绿色渐变
- 日志区用等宽字体（如 Consolas / Monaco）
- 状态指示器用颜色区分（绿色=空闲，黄色=烧录中，蓝色=监控中）

---

## 6. 验收标准

### 6.1 功能验收

1. **WiFi 连接**：
   - ESP8266 上电后 10 秒内连接已配置的 WiFi
   - 连接失败自动切换 AP 模式（SSID: `CCLoader-Fallback`）
   - 浏览器能访问 ESP8266 IP 打开 WebUI

2. **文件上传**：
   - 上传 256KB BIN 文件，3 秒内完成
   - 上传后文件出现在"已上传固件"列表
   - 可删除已上传的文件

3. **烧录功能**：
   - 选择 BIN 后点"开始烧录"，3 分钟内完成 256KB 烧录
   - 进度条实时更新（每 4 块推送一次，约每 0.5 秒）
   - 烧录完成后 CC2530 自动复位运行
   - 勾选"校验"时，烧录后逐块回读比对
   - 烧录失败时显示错误信息

4. **监控功能**：
   - 选择波特率后点"开始监控"，进入监控状态
   - 按 CC2530 的 RESET，能在 < 200ms 内看到第一行日志
   - 日志带毫秒级时间戳
   - 支持暂停/清空/下载（保存为 .log 文件）
   - 支持关键字搜索过滤

5. **模式切换**：
   - 烧录完成后自动跳转到监控页
   - 监控中可随时点"停止"回到空闲
   - 空闲时可立即开始新的烧录或监控
   - 连续切换 10 次不卡死

6. **设置**：
   - 可配置 WiFi SSID 和密码
   - 可配置默认监控波特率
   - 可配置默认是否校验
   - 保存后重启生效

### 6.2 性能验收

1. **烧录速度**：
   - 256KB BIN 烧录时间 ≤ 3 分钟（无校验）
   - 256KB BIN 烧录时间 ≤ 5 分钟（有校验）

2. **监控延迟**：
   - CC2530 发送一行日志到 WebUI 显示的延迟 < 100ms
   - 按 RESET 到看到第一行日志的延迟 < 200ms

3. **稳定性**：
   - 连续监控 30 分钟无断连、无卡死
   - 烧录过程中 WiFi 偶发断连不影响烧录（自动重连）
   - CC2530 频繁复位（每 2 秒一次）不导致 ESP8266 崩溃

4. **内存**：
   - 运行时堆内存剩余 ≥ 10KB
   - 无内存泄漏（连续运行 24 小时堆内存稳定）

### 6.3 兼容性

1. **浏览器**：
   - Chrome 90+ / Firefox 88+ / Edge 90+ / Safari 14+
   - 手机浏览器（iOS Safari / Android Chrome）

2. **平台**：
   - Windows 10/11
   - macOS 11+
   - Linux（Ubuntu 20.04+）
   - 手机（iOS 14+ / Android 10+）

---

## 7. 交付物

### 7.1 ESP8266 固件

- 改造后的 `CCLoader.ino`（含 HTTP 服务器、WebSocket、烧录、监控状态机）
- `platformio.ini`（含依赖库配置）
- `data/` 目录（WebUI 静态文件：index.html / style.css / app.js）
- 编译后的固件 `.bin`（可直接 OTA 或 USB 烧录到 NodeMCU）

### 7.2 前端

- `data/index.html`：WebUI 主页面
- `data/style.css`：样式
- `data/app.js`：前端逻辑
- 可选：`data/favicon.ico`

### 7.3 文档

- `README.md`：使用说明（接线、配置、操作流程）
- `REQUIREMENT_WEBUI.md`：本文档（需求规格）

---

## 8. 风险与备选方案

### 8.1 风险 1：烧录时 WiFi 中断导致前端断连

**现象**：烧录过程中 WebUI 显示"连接断开"，但烧录仍在后台继续

**原因**：CC Debug 协议是同步阻塞的 bit-bang，期间无法响应 WiFi 事件

**缓解**：
- 烧录时每 4 块（约 0.5 秒）调用一次 `webSocket.loop()` 和 `server.handleClient()`
- 前端 WebSocket 设 10 秒超时，断连后自动重连
- 烧录完成后推送最终状态，前端重连后能获取结果

### 8.2 风险 2：LittleFS 空间不足

**现象**：上传多个 BIN 后空间不足

**缓解**：
- 默认 LittleFS 分区 1MB，可存 4 个 256KB BIN
- 上传时检查剩余空间，不足时拒绝并提示删除旧文件
- 必要时在 platformio.ini 调整分区表

### 8.3 风险 3：GPIO3 复用冲突

**现象**：进入监控模式后 ESP8266 重启或收不到数据

**原因**：Serial.end() 后 GPIO3 状态不确定

**缓解**：
- 进入监控模式前 `Serial.end()` + `delay(100)` + `pinMode(3, INPUT)`
- 退出监控模式后 `Serial.begin(115200)` 恢复
- 如果硬件 UART 不稳定，改用 SoftwareSerial（但 115200 不稳定，需降到 38400）

### 8.4 风险 4：大文件上传失败

**现象**：上传 256KB BIN 时中断或超时

**缓解**：
- 分块上传（每块 1KB）
- 上传超时设为 60 秒
- 前端显示上传进度
- 失败后可断点续传（记录已上传字节数）

---

## 9. 开发环境

### 9.1 ESP8266 固件

- PlatformIO（推荐）
- ESP8266 Arduino Core 4.2.0
- 依赖库：WebSockets、ArduinoJson、LittleFS（内置）

### 9.2 前端

- 原生 HTML/CSS/JS（无框架）
- 可选：用 VS Code 编辑，浏览器调试

### 9.3 测试环境

- NodeMCU ESP8266 (ESP-12E)
- CC2530 商业模块（已有 UART0_TX/UART0_RX 引出）
- Windows 10/11 + Chrome
- WiFi 路由器（2.4GHz）

### 9.4 编译命令

```bash
# 编译固件
pio run

# 烧录固件到 ESP8266
pio run -t upload --upload-port COMx

# 上传 LittleFS 文件系统（WebUI 静态文件）
pio run -t uploadfs --upload-port COMx

# 串口监视器
pio device monitor -p COMx -b 115200
```

---

## 10. 参考资料

- CCLoader 原项目：https://github.com/RedBearLab/CCLoader
- ESP8266WebServer 文档：https://github.com/esp8266/Arduino/tree/master/libraries/ESP8266WebServer
- WebSocketsServer 库：https://github.com/Links2004/arduinoWebSockets
- LittleFS 文档：https://arduino-esp8266.readthedocs.io/en/latest/filesystem.html
- ArduinoJson 文档：https://arduinojson.org/
- CC2530 UART 寄存器：CC2530 User Guide (SWRU191)

---

## 附录 A：原有 CCLoader.ino 关键代码位置

文件：`D:\VC\CCLoader\Arduino\CCLoader\CCLoader.ino`

| 函数 | 行号 | 用途 | 改造说明 |
|---|---|---|---|
| `setup()` | 523 | 初始化引脚 + Serial | 增加 WiFi、HTTP、WS、LittleFS 初始化 |
| `loop()` | 532 | 烧录状态机 | 改为三态状态机，增加 HTTP/WS 处理 |
| `debug_init()` | 234 | 进入 CC Debug 模式 | 保持不变 |
| `read_chip_id()` | 261 | 读取芯片 ID | 保持不变 |
| `chip_erase()` | 323 | 全片擦除 | 保持不变 |
| `write_flash_memory_block()` | 462 | 写 Flash 块 | 保持不变，数据源从 Serial 改为 LittleFS |
| `read_flash_memory_block()` | 426 | 读 Flash 块 | 保持不变，用于校验 |
| `RunDUP()` | 496 | 复位 CC2530 | 保持不变 |

## 附录 B：API 请求/响应示例

### B.1 上传 BIN 文件

**请求**：
```
POST /api/upload
Content-Type: multipart/form-data; boundary=----WebKitFormBoundary

------WebKitFormBoundary
Content-Disposition: form-data; name="file"; filename="DIYRuZRT.bin"
Content-Type: application/octet-stream

<256KB 二进制数据>
------WebKitFormBoundary--
```

**响应**：
```json
{
  "success": true,
  "filename": "DIYRuZRT.bin",
  "size": 262144
}
```

### B.2 开始烧录

**请求**：
```
POST /api/burn
Content-Type: application/json

{
  "filename": "DIYRuZRT.bin",
  "verify": false
}
```

**响应**：
```json
{
  "success": true,
  "total_blocks": 512
}
```

**后续 WebSocket 推送**：
```json
{"type": "burn_progress", "percent": 25, "current_block": 128, "total_blocks": 512, "done": false, "error": ""}
{"type": "burn_progress", "percent": 50, "current_block": 256, "total_blocks": 512, "done": false, "error": ""}
{"type": "burn_progress", "percent": 100, "current_block": 512, "total_blocks": 512, "done": true, "error": ""}
```

### B.3 开始监控

**请求**：
```
POST /api/monitor
Content-Type: application/json

{
  "baud": 115200
}
```

**响应**：
```json
{
  "success": true,
  "baud": 115200
}
```

**后续 WebSocket 推送**：
```json
{"type": "monitor_start", "baud": 115200}
{"type": "monitor_data", "data": "TTENClMyCk0zClJBVyBCT09UXHINCg=="}
{"type": "monitor_data", "data": "W0RJWV0gemNsRElZUnpSVF9Jbml0IGRvbmUNCg=="}
```

（data 字段为 Base64 编码的 UTF-8 文本）

### B.4 停止监控

**请求**：
```
POST /api/stop
```

**响应**：
```json
{
  "success": true
}
```

**WebSocket 推送**：
```json
{"type": "monitor_stop"}
```

### B.5 获取状态

**请求**：
```
GET /api/status
```

**响应**：
```json
{
  "state": "idle",
  "burn": {
    "percent": 0,
    "current_block": 0,
    "total_blocks": 0,
    "done": false,
    "error": ""
  },
  "monitor": {
    "active": false,
    "baud": 115200,
    "bytes_received": 0
  },
  "wifi": {
    "ssid": "MyWiFi",
    "ip": "192.168.1.100",
    "rssi": -55
  },
  "uptime": 3600
}
```

---

## 附录 C：LittleFS 分区规划

NodeMCU ESP-12E 默认 4MB Flash 分区：

| 区域 | 嵌套偏移 | 大小 | 用途 |
|---|---|---|---|
| 0x000000 - 0x0BFFFF | - | 768KB | ESP8266 固件（含 CCLoader.ino） |
| 0x0C0000 - 0x0FFFFF | - | 256KB | LittleFS（WebUI 静态文件 + 配置 + BIN） |
| 0x100000 - 0x2FBFFF | - | 2MB | LittleFS 扩展（可选，存更多 BIN） |
| 0x2FC000 - 0x2FFFFF | - | 16KB | EEPROM（WiFi 配置备选） |
| 0x300000 - 0x3FBFFF | - | 1008KB | OTA 备用区（可选） |
| 0x3FE000 - 0x3FFFFF | - | 8KB | RF 校准数据 |

如需更多 BIN 存储空间，在 `platformio.ini` 配置：
```ini
board_build.ldscript = eagle.flash.4m3m.ld  # 3MB LittleFS
```

---

## 附录 D：WebUI 截图示意（ASCII）

### 烧录页

```
┌──────────────────────────────────────────────────────┐
│  CCLoader WebUI          [空闲] [192.168.1.100]      │
├──────────────────────────────────────────────────────┤
│  [烧录]  [监控]  [设置]                               │
├──────────────────────────────────────────────────────┤
│                                                      │
│  固件文件: [选择文件...] DIYRuZRT.bin (256KB)        │
│  [上传]                                               │
│                                                      │
│  已上传固件:                                          │
│  ● DIYRuZRT.bin        256KB  2026-07-20 14:30 [删除]│
│  ○ Router_v1.bin       256KB  2026-07-19 10:15 [删除]│
│                                                      │
│  ──────────────────────────────────────────────────  │
│  选中: DIYRuZRT.bin                                   │
│  [ ] 烧录后校验                                       │
│                                                      │
│  [开始烧录]                                           │
│                                                      │
│  进度: [████████████████░░░░░░░░░░░░] 60% (308/512) │
│                                                      │
│  日志:                                                │
│  [14:30:01] Chip ID: 0xA5CC                          │
│  [14:30:02] Erasing chip...                          │
│  [14:30:05] Writing block 1/512                      │
│  [14:30:06] Writing block 2/512                      │
│  ...                                                 │
│                                                      │
└──────────────────────────────────────────────────────┘
```

### 监控页

```
┌──────────────────────────────────────────────────────┐
│  CCLoader WebUI          [监控中] [192.168.1.100]    │
├──────────────────────────────────────────────────────┤
│  [烧录]  [监控]  [设置]                               │
├──────────────────────────────────────────────────────┤
│                                                      │
│  波特率: [115200 ▼]     [开始监控]                   │
│                                                      │
│  [暂停] [清空] [下载] [搜索: M1_____________]        │
│                                                      │
│  ┌────────────────────────────────────────────────┐ │
│  │ [14:40:01.234] M1                              │ │
│  │ [14:40:01.235] M2                              │ │
│  │ [14:40:01.237] M3                              │ │
│  │ [14:40:01.240] RAW BOOT                        │ │
│  │ [14:40:01.242] [DIY] zclDIYRuZRT_Init done     │ │
│  │ [14:40:01.245] [DIY] relay_state=0x00          │ │
│  │ [14:40:02.100] [DIY] ZDO_STATE_CHANGE          │ │
│  │ [14:40:02.150] [DIY] ZDO_STATE_CHANGE          │ │
│  │ ...                                            │ │
│  │                                                │ │
│  │ (自动滚动到最新)                               │ │
│  └────────────────────────────────────────────────┘ │
│                                                      │
│  接收: 1234 字节 | 连接: 已建立 | [停止]             │
│                                                      │
└──────────────────────────────────────────────────────┘
```
