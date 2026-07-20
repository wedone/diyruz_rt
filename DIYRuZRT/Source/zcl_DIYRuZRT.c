#include "ZComDef.h"
#include "OSAL.h"
#include "AF.h"
#include "ZDApp.h"
#include "ZDObject.h"
#include "MT_SYS.h"

#include "nwk_util.h"

#include "zcl.h"
#include "zcl_general.h"
#include "zcl_ha.h"
#include "zcl_diagnostic.h"
#include "zcl_DIYRuZRT.h"

#include "bdb.h"
#include "bdb_interface.h"
#include "gp_interface.h"

#include "onboard.h"

/* HAL */
#include "hal_lcd.h"
#include "hal_led.h"
#include "hal_key.h"
#include "hal_drivers.h"
#include "hal_board_cfg_DIYRuZRT.h"  /* PUSH/RELAY/TOUCH 宏定义（确保在 preinclude 机制未生效时仍可用） */

#if defined(DIY_DEBUG_UART)
#include "hal_uart.h"
/* 调试串口日志：通过 UART0 (P0_2 RX / P0_3 TX) 输出文本日志
 * 依赖 MT_UART 在 ZTOOL_P1 启用后已对 UART0 完成 HalUARTOpen(115200, 8N1)
 * 业务代码只需调用 HalUARTWrite 即可，无需再次初始化
 * 注意：必须禁用 MT_UART_DEFAULT_OVERFLOW（硬件流控），否则 CTS 悬空会导致 UART 发送卡死 */
#define DIY_LOG_PORT  HAL_UART_PORT_0

static void diy_log_str(const char *s)
{
  uint8 len = 0;
  while (s[len]) len++;
  if (len) HalUARTWrite(DIY_LOG_PORT, (uint8 *)s, len);
}

static void diy_log_u8_dec(uint8 v)
{
  uint8 buf[3];
  uint8 i = 0;
  if (v >= 100) { buf[i++] = (uint8)('0' + (v / 100)); v %= 100; }
  if (v >= 10 || i > 0) { buf[i++] = (uint8)('0' + (v / 10)); v %= 10; }
  buf[i++] = (uint8)('0' + v);
  HalUARTWrite(DIY_LOG_PORT, buf, i);
}

static void diy_log_u8_hex(uint8 v)
{
  uint8 buf[2];
  uint8 hi = (v >> 4) & 0x0F;
  uint8 lo = v & 0x0F;
  buf[0] = (hi < 10) ? (uint8)('0' + hi) : (uint8)('A' + hi - 10);
  buf[1] = (lo < 10) ? (uint8)('0' + lo) : (uint8)('A' + lo - 10);
  HalUARTWrite(DIY_LOG_PORT, buf, 2);
}

static void diy_log_line(const char *s)
{
  diy_log_str(s);
  HalUARTWrite(DIY_LOG_PORT, (uint8 *)"\r\n", 2);
}

#define DIY_LOG(s)          diy_log_line(s)
#define DIY_LOG_STR(s)      diy_log_str(s)
#define DIY_LOG_U8(v)       diy_log_u8_dec(v)
#define DIY_LOG_HEX(v)      diy_log_u8_hex(v)
#else
/* 非调试模式：所有日志宏编译为空，零运行时开销 */
#define DIY_LOG(s)
#define DIY_LOG_STR(s)
#define DIY_LOG_U8(v)
#define DIY_LOG_HEX(v)
#endif

// Идентификатор задачи нашего приложения
byte zclDIYRuZRT_TaskID;

// Состояние сети
devStates_t zclDIYRuZRT_NwkState = DEV_INIT;

// Состояние кнопок
static uint8 halKeySavedKeys;
// 触摸按键上次状态（Task 5 使用）
static uint8 halKeySavedTouch;

// 4路继电器状态（bit0~3 对应 EP1~4，1=ON, 0=OFF）
uint8 RELAY_STATE = 0;

// Структура для отправки отчета
afAddrType_t zclDIYRuZRT_DstAddr;
// Номер сообщения
uint8 SeqNum = 0;

static void zclDIYRuZRT_HandleKeys( byte shift, byte keys );
static void zclDIYRuZRT_BasicResetCB( void );
static void zclDIYRuZRT_ProcessIdentifyTimeChange( uint8 endpoint );

static void zclDIYRuZRT_ProcessCommissioningStatus(bdbCommissioningModeMsg_t *bdbCommissioningModeMsg);

// Функции обработки входящих сообщений ZCL Foundation команд/ответов
static void zclDIYRuZRT_ProcessIncomingMsg( zclIncomingMsg_t *msg );
#ifdef ZCL_READ
static uint8 zclDIYRuZRT_ProcessInReadRspCmd( zclIncomingMsg_t *pInMsg );
#endif
#ifdef ZCL_WRITE
static uint8 zclDIYRuZRT_ProcessInWriteRspCmd( zclIncomingMsg_t *pInMsg );
#endif
static uint8 zclDIYRuZRT_ProcessInDefaultRspCmd( zclIncomingMsg_t *pInMsg );
#ifdef ZCL_DISCOVER
static uint8 zclDIYRuZRT_ProcessInDiscCmdsRspCmd( zclIncomingMsg_t *pInMsg );
static uint8 zclDIYRuZRT_ProcessInDiscAttrsRspCmd( zclIncomingMsg_t *pInMsg );
static uint8 zclDIYRuZRT_ProcessInDiscAttrsExtRspCmd( zclIncomingMsg_t *pInMsg );
#endif

