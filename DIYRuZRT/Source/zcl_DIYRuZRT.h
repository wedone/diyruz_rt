#ifndef ZCL_DIYRuZRT_H
#define ZCL_DIYRuZRT_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "zcl.h"

// Номер эндпоинта устройства
#define DIYRuZRT_ENDPOINT            1

// 4路开关的 Endpoint 编号
#define DIYRuZRT_ENDPOINT_1           1
#define DIYRuZRT_ENDPOINT_2           2
#define DIYRuZRT_ENDPOINT_3           3
#define DIYRuZRT_ENDPOINT_4           4

// События приложения
#define DIYRuZRT_EVT_BLINK                0x0001
#define DIYRuZRT_EVT_LONG                 0x0002
#define DIYRuZRT_END_DEVICE_REJOIN_EVT    0x0004
#define DIYRuZRT_REPORTING_EVT            0x0008
// 延迟保存 NV
#define DIYRuZRT_EVT_NV_SAVE              0x0010
  
  
// NVM IDs
#define NV_DIYRuZRT_RELAY_STATE_ID        0x0402

// 4 个 Endpoint 的 SimpleDescriptor
extern SimpleDescriptionFormat_t zclDIYRuZRT_SimpleDesc_EP1;
extern SimpleDescriptionFormat_t zclDIYRuZRT_SimpleDesc_EP2;
extern SimpleDescriptionFormat_t zclDIYRuZRT_SimpleDesc_EP3;
extern SimpleDescriptionFormat_t zclDIYRuZRT_SimpleDesc_EP4;

extern CONST zclCommandRec_t zclDIYRuZRT_Cmds[];

extern CONST uint8 zclCmdsArraySize;

// 4 组属性列表
extern CONST zclAttrRec_t zclDIYRuZRT_Attrs_EP1[];
extern CONST zclAttrRec_t zclDIYRuZRT_Attrs_EP2[];
extern CONST zclAttrRec_t zclDIYRuZRT_Attrs_EP3[];
extern CONST zclAttrRec_t zclDIYRuZRT_Attrs_EP4[];
extern CONST uint8 zclDIYRuZRT_NumAttributes_EP1;
extern CONST uint8 zclDIYRuZRT_NumAttributes_EP2;
extern CONST uint8 zclDIYRuZRT_NumAttributes_EP3;
extern CONST uint8 zclDIYRuZRT_NumAttributes_EP4;

// 4 路 OnOff 属性变量
extern uint8 zclDIYRuZRT_OnOff_EP1;
extern uint8 zclDIYRuZRT_OnOff_EP2;
extern uint8 zclDIYRuZRT_OnOff_EP3;
extern uint8 zclDIYRuZRT_OnOff_EP4;

// Атрибуты идентификации
extern uint16 zclDIYRuZRT_IdentifyTime;
extern uint8  zclDIYRuZRT_IdentifyCommissionState;

// TODO: Declare application specific attributes here

// Инициализация задачи
extern void zclDIYRuZRT_Init( byte task_id );

// Обработчик сообщений задачи
extern UINT16 zclDIYRuZRT_event_loop( byte task_id, UINT16 events );

// Сброс всех атрибутов в начальное состояние
extern void zclDIYRuZRT_ResetAttributesToDefaultValues(void);

// Функции работы с кнопками
extern void DIYRuZRT_HalKeyInit( void );
extern void DIYRuZRT_HalKeyPoll ( void );

// Функции команд управления
// 4 路 OnOff 命令回调
static void zclDIYRuZRT_OnOffCB_EP1(uint8);
static void zclDIYRuZRT_OnOffCB_EP2(uint8);
static void zclDIYRuZRT_OnOffCB_EP3(uint8);
static void zclDIYRuZRT_OnOffCB_EP4(uint8);

#ifdef __cplusplus
}
#endif

#endif /* ZCL_DIYRuZRT_H */