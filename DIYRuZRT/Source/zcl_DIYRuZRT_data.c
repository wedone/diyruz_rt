#include "ZComDef.h"
#include "OSAL.h"
#include "AF.h"
#include "ZDConfig.h"

#include "zcl.h"
#include "zcl_general.h"
#include "zcl_ha.h"

/* TODO: Дополняйте нужные заголовки для соответствующих кластеров
#include "zcl_poll_control.h"
#include "zcl_electrical_measurement.h"
#include "zcl_diagnostic.h"
#include "zcl_meter_identification.h"
#include "zcl_appliance_identification.h"
#include "zcl_appliance_events_alerts.h"
#include "zcl_power_profile.h"
#include "zcl_appliance_control.h"
#include "zcl_appliance_statistics.h"
#include "zcl_hvac.h"
*/

#include "zcl_DIYRuZRT.h"

// версия устройства и флаги
#define DIYRuZRT_DEVICE_VERSION     0
#define DIYRuZRT_FLAGS              0

// версия оборудования
#define DIYRuZRT_HWVERSION          1
// версия ZCL
#define DIYRuZRT_ZCLVERSION         1

// версия кластеров
const uint16 zclDIYRuZRT_clusterRevision_all = 0x0001; 

// переменные/константы Basic кластера

// версия оборудования
const uint8 zclDIYRuZRT_HWRevision = DIYRuZRT_HWVERSION;
// версия ZCL
const uint8 zclDIYRuZRT_ZCLVersion = DIYRuZRT_ZCLVERSION;
// производитель（借壳 _TZ3000_XXXX 以 TS0004 身份接入 Zigbee2MQTT）
const uint8 zclDIYRuZRT_ManufacturerName[] = { 12, '_','T','Z','3','0','0','0','_','X','X','X','X' };
// модель设备（TS0004 = 4路开关）
const uint8 zclDIYRuZRT_ModelId[] = { 6, 'T','S','0','0','0','4' };
// дата版本
const uint8 zclDIYRuZRT_DateCode[] = { 8, '2','0','2','6','0','7','1','7' };
// вид питания POWER_SOURCE_MAINS_1_PHASE - питание от сети с одной фазой
const uint8 zclDIYRuZRT_PowerSource = POWER_SOURCE_MAINS_1_PHASE;
// расположение устройства
uint8 zclDIYRuZRT_LocationDescription[17] = { 16, ' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ' };
uint8 zclDIYRuZRT_PhysicalEnvironment = 0;
uint8 zclDIYRuZRT_DeviceEnable = DEVICE_ENABLED;

// переменные/константы Identify кластера

// время идентификации
uint16 zclDIYRuZRT_IdentifyTime;

// Состояние реле（NV 存储用，位操作表示 4 路状态）
extern uint8 RELAY_STATE;

// 4路开关的 OnOff 属性变量（每路独立）
uint8 zclDIYRuZRT_OnOff_EP1 = 0;
uint8 zclDIYRuZRT_OnOff_EP2 = 0;
uint8 zclDIYRuZRT_OnOff_EP3 = 0;
uint8 zclDIYRuZRT_OnOff_EP4 = 0;

// Таблица реализуемых команд для DISCOVER запроса
#if ZCL_DISCOVER
CONST zclCommandRec_t zclDIYRuZRT_Cmds[] =
{
  {
    ZCL_CLUSTER_ID_GEN_BASIC,
    COMMAND_BASIC_RESET_FACT_DEFAULT,
    CMD_DIR_SERVER_RECEIVED
  },
  {
    ZCL_CLUSTER_ID_GEN_ON_OFF,
    COMMAND_OFF,
    CMD_DIR_SERVER_RECEIVED
  },
  {
    ZCL_CLUSTER_ID_GEN_ON_OFF,
    COMMAND_ON,
    CMD_DIR_SERVER_RECEIVED
  },
  {
    ZCL_CLUSTER_ID_GEN_ON_OFF,
    COMMAND_TOGGLE,
    CMD_DIR_SERVER_RECEIVED
  },
};

CONST uint8 zclCmdsArraySize = ( sizeof(zclDIYRuZRT_Cmds) / sizeof(zclDIYRuZRT_Cmds[0]) );
#endif // ZCL_DISCOVER