// Изменение состояние реле
static void updateRelay( uint8 ch, bool value );
// Отображение состояния реле на пинах
static void applyRelay( uint8 ch );
static void applyRelayAll( void );
// 同步 OnOff 属性变量
static void syncOnOffAttr( uint8 ch );
static void syncOnOffAttrAll( void );
// 统一处理 OnOff 命令
static void handleOnOff( uint8 ep, uint8 cmd );
// Выход из сети
void zclDIYRuZRT_LeaveNetwork( void );
// Отправка отчета о состоянии реле
void zclDIYRuZRT_ReportOnOff( uint8 ep );

/*********************************************************************
 * Таблица обработчиков основных ZCL команд
 * 4 组回调结构体（分别对应 4 个 Endpoint，仅 OnOffCB 不同）
 */
static zclGeneral_AppCallbacks_t zclDIYRuZRT_CmdCallbacks_EP1 =
{
  zclDIYRuZRT_BasicResetCB,               // Basic Cluster Reset command
  NULL,                                   // Identify Trigger Effect command
  zclDIYRuZRT_OnOffCB_EP1,                // On/Off cluster commands
  NULL,                                   // On/Off cluster enhanced command Off with Effect
  NULL,                                   // On/Off cluster enhanced command On with Recall Global Scene
  NULL,                                   // On/Off cluster enhanced command On with Timed Off
#ifdef ZCL_LEVEL_CTRL
  NULL,                                   // Level Control Move to Level command
  NULL,                                   // Level Control Move command
  NULL,                                   // Level Control Step command
  NULL,                                   // Level Control Stop command
#endif
#ifdef ZCL_GROUPS
  NULL,                                   // Group Response commands
#endif
#ifdef ZCL_SCENES
  NULL,                                  // Scene Store Request command
  NULL,                                  // Scene Recall Request command
  NULL,                                  // Scene Response command
#endif
#ifdef ZCL_ALARMS
  NULL,                                  // Alarm (Response) commands
#endif
#ifdef SE_UK_EXT
  NULL,                                  // Get Event Log command
  NULL,                                  // Publish Event Log command
#endif
  NULL,                                  // RSSI Location command
  NULL                                   // RSSI Location Response command
};

static zclGeneral_AppCallbacks_t zclDIYRuZRT_CmdCallbacks_EP2 =
{
  zclDIYRuZRT_BasicResetCB,               // Basic Cluster Reset command
  NULL,                                   // Identify Trigger Effect command
  zclDIYRuZRT_OnOffCB_EP2,                // On/Off cluster commands
  NULL,                                   // On/Off cluster enhanced command Off with Effect
  NULL,                                   // On/Off cluster enhanced command On with Recall Global Scene
  NULL,                                   // On/Off cluster enhanced command On with Timed Off
#ifdef ZCL_LEVEL_CTRL
  NULL,                                   // Level Control Move to Level command
  NULL,                                   // Level Control Move command
  NULL,                                   // Level Control Step command
  NULL,                                   // Level Control Stop command
#endif
#ifdef ZCL_GROUPS
  NULL,                                   // Group Response commands
#endif
#ifdef ZCL_SCENES
  NULL,                                  // Scene Store Request command
  NULL,                                  // Scene Recall Request command
  NULL,                                  // Scene Response command
#endif
#ifdef ZCL_ALARMS
  NULL,                                  // Alarm (Response) commands
#endif
#ifdef SE_UK_EXT
  NULL,                                  // Get Event Log command
  NULL,                                  // Publish Event Log command
#endif
  NULL,                                  // RSSI Location command
  NULL                                   // RSSI Location Response command
};

static zclGeneral_AppCallbacks_t zclDIYRuZRT_CmdCallbacks_EP3 =
{
  zclDIYRuZRT_BasicResetCB,               // Basic Cluster Reset command
  NULL,                                   // Identify Trigger Effect command
  zclDIYRuZRT_OnOffCB_EP3,                // On/Off cluster commands
  NULL,                                   // On/Off cluster enhanced command Off with Effect
  NULL,                                   // On/Off cluster enhanced command On with Recall Global Scene
  NULL,                                   // On/Off cluster enhanced command On with Timed Off
#ifdef ZCL_LEVEL_CTRL
  NULL,                                   // Level Control Move to Level command
  NULL,                                   // Level Control Move command
  NULL,                                   // Level Control Step command
  NULL,                                   // Level Control Stop command
#endif
#ifdef ZCL_GROUPS
  NULL,                                   // Group Response commands
#endif
#ifdef ZCL_SCENES
  NULL,                                  // Scene Store Request command
  NULL,                                  // Scene Recall Request command
  NULL,                                  // Scene Response command
#endif
#ifdef ZCL_ALARMS
  NULL,                                  // Alarm (Response) commands
#endif
#ifdef SE_UK_EXT
  NULL,                                  // Get Event Log command
  NULL,                                  // Publish Event Log command
#endif
  NULL,                                  // RSSI Location command
  NULL                                   // RSSI Location Response command
};

