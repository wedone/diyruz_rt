/*
 * preinclude_boot.h - Bootloader 专用预编译配置
 *
 * 用于 Bootloader 子工程，编译 CC2530 SB Bootloader（hal_ota.c 中的 HAL_OTA_BOOT_CODE 分支）
 * 输出 bootloader.hex（2KB，占用 0x0000~0x07FF）
 *
 * 使用方式：
 *   在 IAR Bootloader 工程的 PreInclude 配置中指定此文件
 */

// 编译为 Bootloader 代码（hal_ota.c 中的 main/dl2rc/crcCalc 分支）
#define HAL_OTA_BOOT_CODE TRUE

// Bootloader 使用内部 Flash 存储镜像（不使用外部 SPI Flash）
#define HAL_OTA_XNV_IS_INT FALSE

// 协议栈版本（OTA 镜像头用）
#define OTA_STACK_VER_PRO  0x0002

// 通道掩码（与主应用一致）
#undef DEFAULT_CHANLIST
#define DEFAULT_CHANLIST 0x07FFF800

// 引入板级配置
#include "hal_board_cfg_DIYRuZRT.h"