// =====================================================================
// EP1 属性列表：完整 genBasic + genIdentify + genOnOff
// =====================================================================
CONST zclAttrRec_t zclDIYRuZRT_Attrs_EP1[] =
{
  // *** Атрибуты Basic кластера ***
  {
    ZCL_CLUSTER_ID_GEN_BASIC,             // ID кластера - определен в zcl.h
    { // версия оборудования
      ATTRID_BASIC_HW_VERSION,            // ID атрибута - определен в zcl_general.h
      ZCL_DATATYPE_UINT8,                 // Тип данных  - определен zcl.h
      ACCESS_CONTROL_READ,                // Тип доступа к атрибута - определен в zcl.h
      (void *)&zclDIYRuZRT_HWRevision     // Указатель на переменную хранящую значение
    }
  },
  {
    ZCL_CLUSTER_ID_GEN_BASIC,
    { // версия ZCL
      ATTRID_BASIC_ZCL_VERSION,
      ZCL_DATATYPE_UINT8,
      ACCESS_CONTROL_READ,
      (void *)&zclDIYRuZRT_ZCLVersion
    }
  },
  {
    ZCL_CLUSTER_ID_GEN_BASIC,
    { // версия приложения
      ATTRID_BASIC_APPL_VERSION,
      ZCL_DATATYPE_UINT8,
      ACCESS_CONTROL_READ,
      (void *)&zclDIYRuZRT_ZCLVersion
    }
  },
  {
    ZCL_CLUSTER_ID_GEN_BASIC,
    { // версия стека
      ATTRID_BASIC_STACK_VERSION,
      ZCL_DATATYPE_UINT8,
      ACCESS_CONTROL_READ,
      (void *)&zclDIYRuZRT_ZCLVersion
    }
  },
  {
    ZCL_CLUSTER_ID_GEN_BASIC,
    { // версия прошивки
      ATTRID_BASIC_SW_BUILD_ID,
      ZCL_DATATYPE_CHAR_STR,
      ACCESS_CONTROL_READ,
      (void *)zclDIYRuZRT_DateCode
    }
  },
  {
    ZCL_CLUSTER_ID_GEN_BASIC,
    { // производитель
      ATTRID_BASIC_MANUFACTURER_NAME,
      ZCL_DATATYPE_CHAR_STR,
      ACCESS_CONTROL_READ,
      (void *)zclDIYRuZRT_ManufacturerName
    }
  },
  {
    ZCL_CLUSTER_ID_GEN_BASIC,
    { // модель
      ATTRID_BASIC_MODEL_ID,
      ZCL_DATATYPE_CHAR_STR,
      ACCESS_CONTROL_READ,
      (void *)zclDIYRuZRT_ModelId
    }
  },
  {
    ZCL_CLUSTER_ID_GEN_BASIC,
    { // дата версии
      ATTRID_BASIC_DATE_CODE,
      ZCL_DATATYPE_CHAR_STR,
      ACCESS_CONTROL_READ,
      (void *)zclDIYRuZRT_DateCode
    }
  },
  {
    ZCL_CLUSTER_ID_GEN_BASIC,
    { // тип питания
      ATTRID_BASIC_POWER_SOURCE,
      ZCL_DATATYPE_ENUM8,
      ACCESS_CONTROL_READ,
      (void *)&zclDIYRuZRT_PowerSource
    }
  },
  {
    ZCL_CLUSTER_ID_GEN_BASIC,
    { // расположение
      ATTRID_BASIC_LOCATION_DESC,
      ZCL_DATATYPE_CHAR_STR,
      (ACCESS_CONTROL_READ | ACCESS_CONTROL_WRITE), // может быть изменен
      (void *)zclDIYRuZRT_LocationDescription
    }
  },
  {
    ZCL_CLUSTER_ID_GEN_BASIC,
    {
      ATTRID_BASIC_PHYSICAL_ENV,
      ZCL_DATATYPE_ENUM8,
      (ACCESS_CONTROL_READ | ACCESS_CONTROL_WRITE),
      (void *)&zclDIYRuZRT_PhysicalEnvironment
    }
  },
  {
    ZCL_CLUSTER_ID_GEN_BASIC,
    {
      ATTRID_BASIC_DEVICE_ENABLED,
      ZCL_DATATYPE_BOOLEAN,
      (ACCESS_CONTROL_READ | ACCESS_CONTROL_WRITE),
      (void *)&zclDIYRuZRT_DeviceEnable
    }
  },
  {
    ZCL_CLUSTER_ID_GEN_BASIC,
    { // версия Basic кластера
      ATTRID_CLUSTER_REVISION,
      ZCL_DATATYPE_UINT16,
      ACCESS_CONTROL_READ,
      (void *)&zclDIYRuZRT_clusterRevision_all
    }
  },

#ifdef ZCL_IDENTIFY
  // *** Атрибуты Identify кластера ***
  {
    ZCL_CLUSTER_ID_GEN_IDENTIFY,
    { // время идентификации
      ATTRID_IDENTIFY_TIME,
      ZCL_DATATYPE_UINT16,
      (ACCESS_CONTROL_READ | ACCESS_CONTROL_WRITE),
      (void *)&zclDIYRuZRT_IdentifyTime
    }
  },
#endif
  {
    ZCL_CLUSTER_ID_GEN_IDENTIFY,
    { // версия Identify кластера
      ATTRID_CLUSTER_REVISION,
      ZCL_DATATYPE_UINT16,
      ACCESS_CONTROL_READ,
      (void *)&zclDIYRuZRT_clusterRevision_all
    }
  },

  // *** Атрибуты On/Off кластера (EP1) ***
  {
    ZCL_CLUSTER_ID_GEN_ON_OFF,
    { // состояние（EP1）
      ATTRID_ON_OFF,
      ZCL_DATATYPE_BOOLEAN,
      ACCESS_CONTROL_READ,
      (void *)&zclDIYRuZRT_OnOff_EP1
    }
  },
  {
    ZCL_CLUSTER_ID_GEN_ON_OFF,
    {  // версия On/Off кластера
      ATTRID_CLUSTER_REVISION,
      ZCL_DATATYPE_UINT16,
      ACCESS_CONTROL_READ | ACCESS_CLIENT,
      (void *)&zclDIYRuZRT_clusterRevision_all
    }
  },
};