static zclGeneral_AppCallbacks_t zclDIYRuZRT_CmdCallbacks_EP4 =
{
  zclDIYRuZRT_BasicResetCB,               // Basic Cluster Reset command
  NULL,                                   // Identify Trigger Effect command
  zclDIYRuZRT_OnOffCB_EP4,                // On/Off cluster commands
  NULL,                                   // On/Off cluster enhanced command Off with Effect
  NULL,                                   // On/Off cluster enhanced command On with Recall Global Scene
  NULL,                                   // On/Off cluster enhanced command On with Timed Off
#ifdef ZCL_LEVEL_CTRL
  NULL,                                   // Level Control Move to Level command
  NULL,                                   // Level Control Move command
  NULL,                                   // Level Control Step command
  NULL,                                   // Level Control Stop command
#endif
#ifdef ZCL_GROUPS
  NULL,                                   // Group Response commands
#endif
#ifdef ZCL_SCENES
  NULL,                                  // Scene Store Request command
  NULL,                                  // Scene Recall Request command
  NULL,                                  // Scene Response command
#endif
#ifdef ZCL_ALARMS
  NULL,                                  // Alarm (Response) commands
#endif
#ifdef SE_UK_EXT
  NULL,                                  // Get Event Log command
  NULL,                                  // Publish Event Log command
#endif
  NULL,                                  // RSSI Location command
  NULL                                   // RSSI Location Response command
};

/*********************************************************************
 * TODO: Add other callback structures for any additional application specific 
 *       Clusters being used, see available callback structures below.
 *
 *       bdbTL_AppCallbacks_t 
 *       zclApplianceControl_AppCallbacks_t 
 *       zclApplianceEventsAlerts_AppCallbacks_t 
 *       zclApplianceStatistics_AppCallbacks_t 
 *       zclElectricalMeasurement_AppCallbacks_t 
 *       zclGeneral_AppCallbacks_t 
 *       zclGp_AppCallbacks_t 
 *       zclHVAC_AppCallbacks_t 
 *       zclLighting_AppCallbacks_t 
 *       zclMS_AppCallbacks_t 
 *       zclPollControl_AppCallbacks_t 
 *       zclPowerProfile_AppCallbacks_t 
 *       zclSS_AppCallbacks_t  
 *
 */


// Функция инициализации задачи приложения
void zclDIYRuZRT_Init( byte task_id )
{
  zclDIYRuZRT_TaskID = task_id;

  // 注册 4 个 Endpoint 的 SimpleDescriptor
  bdb_RegisterSimpleDescriptor( &zclDIYRuZRT_SimpleDesc_EP1 );
  bdb_RegisterSimpleDescriptor( &zclDIYRuZRT_SimpleDesc_EP2 );
  bdb_RegisterSimpleDescriptor( &zclDIYRuZRT_SimpleDesc_EP3 );
  bdb_RegisterSimpleDescriptor( &zclDIYRuZRT_SimpleDesc_EP4 );

  // 注册 4 组命令回调
  zclGeneral_RegisterCmdCallbacks( DIYRuZRT_ENDPOINT_1, &zclDIYRuZRT_CmdCallbacks_EP1 );
  zclGeneral_RegisterCmdCallbacks( DIYRuZRT_ENDPOINT_2, &zclDIYRuZRT_CmdCallbacks_EP2 );
  zclGeneral_RegisterCmdCallbacks( DIYRuZRT_ENDPOINT_3, &zclDIYRuZRT_CmdCallbacks_EP3 );
  zclGeneral_RegisterCmdCallbacks( DIYRuZRT_ENDPOINT_4, &zclDIYRuZRT_CmdCallbacks_EP4 );

  // 注册 4 组属性列表
  zcl_registerAttrList( DIYRuZRT_ENDPOINT_1, zclDIYRuZRT_NumAttributes_EP1, zclDIYRuZRT_Attrs_EP1 );
  zcl_registerAttrList( DIYRuZRT_ENDPOINT_2, zclDIYRuZRT_NumAttributes_EP2, zclDIYRuZRT_Attrs_EP2 );
  zcl_registerAttrList( DIYRuZRT_ENDPOINT_3, zclDIYRuZRT_NumAttributes_EP3, zclDIYRuZRT_Attrs_EP3 );
  zcl_registerAttrList( DIYRuZRT_ENDPOINT_4, zclDIYRuZRT_NumAttributes_EP4, zclDIYRuZRT_Attrs_EP4 );

  // Подписка задачи на получение сообщений о командах/ответах
  zcl_registerForMsg( zclDIYRuZRT_TaskID );

#ifdef ZCL_DISCOVER
  // Регистрация списка команд, реализуемых приложением
  zcl_registerCmdList( DIYRuZRT_ENDPOINT_1, zclCmdsArraySize, zclDIYRuZRT_Cmds );
  zcl_registerCmdList( DIYRuZRT_ENDPOINT_2, zclCmdsArraySize, zclDIYRuZRT_Cmds );
  zcl_registerCmdList( DIYRuZRT_ENDPOINT_3, zclCmdsArraySize, zclDIYRuZRT_Cmds );
  zcl_registerCmdList( DIYRuZRT_ENDPOINT_4, zclCmdsArraySize, zclDIYRuZRT_Cmds );
#endif

  // Подписка задачи на получение всех событий для кнопок
  RegisterForKeys( zclDIYRuZRT_TaskID );

  bdb_RegisterCommissioningStatusCB( zclDIYRuZRT_ProcessCommissioningStatus );
  bdb_RegisterIdentifyTimeChangeCB( zclDIYRuZRT_ProcessIdentifyTimeChange );

#ifdef ZCL_DIAGNOSTIC
  // Register the application's callback function to read/write attribute data.
  // This is only required when the attribute data format is unknown to ZCL.
  // 注意：ZCL_DIAGNOSTIC 仅注册 EP1
  zcl_registerReadWriteCB( DIYRuZRT_ENDPOINT_1, zclDiagnostic_ReadWriteAttrCB, NULL );

  if ( zclDiagnostic_InitStats() == ZSuccess )
  {
    // Here the user could start the timer to save Diagnostics to NV
  }
#endif

  // Установка адреса и эндпоинта для отправки отчета
  zclDIYRuZRT_DstAddr.addrMode = (afAddrMode_t)AddrNotPresent;
  zclDIYRuZRT_DstAddr.endPoint = 0;
  zclDIYRuZRT_DstAddr.addr.shortAddr = 0;

  // инициализируем NVM для хранения RELAY STATE
  if ( SUCCESS == osal_nv_item_init( NV_DIYRuZRT_RELAY_STATE_ID, 1, &RELAY_STATE ) ) {
    // читаем значение RELAY STATE из памяти
    osal_nv_read( NV_DIYRuZRT_RELAY_STATE_ID, 0, 1, &RELAY_STATE );
  }

  // 同步 OnOff 属性变量
  syncOnOffAttrAll();
  // 应用初始继电器+LED 状态
  applyRelayAll();

  // запускаем повторяемый таймер события HAL_KEY_EVENT через 100мс
  osal_start_reload_timer( zclDIYRuZRT_TaskID, HAL_KEY_EVENT, 100);

  // 移除温度上报定时器

  // 强制写入"新网络"标记，避免 ZDOInitDeviceEx 走恢复网络路径
  // 对首次启动的工厂新设备，NV 存有残留值可能导致系统试图恢复不存在的网络
  zgWriteStartupOptions( ZG_STARTUP_SET, ZCD_STARTOPT_DEFAULT_NETWORK_STATE );

  // 启动入网流程：全套 commissioning（与长按 SW1 5秒行为一致）
  bdb_StartCommissioning(
    BDB_COMMISSIONING_MODE_NWK_FORMATION |
    BDB_COMMISSIONING_MODE_NWK_STEERING |
    BDB_COMMISSIONING_MODE_FINDING_BINDING |
    BDB_COMMISSIONING_MODE_INITIATOR_TL
  );

  DIY_LOG("[DIY] zclDIYRuZRT_Init done");
  DIY_LOG_STR("[DIY] relay_state=0x");
  DIY_LOG_HEX(RELAY_STATE);
  DIY_LOG("");
}


