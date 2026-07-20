#define SECURE 1
#define TC_LINKKEY_JOIN
#define NV_INIT
#define NV_RESTORE

// ====== 调试串口开关 ======
// 启用 ZTOOL_P1 后，MT_UART 会用 UART0 Alt-1 (P0_2=RX, P0_3=TX) 初始化串口
// Z-Stack 通过 MT_UART 输出系统日志（ZDO 状态、AF 数据等）
// 由于 LED3/LED4 占用 P0_2/P0_3，启用 DIY_DEBUG_UART 让 LED3/LED4 让出引脚给 UART
#define ZTOOL_P1
#define DIY_DEBUG_UART

// DIY_DEBUG_UART 模式下缩小 UART DMA 缓冲以节省 XDATA
// TX 缓冲需容纳多行诊断日志 + MT 二进制帧，128 字节较安全
#if defined(DIY_DEBUG_UART)
#define MT_UART_TX_BUFF_MAX  128
#define MT_UART_RX_BUFF_MAX  64
// 统一使用 115200，与 Z-Stack MT_UART 默认波特率一致
#define MT_UART_DEFAULT_BAUDRATE  HAL_UART_BR_115200
// 禁用硬件流控（RTS/CTS）：CC2530 模块未接 CTS 引脚，启用流控会导致 UART 发送卡死
// MT_UART.h 中默认值为 TRUE，这里显式覆盖为 FALSE
#define MT_UART_DEFAULT_OVERFLOW  FALSE
#endif

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

// ====== PA/LNA 前端放大驱动 ======
// 启用后固件配置 CC2530+CC2591/RFX2401C 的 PA(发射放大)+LNA(接收放大)
// 适用模块：SE ZB Module (CC2530+RFX2401C) 等带前端放大芯片的模块
// 注意：86 开关裸模块无放大芯片，最终产品版本需移除此定义
// 引脚分配：P0_7=HGM(高增益模式), P1_0=LNA_EN, P1_1=PA_EN
// 与 TOUCH4(P0_7) 和 RELAY1(P1_0) 冲突，仅在带放大模块测试时启用
//
// 测试记录：
// - HAL_PA_LNA + OBSSEL P1_0/P1_1 → 0 beacon（SE 模块引脚推断错误？）
// - 下一步：禁用 HAL_PA_LNA 测试裸 CC2530 模式
//#define HAL_PA_LNA

// OTA 预留（当前关闭，后续版本启用时改为 1 并实现 OTA Cluster 服务端回调）
// OTA 分区规划（CC2530F256，256KB Flash）：
//   Boot Loader:  8KB  (0x0000~0x1FFF, 4 pages)
//   用户代码:    114KB  (0x2000~0x1FFFF, ~57 pages)
//   OTA 预留:   ~120KB  (0x20000~0x3DFFF, ~60 pages)
//   NV:          12KB  (0x3E000~0x3FFFF, 6 pages)
//   Lock Bits:    2KB  (0x3FE00~0x3FFFF, 1 page)
#define ZCL_OTA 0

#include "hal_board_cfg_DIYRuZRT.h"