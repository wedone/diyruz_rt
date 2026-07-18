#define SECURE 1
#define TC_LINKKEY_JOIN
#define NV_INIT
#define NV_RESTORE
#define xZTOOL_P1
#define MT_TASK
#define MT_APP_FUNC
#define MT_SYS_FUNC
#define MT_ZDO_FUNC
#define MT_ZDO_MGMT
#define MT_APP_CNF_FUNC
#define LEGACY_LCD_DEBUG
//#define LCD_SUPPORTED DEBUG
#define MULTICAST_ENABLED FALSE
#define ZCL_READ
#define ZCL_WRITE
#define ZCL_BASIC
#define ZCL_IDENTIFY
#define ZCL_SCENES
#define ZCL_GROUPS
#define ZCL_ON_OFF
#define ZCL_REPORTING_DEVICE

#define DISABLE_GREENPOWER_BASIC_PROXY

// 通道掩码：覆盖所有 Zigbee 2.4GHz 通道（11~26）
// 先 #undef 避免 f8wConfig.cfg 中 -DDEFAULT_CHANLIST 的重定义警告
#undef DEFAULT_CHANLIST
#define DEFAULT_CHANLIST 0x07FFF800

// 4路智能开关：不复用 Sonoff 配置（避免 LED1=P0_7/LED2=P1_0 与 TOUCH4/RELAY1 引脚冲突）
// OSC32K_CRYSTAL_INSTALLED 在 hal_board_cfg_DIYRuZRT.h 中独立定义为 FALSE

// OTA 启用（feature/ota 分支）
// OTA 分区规划（CC2530F256，256KB Flash）：
//   Bootloader:   2KB  (0x0000~0x07FF, 1 page, ota-boot.xcl)
//   应用代码:   ~248KB  (0x0800~0x3DFFF, ota.xcl)
//     - 应用主体（含 OTA 缓存区）
//   NV:          12KB  (0x3E000~0x3F7FF, 6 pages)
//   Lock Bits:    2KB  (0x3F800~0x3FFFF, 1 page)
// HAL_OTA_DL_MAX = 0x40000 - (6+2)*2KB = 248KB
// HAL_OTA_DL_SIZE = 248KB / 2 = 124KB（应用）
// HAL_OTA_DL_OSET = 124KB（OTA 缓存区起始偏移）
#define ZCL_OTA 1

// OTA 客户端模式：设备作为 OTA 客户端，接收 Z2M 推送的升级
#define OTA_CLIENT TRUE

// OTA 镜像标识（Z2M OTA index 匹配用）
#define OTA_MANUFACTURER_ID  0x115F  // DIYRuZ 自定义制造商 ID
#define OTA_TYPE_ID          0x0004  // 4 路开关镜像类型

#include "hal_board_cfg_DIYRuZRT.h"