// Основной цикл обрабоки событий задачи
uint16 zclDIYRuZRT_event_loop( uint8 task_id, uint16 events )
{
  afIncomingMSGPacket_t *MSGpkt;

  (void)task_id;  // Intentionally unreferenced parameter

  if ( events & SYS_EVENT_MSG )
  {
    while ( (MSGpkt = (afIncomingMSGPacket_t *)osal_msg_receive( zclDIYRuZRT_TaskID )) )
    {
      switch ( MSGpkt->hdr.event )
      {
        case ZCL_INCOMING_MSG:
          // Обработка входящего сообщения ZCL Foundation комнды/ответа
          zclDIYRuZRT_ProcessIncomingMsg( (zclIncomingMsg_t *)MSGpkt );
          break;

        case KEY_CHANGE:
          zclDIYRuZRT_HandleKeys( ((keyChange_t *)MSGpkt)->state, ((keyChange_t *)MSGpkt)->keys );
          break;

        case ZDO_STATE_CHANGE:
          zclDIYRuZRT_NwkState = (devStates_t)(MSGpkt->hdr.status);

          DIY_LOG_STR("[DIY] ZDO_STATE_CHANGE: ");
          DIY_LOG_U8((uint8)zclDIYRuZRT_NwkState);
          DIY_LOG("");

          // Теперь мы в сети
          if ( (zclDIYRuZRT_NwkState == DEV_ZB_COORD) ||
               (zclDIYRuZRT_NwkState == DEV_ROUTER)   ||
               (zclDIYRuZRT_NwkState == DEV_END_DEVICE) )
          {
            DIY_LOG("[DIY] joined network, report on/off");

            // отключаем мигание
            osal_stop_timerEx(zclDIYRuZRT_TaskID, HAL_LED_BLINK_EVENT);
            HalLedSet( HAL_LED_2, HAL_LED_MODE_OFF );

            // 上报 4 个 EP 的 OnOff 状态
            zclDIYRuZRT_ReportOnOff(1);
            zclDIYRuZRT_ReportOnOff(2);
            zclDIYRuZRT_ReportOnOff(3);
            zclDIYRuZRT_ReportOnOff(4);
          }
          break;

        default:
          break;
      }

      osal_msg_deallocate( (uint8 *)MSGpkt );
    }

    // возврат необработаных сообщений
    return (events ^ SYS_EVENT_MSG);
  }

  /* Обработка событий приложения */
  
  // событие DIYRuZRT_EVT_BLINK
  if ( events & DIYRuZRT_EVT_BLINK )
  {
    // переключим светодиод
    HalLedSet( HAL_LED_2, HAL_LED_MODE_TOGGLE );
    return ( events ^ DIYRuZRT_EVT_BLINK );
  }
  // событие DIYRuZRT_EVT_LONG
  if ( events & DIYRuZRT_EVT_LONG )
  {
    // Проверяем текущее состояние устройства
    // В сети или не в сети?
    if ( bdbAttributes.bdbNodeIsOnANetwork )
    {
      DIY_LOG("[DIY] EVT_LONG: leave network");
      // покидаем сеть
      zclDIYRuZRT_LeaveNetwork();
    }
    else
    {
      DIY_LOG("[DIY] EVT_LONG: start commissioning");
      // инициируем вход в сеть
      bdb_StartCommissioning(
        BDB_COMMISSIONING_MODE_NWK_FORMATION |
        BDB_COMMISSIONING_MODE_NWK_STEERING |
        BDB_COMMISSIONING_MODE_FINDING_BINDING |
        BDB_COMMISSIONING_MODE_INITIATOR_TL
      );
      // будем мигать пока не подключимся
      osal_start_timerEx(zclDIYRuZRT_TaskID, DIYRuZRT_EVT_BLINK, 500);
    }

    return ( events ^ DIYRuZRT_EVT_LONG );
  }
  
  // событие DIYRuZRT_EVT_NV_SAVE (延迟保存 NV)
  if (events & DIYRuZRT_EVT_NV_SAVE) {
    osal_nv_write(NV_DIYRuZRT_RELAY_STATE_ID, 0, 1, &RELAY_STATE);
    return events ^ DIYRuZRT_EVT_NV_SAVE;
  }

  // событие опроса кнопок
  if (events & HAL_KEY_EVENT)
  {
    /* Считывание кнопок */
    DIYRuZRT_HalKeyPoll();

    return events ^ HAL_KEY_EVENT;
  }
  
  // Отбросим необработаные сообщения
  return 0;
}