CONST uint8 zclDIYRuZRT_NumAttributes_EP1 = (sizeof(zclDIYRuZRT_Attrs_EP1) / sizeof(zclDIYRuZRT_Attrs_EP1[0]));

// =====================================================================
// EP2 属性列表：最小 genBasic + genIdentify + genOnOff
// =====================================================================
CONST zclAttrRec_t zclDIYRuZRT_Attrs_EP2[] =
{
  // *** Атрибуты Basic кластера（最小集） ***
  {
    ZCL_CLUSTER_ID_GEN_BASIC,
    { // версия Basic кластера
      ATTRID_CLUSTER_REVISION,
      ZCL_DATATYPE_UINT16,
      ACCESS_CONTROL_READ,
      (void *)&zclDIYRuZRT_clusterRevision_all
    }
  },

#ifdef ZCL_IDENTIFY
  // *** Атрибуты Identify кластера ***
  {
    ZCL_CLUSTER_ID_GEN_IDENTIFY,
    { // время идентификации
      ATTRID_IDENTIFY_TIME,
      ZCL_DATATYPE_UINT16,
      (ACCESS_CONTROL_READ | ACCESS_CONTROL_WRITE),
      (void *)&zclDIYRuZRT_IdentifyTime
    }
  },
#endif
  {
    ZCL_CLUSTER_ID_GEN_IDENTIFY,
    { // версия Identify кластера
      ATTRID_CLUSTER_REVISION,
      ZCL_DATATYPE_UINT16,
      ACCESS_CONTROL_READ,
      (void *)&zclDIYRuZRT_clusterRevision_all
    }
  },

  // *** Атрибуты On/Off кластера (EP2) ***
  {
    ZCL_CLUSTER_ID_GEN_ON_OFF,
    { // состояние（EP2）
      ATTRID_ON_OFF,
      ZCL_DATATYPE_BOOLEAN,
      ACCESS_CONTROL_READ,
      (void *)&zclDIYRuZRT_OnOff_EP2
    }
  },
  {
    ZCL_CLUSTER_ID_GEN_ON_OFF,
    {  // версия On/Off кластера
      ATTRID_CLUSTER_REVISION,
      ZCL_DATATYPE_UINT16,
      ACCESS_CONTROL_READ | ACCESS_CLIENT,
      (void *)&zclDIYRuZRT_clusterRevision_all
    }
  },
};

