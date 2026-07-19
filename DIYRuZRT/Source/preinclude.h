#define SECURE 1
#define TC_LINKKEY_JOIN
#define NV_INIT
#define NV_RESTORE

// ====== 调试串口开关 ======
// 启用 ZTOOL_P1 后，MT_UART 会用 UART0 Alt-1 (P0_2=RX, P0_3=TX) 初始化串口
// 由于 LED3/LED4 当前占用 P0_2/P0_3，启用本开关后需在 hal_board_cfg_DIYRuZRT.h
// 中通过 DIY_DEBUG_UART 屏蔽 LED3/LED4，把 P0_2/P0_3 让给 UART
// 调试完成后将 ZTOOL_P1 改回 xZTOOL_P1 并删除 DIY_DEBUG_UART 即可恢复
#define ZTOOL_P1
#define DIY_DEBUG_UART

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

// OTA 预留（当前关闭，后续版本启用时改为 1 并实现 OTA Cluster 服务端回调）
// OTA 分区规划（CC2530F256，256KB Flash）：
//   Boot Loader:  8KB  (0x0000~0x1FFF, 4 pages)
//   用户代码:    114KB  (0x2000~0x1FFFF, ~57 pages)
//   OTA 预留:   ~120KB  (0x20000~0x3DFFF, ~60 pages)
//   NV:          12KB  (0x3E000~0x3FFFF, 6 pages)
//   Lock Bits:    2KB  (0x3FE00~0x3FFFF, 1 page)
#define ZCL_OTA 0

#include "hal_board_cfg_DIYRuZRT.h"