// Обработчик нажатий клавиш
static void zclDIYRuZRT_HandleKeys( byte shift, byte keys )
{
  if ( keys & HAL_KEY_SW_1 )
  {
    // Запускаем таймер для определения долгого нажания 5сек
    osal_start_timerEx(zclDIYRuZRT_TaskID, DIYRuZRT_EVT_LONG, 5000);
  }
  else
  {
    // Останавливаем таймер ожидания долгого нажатия
    osal_stop_timerEx(zclDIYRuZRT_TaskID, DIYRuZRT_EVT_LONG);
  }
}


// Обработчик изменения статусов соединения с сетью
static void zclDIYRuZRT_ProcessCommissioningStatus(bdbCommissioningModeMsg_t *bdbCommissioningModeMsg)
{
  DIY_LOG_STR("[DIY] commission mode=");
  DIY_LOG_U8(bdbCommissioningModeMsg->bdbCommissioningMode);
  DIY_LOG_STR(" status=");
  DIY_LOG_U8(bdbCommissioningModeMsg->bdbCommissioningStatus);
  DIY_LOG("");

  switch(bdbCommissioningModeMsg->bdbCommissioningMode)
  {
    case BDB_COMMISSIONING_FORMATION:
      if(bdbCommissioningModeMsg->bdbCommissioningStatus == BDB_COMMISSIONING_SUCCESS)
      {
        //After formation, perform nwk steering again plus the remaining commissioning modes that has not been process yet
        bdb_StartCommissioning(BDB_COMMISSIONING_MODE_NWK_STEERING | bdbCommissioningModeMsg->bdbRemainingCommissioningModes);
      }
      else
      {
        //Want to try other channels?
        //try with bdb_setChannelAttribute
      }
    break;
    case BDB_COMMISSIONING_NWK_STEERING:
      if(bdbCommissioningModeMsg->bdbCommissioningStatus == BDB_COMMISSIONING_SUCCESS)
      {
        //YOUR JOB:
        //We are on the nwk, what now?
      }
      else
      {
        // 未找到网络，先清空 commissioning mode 绕过 "already running"
        // 检查，然后重试 NWK_STEERING
        bdbAttributes.bdbCommissioningMode = 0;
        bdb_StartCommissioning(BDB_COMMISSIONING_MODE_NWK_STEERING);
      }
    break;
    case BDB_COMMISSIONING_FINDING_BINDING:
      if(bdbCommissioningModeMsg->bdbCommissioningStatus == BDB_COMMISSIONING_SUCCESS)
      {
        //YOUR JOB:
      }
      else
      {
        //YOUR JOB:
        //retry?, wait for user interaction?
      }
    break;
    case BDB_COMMISSIONING_INITIALIZATION:
      if (bdbCommissioningModeMsg->bdbCommissioningStatus == BDB_COMMISSIONING_SUCCESS)
      {
        // 已在网络中，继续执行剩余的 commissioning 模式
        bdb_StartCommissioning(bdbCommissioningModeMsg->bdbRemainingCommissioningModes);
      }
      else
      {
        // 初始化失败（设备未入网），先清空 commissioning mode 绕过 BDB 的
        // "already running" 检查，再启动 NWK_STEERING 寻找网络加入
        bdbAttributes.bdbCommissioningMode = 0;
        bdb_StartCommissioning(BDB_COMMISSIONING_MODE_NWK_STEERING);
      }
    break;
#if ZG_BUILD_ENDDEVICE_TYPE    
    case BDB_COMMISSIONING_PARENT_LOST:
      if(bdbCommissioningModeMsg->bdbCommissioningStatus == BDB_COMMISSIONING_NETWORK_RESTORED)
      {
        //We did recover from losing parent
      }
      else
      {
        //Parent not found, attempt to rejoin again after a fixed delay
        osal_start_timerEx(zclDIYRuZRT_TaskID, DIYRuZRT_END_DEVICE_REJOIN_EVT, DIYRuZRT_END_DEVICE_REJOIN_DELAY);
      }
    break;
#endif 
  }
}


// Обработчик изменения времени идентификации
static void zclDIYRuZRT_ProcessIdentifyTimeChange( uint8 endpoint )
{
  (void) endpoint;

  if ( zclDIYRuZRT_IdentifyTime > 0 )
  {
    //HalLedBlink ( HAL_LED_2, 0xFF, HAL_LED_DEFAULT_DUTY_CYCLE, HAL_LED_DEFAULT_FLASH_TIME );
  }
  else
  {
    //HalLedSet ( HAL_LED_2, HAL_LED_MODE_OFF );
  }
}