CONST uint8 zclDIYRuZRT_NumAttributes_EP2 = (sizeof(zclDIYRuZRT_Attrs_EP2) / sizeof(zclDIYRuZRT_Attrs_EP2[0]));

// =====================================================================
// EP3 属性列表：最小 genBasic + genIdentify + genOnOff
// =====================================================================
CONST zclAttrRec_t zclDIYRuZRT_Attrs_EP3[] =
{
  // *** Атрибуты Basic кластера（最小集） ***
  {
    ZCL_CLUSTER_ID_GEN_BASIC,
    { // версия Basic кластера
      ATTRID_CLUSTER_REVISION,
      ZCL_DATATYPE_UINT16,
      ACCESS_CONTROL_READ,
      (void *)&zclDIYRuZRT_clusterRevision_all
    }
  },

#ifdef ZCL_IDENTIFY
  // *** Атрибуты Identify кластера ***
  {
    ZCL_CLUSTER_ID_GEN_IDENTIFY,
    { // время идентификации
      ATTRID_IDENTIFY_TIME,
      ZCL_DATATYPE_UINT16,
      (ACCESS_CONTROL_READ | ACCESS_CONTROL_WRITE),
      (void *)&zclDIYRuZRT_IdentifyTime
    }
  },
#endif
  {
    ZCL_CLUSTER_ID_GEN_IDENTIFY,
    { // версия Identify кластера
      ATTRID_CLUSTER_REVISION,
      ZCL_DATATYPE_UINT16,
      ACCESS_CONTROL_READ,
      (void *)&zclDIYRuZRT_clusterRevision_all
    }
  },

  // *** Атрибуты On/Off кластера (EP3) ***
  {
    ZCL_CLUSTER_ID_GEN_ON_OFF,
    { // состояние（EP3）
      ATTRID_ON_OFF,
      ZCL_DATATYPE_BOOLEAN,
      ACCESS_CONTROL_READ,
      (void *)&zclDIYRuZRT_OnOff_EP3
    }
  },
  {
    ZCL_CLUSTER_ID_GEN_ON_OFF,
    {  // версия On/Off кластера
      ATTRID_CLUSTER_REVISION,
      ZCL_DATATYPE_UINT16,
      ACCESS_CONTROL_READ | ACCESS_CLIENT,
      (void *)&zclDIYRuZRT_clusterRevision_all
    }
  },
};

CONST uint8 zclDIYRuZRT_NumAttributes_EP3 = (sizeof(zclDIYRuZRT_Attrs_EP3) / sizeof(zclDIYRuZRT_Attrs_EP3[0]));

