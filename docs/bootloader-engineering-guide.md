# Bootloader 工程创建指南

本文档说明如何在 IAR EW8051 中创建 CC2530 SB Bootloader 子工程，用于编译 OTA Bootloader。

## 背景

OTA 升级需要两个独立的固件：
1. **Bootloader**（2KB，0x0000~0x07FF）：上电后校验应用 CRC，决定是否跳转到应用或等待 OTA 升级
2. **OTA 应用**（0x0800~）：包含完整 Zigbee 协议栈和应用逻辑

Bootloader 源码已在 [hal_ota.c](../Components/hal/target/CC2530EB/hal_ota.c) 中，通过 `HAL_OTA_BOOT_CODE=TRUE` 编译开关激活 `main()` / `dl2rc()` / `crcCalc()` 函数。

## 已就绪的资源

| 资源 | 路径 | 说明 |
|------|------|------|
| Bootloader 源码 | `Components/hal/target/CC2530EB/hal_ota.c` | TI 官方 SB Bootloader 实现 |
| Bootloader 链接器 | `Tools/CC2530DB/ota-boot.xcl` | `_CODE_START=0x0000`, `_CODE_END=0x07FF` |
| Bootloader PreInclude | `DIYRuZRT/Source/preinclude_boot.h` | 定义 `HAL_OTA_BOOT_CODE=TRUE` |
| 协议栈配置 | `Tools/CC2530DB/f8w2530.cfg` | 最小配置（无协议栈） |

## 创建步骤（在 IAR 中操作）

### 1. 新建工程

1. 打开 IAR EW8051
2. `Project` → `Create New Project...`
3. Toolchain: `8051`
4. Project template: `Empty project`
5. 保存到 `DIYRuZRT/Bootloader/CC2530DB/Bootloader.ewp`

### 2. 添加源文件

只添加以下文件：
- `$PROJ_DIR$\..\..\Components\hal\target\CC2530EB\hal_ota.c`

### 3. 配置 General Options

- **Device**: `CC2530F256`
- **Data model**: `Near`
- **Calling convention**: `XDATA reentrant`
- **Code model**: `Banked`

### 4. 配置 C/C++ Compiler

#### Preprocessor
- **Defined symbols**:
  ```
  HAL_OTA_BOOT_CODE=TRUE
  ```
- **PreInclude file**:
  ```
  $PROJ_DIR$\..\Source\preinclude_boot.h
  ```

#### Include paths
```
$PROJ_DIR$
$PROJ_DIR$\..\Source
$PROJ_DIR$\..\..\Source
$PROJ_DIR$\..\..\ZMain\TI2530DB
$PROJ_DIR$\..\..\Components\hal\include
$PROJ_DIR$\..\..\Components\hal\target\CC2530EB
$PROJ_DIR$\..\..\Components\osal\include
$PROJ_DIR$\..\..\Components\stack\af
$PROJ_DIR$\..\..\Components\stack\zcl
$PROJ_DIR$\..\..\Components\stack\nwk
$PROJ_DIR$\..\..\Components\stack\sys
$PROJ_DIR$\..\..\Components\stack\zdo
$PROJ_DIR$\..\..\Components\services\saddr
```

#### Extra Options
```
-f $PROJ_DIR$\..\..\Tools\CC2530DB\f8w2530.cfg
```

### 5. 配置 Linker

- **Override default**: 勾选
- **Linker command file**: `$PROJ_DIR$\..\..\Tools\CC2530DB\ota-boot.xcl`

### 6. 配置 Output Converter

- **Output file**: `Bootloader`
- **Format**: `Intel extended hex`
- **Output directory**: `$PROJ_DIR$\BootloaderEB\Exe`

### 7. 编译

`Project` → `Rebuild All`

输出文件：`DIYRuZRT/Bootloader/CC2530DB/BootloaderEB/Exe/Bootloader.hex`（约 2KB）

## 合并固件

使用 [scripts/merge_hex.py](../scripts/merge_hex.py) 合并 Bootloader 和 OTA 应用：

```bash
python scripts/merge_hex.py \
  DIYRuZRT/Bootloader/CC2530DB/BootloaderEB/Exe/Bootloader.hex \
  DIYRuZRT/CC2530DB/RouterEB/Exe/DIYRuZRT.hex \
  DIYRuZRT/CC2530DB/RouterEB/Exe/DIYRuZRT_ota_combined.hex
```

## 验证

合并后的 `DIYRuZRT_ota_combined.hex` 应包含：
- 0x0000~0x07FF: Bootloader
- 0x0800~: OTA 应用固件

使用 SmartRF Flash Programmer 2 烧录时：
1. 勾选 `Erase Entire Flash`
2. 烧录 `DIYRuZRT_ota_combined.hex`
3. 设备上电后 Bootloader 校验应用 CRC，通过后跳转到 0x0800 执行应用

## 注意事项

1. **HAL_OTA_BOOT_CODE 必须为 TRUE**：否则 hal_ota.c 会编译为应用层 OTA 驱动，而非 Bootloader
2. **不要包含其他源文件**：Bootloader 是独立的裸机程序，不依赖 OSAL、Z-Stack 等
3. **链接器必须用 ota-boot.xcl**：不能用 f8w2530.xcl 或 ota.xcl
4. **输出文件大小约 2KB**：如果超过 2KB，说明编译配置有误