// Обработчик команды сброса в Basic кластере
static void zclDIYRuZRT_BasicResetCB( void )
{
  /* TODO: remember to update this function with any
     application-specific cluster attribute variables */
  
  zclDIYRuZRT_ResetAttributesToDefaultValues();
}

/******************************************************************************
 *
 *  Functions for processing ZCL Foundation incoming Command/Response messages
 *
 *****************************************************************************/

// Функция обработки входящих ZCL Foundation команд/ответов
static void zclDIYRuZRT_ProcessIncomingMsg( zclIncomingMsg_t *pInMsg )
{
  switch ( pInMsg->zclHdr.commandID )
  {
#ifdef ZCL_READ
    case ZCL_CMD_READ_RSP:
      zclDIYRuZRT_ProcessInReadRspCmd( pInMsg );
      break;
#endif
#ifdef ZCL_WRITE
    case ZCL_CMD_WRITE_RSP:
      zclDIYRuZRT_ProcessInWriteRspCmd( pInMsg );
      break;
#endif
    case ZCL_CMD_CONFIG_REPORT:
    case ZCL_CMD_CONFIG_REPORT_RSP:
    case ZCL_CMD_READ_REPORT_CFG:
    case ZCL_CMD_READ_REPORT_CFG_RSP:
    case ZCL_CMD_REPORT:
      //bdb_ProcessIncomingReportingMsg( pInMsg );
      break;
      
    case ZCL_CMD_DEFAULT_RSP:
      zclDIYRuZRT_ProcessInDefaultRspCmd( pInMsg );
      break;
#ifdef ZCL_DISCOVER
    case ZCL_CMD_DISCOVER_CMDS_RECEIVED_RSP:
      zclDIYRuZRT_ProcessInDiscCmdsRspCmd( pInMsg );
      break;

    case ZCL_CMD_DISCOVER_CMDS_GEN_RSP:
      zclDIYRuZRT_ProcessInDiscCmdsRspCmd( pInMsg );
      break;

    case ZCL_CMD_DISCOVER_ATTRS_RSP:
      zclDIYRuZRT_ProcessInDiscAttrsRspCmd( pInMsg );
      break;

    case ZCL_CMD_DISCOVER_ATTRS_EXT_RSP:
      zclDIYRuZRT_ProcessInDiscAttrsExtRspCmd( pInMsg );
      break;
#endif
    default:
      break;
  }

  if ( pInMsg->attrCmd )
    osal_mem_free( pInMsg->attrCmd );
}

#ifdef ZCL_READ
// Обработка ответа команды Read
static uint8 zclDIYRuZRT_ProcessInReadRspCmd( zclIncomingMsg_t *pInMsg )
{
  zclReadRspCmd_t *readRspCmd;
  uint8 i;

  readRspCmd = (zclReadRspCmd_t *)pInMsg->attrCmd;
  for (i = 0; i < readRspCmd->numAttr; i++)
  {
    // Notify the originator of the results of the original read attributes
    // attempt and, for each successfull request, the value of the requested
    // attribute
  }

  return ( TRUE );
}
#endif // ZCL_READ

#ifdef ZCL_WRITE
// Обработка ответа команды Write
static uint8 zclDIYRuZRT_ProcessInWriteRspCmd( zclIncomingMsg_t *pInMsg )
{
  zclWriteRspCmd_t *writeRspCmd;
  uint8 i;

  writeRspCmd = (zclWriteRspCmd_t *)pInMsg->attrCmd;
  for ( i = 0; i < writeRspCmd->numAttr; i++ )
  {
    // Notify the device of the results of the its original write attributes
    // command.
  }

  return ( TRUE );
}
#endif // ZCL_WRITE

// Обработка ответа команды по-умолчанию
static uint8 zclDIYRuZRT_ProcessInDefaultRspCmd( zclIncomingMsg_t *pInMsg )
{
  // zclDefaultRspCmd_t *defaultRspCmd = (zclDefaultRspCmd_t *)pInMsg->attrCmd;

  // Device is notified of the Default Response command.
  (void)pInMsg;

  return ( TRUE );
}

#ifdef ZCL_DISCOVER
// Обработка ответа команды Discover
static uint8 zclDIYRuZRT_ProcessInDiscCmdsRspCmd( zclIncomingMsg_t *pInMsg )
{
  zclDiscoverCmdsCmdRsp_t *discoverRspCmd;
  uint8 i;

  discoverRspCmd = (zclDiscoverCmdsCmdRsp_t *)pInMsg->attrCmd;
  for ( i = 0; i < discoverRspCmd->numCmd; i++ )
  {
    // Device is notified of the result of its attribute discovery command.
  }

  return ( TRUE );
}

// Обработка ответа команды Discover Attributes
static uint8 zclDIYRuZRT_ProcessInDiscAttrsRspCmd( zclIncomingMsg_t *pInMsg )
{
  zclDiscoverAttrsRspCmd_t *discoverRspCmd;
  uint8 i;

  discoverRspCmd = (zclDiscoverAttrsRspCmd_t *)pInMsg->attrCmd;
  for ( i = 0; i < discoverRspCmd->numAttr; i++ )
  {
    // Device is notified of the result of its attribute discovery command.
  }

  return ( TRUE );
}