// =====================================================================
// EP4 属性列表：最小 genBasic + genIdentify + genOnOff
// =====================================================================
CONST zclAttrRec_t zclDIYRuZRT_Attrs_EP4[] =
{
  // *** Атрибуты Basic кластера（最小集） ***
  {
    ZCL_CLUSTER_ID_GEN_BASIC,
    { // версия Basic кластера
      ATTRID_CLUSTER_REVISION,
      ZCL_DATATYPE_UINT16,
      ACCESS_CONTROL_READ,
      (void *)&zclDIYRuZRT_clusterRevision_all
    }
  },

#ifdef ZCL_IDENTIFY
  // *** Атрибуты Identify кластера ***
  {
    ZCL_CLUSTER_ID_GEN_IDENTIFY,
    { // время идентификации
      ATTRID_IDENTIFY_TIME,
      ZCL_DATATYPE_UINT16,
      (ACCESS_CONTROL_READ | ACCESS_CONTROL_WRITE),
      (void *)&zclDIYRuZRT_IdentifyTime
    }
  },
#endif
  {
    ZCL_CLUSTER_ID_GEN_IDENTIFY,
    { // версия Identify кластера
      ATTRID_CLUSTER_REVISION,
      ZCL_DATATYPE_UINT16,
      ACCESS_CONTROL_READ,
      (void *)&zclDIYRuZRT_clusterRevision_all
    }
  },

  // *** Атрибуты On/Off кластера (EP4) ***
  {
    ZCL_CLUSTER_ID_GEN_ON_OFF,
    { // состояние（EP4）
      ATTRID_ON_OFF,
      ZCL_DATATYPE_BOOLEAN,
      ACCESS_CONTROL_READ,
      (void *)&zclDIYRuZRT_OnOff_EP4
    }
  },
  {
    ZCL_CLUSTER_ID_GEN_ON_OFF,
    {  // версия On/Off кластера
      ATTRID_CLUSTER_REVISION,
      ZCL_DATATYPE_UINT16,
      ACCESS_CONTROL_READ | ACCESS_CLIENT,
      (void *)&zclDIYRuZRT_clusterRevision_all
    }
  },
};

CONST uint8 zclDIYRuZRT_NumAttributes_EP4 = (sizeof(zclDIYRuZRT_Attrs_EP4) / sizeof(zclDIYRuZRT_Attrs_EP4[0]));

// =====================================================================
// 输入集群列表
// =====================================================================

// EP1 输入集群列表：genBasic, genIdentify, genGroups, genOnOff（+ OTA 预留）
const cId_t zclDIYRuZRT_InClusterList_EP1[] =
{
  ZCL_CLUSTER_ID_GEN_BASIC,
  ZCL_CLUSTER_ID_GEN_IDENTIFY,
  ZCL_CLUSTER_ID_GEN_GROUPS,
  ZCL_CLUSTER_ID_GEN_ON_OFF,
#if ZCL_OTA
  ZCL_CLUSTER_ID_OTA,
#endif
};
#define ZCLDIYRuZRT_MAX_INCLUSTERS_EP1   (sizeof(zclDIYRuZRT_InClusterList_EP1) / sizeof(zclDIYRuZRT_InClusterList_EP1[0]))

// EP2~EP4 共用的最小输入集群列表：genBasic, genIdentify, genOnOff（无 genGroups）
const cId_t zclDIYRuZRT_InClusterList_Minimal[] =
{
  ZCL_CLUSTER_ID_GEN_BASIC,
  ZCL_CLUSTER_ID_GEN_IDENTIFY,
  ZCL_CLUSTER_ID_GEN_ON_OFF,
};
#define ZCLDIYRuZRT_MAX_INCLUSTERS_MINIMAL   (sizeof(zclDIYRuZRT_InClusterList_Minimal) / sizeof(zclDIYRuZRT_InClusterList_Minimal[0]))

// Список исходящих кластеров приложения（4 个 EP 共用）
const cId_t zclDIYRuZRT_OutClusterList[] =
{
  ZCL_CLUSTER_ID_GEN_BASIC,
};
#define ZCLDIYRuZRT_MAX_OUTCLUSTERS  (sizeof(zclDIYRuZRT_OutClusterList) / sizeof(zclDIYRuZRT_OutClusterList[0]))

// =====================================================================
// 4 个 Endpoint 的 SimpleDescriptionFormat_t
// =====================================================================

// EP1 描述：完整 Basic + Groups + OnOff
SimpleDescriptionFormat_t zclDIYRuZRT_SimpleDesc_EP1 =
{
  DIYRuZRT_ENDPOINT_1,                       //  int Endpoint;
  ZCL_HA_PROFILE_ID,                         //  uint16 AppProfId;
  ZCL_HA_DEVICEID_ON_OFF_SWITCH,             //  uint16 AppDeviceId;
  DIYRuZRT_DEVICE_VERSION,                   //  int   AppDevVer:4;
  DIYRuZRT_FLAGS,                            //  int   AppFlags:4;
  ZCLDIYRuZRT_MAX_INCLUSTERS_EP1,            //  byte  AppNumInClusters;
  (cId_t *)zclDIYRuZRT_InClusterList_EP1,    //  byte *pAppInClusterList;
  ZCLDIYRuZRT_MAX_OUTCLUSTERS,               //  byte  AppNumOutClusters;
  (cId_t *)zclDIYRuZRT_OutClusterList        //  byte *pAppOutClusterList;
};