// Обработка ответа команды Discover Attributes Ext
static uint8 zclDIYRuZRT_ProcessInDiscAttrsExtRspCmd( zclIncomingMsg_t *pInMsg )
{
  zclDiscoverAttrsExtRsp_t *discoverRspCmd;
  uint8 i;

  discoverRspCmd = (zclDiscoverAttrsExtRsp_t *)pInMsg->attrCmd;
  for ( i = 0; i < discoverRspCmd->numAttr; i++ )
  {
    // Device is notified of the result of its attribute discovery command.
  }

  return ( TRUE );
}
#endif // ZCL_DISCOVER


// Инициализация работы кнопок (входов)
void DIYRuZRT_HalKeyInit( void )
{
  /* Сбрасываем сохраняемое состояние кнопок в 0 */
  halKeySavedKeys = 0;
  // 触摸引脚默认 HIGH（上拉），初始化为全 1
  halKeySavedTouch = 0xFF;

  // S1 配网按键初始化
  PUSH1_SEL &= ~(PUSH1_BV); /* Выставляем функцию пина - GPIO */
  PUSH1_DIR &= ~(PUSH1_BV); /* Выставляем режим пина - Вход */

  PUSH1_ICTL &= ~(PUSH1_ICTLBIT); /* Не генерируем прерывания на пине */
  PUSH1_IEN &= ~(PUSH1_IENBIT);   /* Очищаем признак включения прерываний */

  // TOUCH1~4 触摸输入初始化（PUSH2~5 对应 P0_4~P0_7）
  PUSH2_SEL &= ~(PUSH2_BV); /* GPIO 功能 */
  PUSH2_DIR &= ~(PUSH2_BV); /* 输入方向 */
  PUSH3_SEL &= ~(PUSH3_BV);
  PUSH3_DIR &= ~(PUSH3_BV);
  PUSH4_SEL &= ~(PUSH4_BV);
  PUSH4_DIR &= ~(PUSH4_BV);
  PUSH5_SEL &= ~(PUSH5_BV);
  PUSH5_DIR &= ~(PUSH5_BV);
}

// Считывание кнопок
void DIYRuZRT_HalKeyPoll (void)
{
  uint8 keys = 0;
  uint8 touch;

  // S1 配网按键
  if (HAL_PUSH_BUTTON1())
  {
    keys |= HAL_KEY_SW_1;
  }

  // 触摸按键状态检测（低电平有效）
  touch = 0;
  if (TOUCH_PRESSED(1)) touch |= BV(0);
  if (TOUCH_PRESSED(2)) touch |= BV(1);
  if (TOUCH_PRESSED(3)) touch |= BV(2);
  if (TOUCH_PRESSED(4)) touch |= BV(3);

  // 检测下降沿：上次 HIGH → 本次 LOW（触摸按下）
  {
    uint8 falling = (halKeySavedTouch & ~touch) & 0x0F;
    if (falling)
    {
      uint8 ch;
      for (ch = 0; ch < 4; ch++)
      {
        if (falling & BV(ch))
        {
          DIY_LOG_STR("[DIY] touch ch=");
          DIY_LOG_U8(ch + 1);
          DIY_LOG(" down");
          // 切换对应通道继电器状态
          updateRelay(ch, !((RELAY_STATE >> ch) & 1));
        }
      }
    }
  }
  halKeySavedTouch = touch;

  if (keys == halKeySavedKeys)
  {
    // Выход - нет изменений
    return;
  }
  // Сохраним текущее состояние кнопок для сравнения в след раз
  halKeySavedKeys = keys;

  // Вызовем генерацию события изменений кнопок
  OnBoard_SendKeys(keys, HAL_KEY_STATE_NORMAL);
}

// Изменение состояния реле (4路)
static void updateRelay(uint8 ch, bool value)
{
  if (value) RELAY_STATE |= BV(ch);
  else RELAY_STATE &= ~BV(ch);

  DIY_LOG_STR("[DIY] relay ch=");
  DIY_LOG_U8(ch + 1);
  DIY_LOG_STR(" -> ");
  DIY_LOG_U8(value ? 1 : 0);
  DIY_LOG("");

  // 同步 OnOff 属性变量
  syncOnOffAttr(ch);
  // 应用到硬件
  applyRelay(ch);
  // 上报状态
  zclDIYRuZRT_ReportOnOff(ch + 1);

  // 触发延迟 NV 保存（1秒后）
  osal_start_timerEx(zclDIYRuZRT_TaskID, DIYRuZRT_EVT_NV_SAVE, 1000);
}

// Применение состояние реле на пины (4路)
// 硬件反逻辑：继电器 ON → LED 灭；继电器 OFF → LED 亮
static void applyRelay(uint8 ch)
{
  uint8 on = (RELAY_STATE >> ch) & 1;
  switch(ch) {
    case 0: // EP1
      if (on) { RELAY_ON(1); HAL_TURN_OFF_LED1(); }
      else { RELAY_OFF(1); HAL_TURN_ON_LED1(); }
      break;
    case 1: // EP2
      if (on) { RELAY_ON(2); HAL_TURN_OFF_LED2(); }
      else { RELAY_OFF(2); HAL_TURN_ON_LED2(); }
      break;
    case 2: // EP3
      if (on) { RELAY_ON(3); HAL_TURN_OFF_LED3(); }
      else { RELAY_OFF(3); HAL_TURN_ON_LED3(); }
      break;
    case 3: // EP4
      if (on) { RELAY_ON(4); HAL_TURN_OFF_LED4(); }
      else { RELAY_OFF(4); HAL_TURN_ON_LED4(); }
      break;
  }
}

// 应用所有 4 路继电器状态
static void applyRelayAll(void)
{
  uint8 ch;
  for (ch = 0; ch < 4; ch++) {
    applyRelay(ch);
  }
}

// 同步单路 OnOff 属性变量
static void syncOnOffAttr(uint8 ch)
{
  uint8 val = (RELAY_STATE >> ch) & 1;
  switch(ch) {
    case 0: zclDIYRuZRT_OnOff_EP1 = val; break;
    case 1: zclDIYRuZRT_OnOff_EP2 = val; break;
    case 2: zclDIYRuZRT_OnOff_EP3 = val; break;
    case 3: zclDIYRuZRT_OnOff_EP4 = val; break;
  }
}

// 同步所有 4 路 OnOff 属性变量
static void syncOnOffAttrAll(void)
{
  uint8 ch;
  for (ch = 0; ch < 4; ch++) {
    syncOnOffAttr(ch);
  }
}


// Инициализация выхода из сети
void zclDIYRuZRT_LeaveNetwork( void )
{
  DIY_LOG("[DIY] LeaveNetwork");

  zclDIYRuZRT_ResetAttributesToDefaultValues();

  NLME_LeaveReq_t leaveReq;
  // Set every field to 0
  osal_memset(&leaveReq, 0, sizeof(NLME_LeaveReq_t));

  // This will enable the device to rejoin the network after reset.
  leaveReq.rejoin = FALSE;

  // Set the NV startup option to force a "new" join.
  zgWriteStartupOptions(ZG_STARTUP_SET, ZCD_STARTOPT_DEFAULT_NETWORK_STATE);

  // Leave the network, and reset afterwards
  if (NLME_LeaveReq(&leaveReq) != ZSuccess) {
    DIY_LOG("[DIY] LeaveReq fail, reset anyway");
    // Couldn't send out leave; prepare to reset anyway
    ZDApp_LeaveReset(FALSE);
  }
}

// 4 路 OnOff 命令回调
static void zclDIYRuZRT_OnOffCB_EP1(uint8 cmd) { handleOnOff(1, cmd); }
static void zclDIYRuZRT_OnOffCB_EP2(uint8 cmd) { handleOnOff(2, cmd); }
static void zclDIYRuZRT_OnOffCB_EP3(uint8 cmd) { handleOnOff(3, cmd); }
static void zclDIYRuZRT_OnOffCB_EP4(uint8 cmd) { handleOnOff(4, cmd); }

// 统一处理 OnOff 命令
static void handleOnOff(uint8 ep, uint8 cmd) {
  // запомним адрес откуда пришла команда
  // чтобы отправить обратно отчет
  afIncomingMSGPacket_t *pPtr = zcl_getRawAFMsg();
  zclDIYRuZRT_DstAddr.addr.shortAddr = pPtr->srcAddr.addr.shortAddr;

  DIY_LOG_STR("[DIY] onoff ep=");
  DIY_LOG_U8(ep);
  DIY_LOG_STR(" cmd=0x");
  DIY_LOG_HEX(cmd);
  DIY_LOG_STR(" src=0x");
  DIY_LOG_HEX(HI_UINT16(pPtr->srcAddr.addr.shortAddr));
  DIY_LOG_HEX(LO_UINT16(pPtr->srcAddr.addr.shortAddr));
  DIY_LOG("");

  uint8 ch = ep - 1;
  // Включить
  if (cmd == COMMAND_ON) {
    updateRelay(ch, TRUE);
  }
  // Выключить
  else if (cmd == COMMAND_OFF) {
    updateRelay(ch, FALSE);
  }
  // Переключить
  else if (cmd == COMMAND_TOGGLE) {
    updateRelay(ch, !((RELAY_STATE >> ch) & 1));
  }
}

// Информирование о состоянии реле (4路)
void zclDIYRuZRT_ReportOnOff(uint8 ep) {
  const uint8 NUM_ATTRIBUTES = 1;
  zclReportCmd_t *pReportCmd;
  uint8 *pOnOff;

  switch(ep) {
    case 1: pOnOff = &zclDIYRuZRT_OnOff_EP1; break;
    case 2: pOnOff = &zclDIYRuZRT_OnOff_EP2; break;
    case 3: pOnOff = &zclDIYRuZRT_OnOff_EP3; break;
    case 4: pOnOff = &zclDIYRuZRT_OnOff_EP4; break;
    default: return;
  }

  pReportCmd = osal_mem_alloc(sizeof(zclReportCmd_t) +
                              (NUM_ATTRIBUTES * sizeof(zclReport_t)));
  if (pReportCmd != NULL) {
    pReportCmd->numAttr = NUM_ATTRIBUTES;

    pReportCmd->attrList[0].attrID = ATTRID_ON_OFF;
    pReportCmd->attrList[0].dataType = ZCL_DATATYPE_BOOLEAN;
    pReportCmd->attrList[0].attrData = (void *)pOnOff;

    zclDIYRuZRT_DstAddr.addrMode = (afAddrMode_t)Addr16Bit;
    zclDIYRuZRT_DstAddr.addr.shortAddr = 0;
    zclDIYRuZRT_DstAddr.endPoint = ep;

    zcl_SendReportCmd(ep, &zclDIYRuZRT_DstAddr,
                      ZCL_CLUSTER_ID_GEN_ON_OFF, pReportCmd,
                      ZCL_FRAME_SERVER_CLIENT_DIR, false, SeqNum++);
  }

  DIY_LOG_STR("[DIY] report ep=");
  DIY_LOG_U8(ep);
  DIY_LOG_STR(" val=");
  DIY_LOG_U8(*pOnOff);
  DIY_LOG("");

  osal_mem_free(pReportCmd);
}