// EP2 描述：最小 Basic + OnOff
SimpleDescriptionFormat_t zclDIYRuZRT_SimpleDesc_EP2 =
{
  DIYRuZRT_ENDPOINT_2,                          //  int Endpoint;
  ZCL_HA_PROFILE_ID,                            //  uint16 AppProfId;
  ZCL_HA_DEVICEID_ON_OFF_SWITCH,                //  uint16 AppDeviceId;
  DIYRuZRT_DEVICE_VERSION,                      //  int   AppDevVer:4;
  DIYRuZRT_FLAGS,                               //  int   AppFlags:4;
  ZCLDIYRuZRT_MAX_INCLUSTERS_MINIMAL,           //  byte  AppNumInClusters;
  (cId_t *)zclDIYRuZRT_InClusterList_Minimal,   //  byte *pAppInClusterList;
  ZCLDIYRuZRT_MAX_OUTCLUSTERS,                  //  byte  AppNumOutClusters;
  (cId_t *)zclDIYRuZRT_OutClusterList           //  byte *pAppOutClusterList;
};

// EP3 描述：最小 Basic + OnOff
SimpleDescriptionFormat_t zclDIYRuZRT_SimpleDesc_EP3 =
{
  DIYRuZRT_ENDPOINT_3,                          //  int Endpoint;
  ZCL_HA_PROFILE_ID,                            //  uint16 AppProfId;
  ZCL_HA_DEVICEID_ON_OFF_SWITCH,                //  uint16 AppDeviceId;
  DIYRuZRT_DEVICE_VERSION,                      //  int   AppDevVer:4;
  DIYRuZRT_FLAGS,                               //  int   AppFlags:4;
  ZCLDIYRuZRT_MAX_INCLUSTERS_MINIMAL,           //  byte  AppNumInClusters;
  (cId_t *)zclDIYRuZRT_InClusterList_Minimal,   //  byte *pAppInClusterList;
  ZCLDIYRuZRT_MAX_OUTCLUSTERS,                  //  byte  AppNumOutClusters;
  (cId_t *)zclDIYRuZRT_OutClusterList           //  byte *pAppOutClusterList;
};

// EP4 描述：最小 Basic + OnOff
SimpleDescriptionFormat_t zclDIYRuZRT_SimpleDesc_EP4 =
{
  DIYRuZRT_ENDPOINT_4,                          //  int Endpoint;
  ZCL_HA_PROFILE_ID,                            //  uint16 AppProfId;
  ZCL_HA_DEVICEID_ON_OFF_SWITCH,                //  uint16 AppDeviceId;
  DIYRuZRT_DEVICE_VERSION,                      //  int   AppDevVer:4;
  DIYRuZRT_FLAGS,                               //  int   AppFlags:4;
  ZCLDIYRuZRT_MAX_INCLUSTERS_MINIMAL,           //  byte  AppNumInClusters;
  (cId_t *)zclDIYRuZRT_InClusterList_Minimal,   //  byte *pAppInClusterList;
  ZCLDIYRuZRT_MAX_OUTCLUSTERS,                  //  byte  AppNumOutClusters;
  (cId_t *)zclDIYRuZRT_OutClusterList           //  byte *pAppOutClusterList;
};

// Сброс атрибутов в значения по-умолчанию  
void zclDIYRuZRT_ResetAttributesToDefaultValues(void)
{
  int i;
  
  zclDIYRuZRT_LocationDescription[0] = 16;
  for (i = 1; i <= 16; i++)
  {
    zclDIYRuZRT_LocationDescription[i] = ' ';
  }
  
  zclDIYRuZRT_PhysicalEnvironment = PHY_UNSPECIFIED_ENV;
  zclDIYRuZRT_DeviceEnable = DEVICE_ENABLED;
  
#ifdef ZCL_IDENTIFY
  zclDIYRuZRT_IdentifyTime = 0;
#endif
}
