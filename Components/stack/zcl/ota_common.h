/******************************************************************************
  Filename:       ota_common.h
  Revised:        $Date: 2013-12-10 07:42:48 -0800 (Tue, 10 Dec 2013) $
  Revision:       $Revision: 36527 $

  Description:    This file contains code common to the OTA server,
                  client, and dongle.

  基于 TI 官方 Z-Stack ota_common.h，适配 Z-Stack 3.0.2（CC2530）：
  - 使用 OSAL 类型 uint8/uint16/uint32 而非 C99 类型
  - 移除 CC2538 专用的 pack pragma
  - 仅保留 hal_ota.c / zcl_ota.c 实际引用的类型与常量

  Copyright 2010-2013 Texas Instruments Incorporated.
  All rights reserved not granted herein. Limited License.
******************************************************************************/

#ifndef OTA_COMMON_H
#define OTA_COMMON_H

/******************************************************************************
 * INCLUDES
 */
#if !defined HAL_OTA_BOOT_CODE
#include "af.h"
#endif

#include "hal_mcu.h"

/******************************************************************************
 * CONSTANTS
 */

// OTA Header constants
#define OTA_HDR_MAGIC_NUMBER                0x0BEEF11E
#define OTA_HDR_BLOCK_SIZE                  128
#define OTA_HDR_STACK_VERSION               2
#define OTA_HDR_HEADER_VERSION              0x0100
#define OTA_HDR_FIELD_CTRL                  0

#define OTA_HEADER_LEN_MIN                  56
#define OTA_HEADER_LEN_MAX                  69
#define OTA_HEADER_LEN_MIN_ECDSA            166
#define OTA_HEADER_STR_LEN                  32

#define OTA_HEADER_IMAGE_SIZE_POS           52
// OTA_HEADER_FILE_ID_POS is needed for windows tools
#define OTA_HEADER_FILE_ID_POS              10

#define OTA_FC_SCV_PRESENT                  (0x1 << 0)
#define OTA_FC_DSF_PRESENT                  (0x1 << 1)
#define OTA_FC_HWV_PRESENT                  (0x1 << 2)

#define OTA_SUB_ELEMENT_HDR_LEN             6

#define OTA_UPGRADE_IMAGE_TAG_ID            0
#define OTA_ECDSA_SIGNATURE_TAG_ID          1
#define OTA_EDCSA_CERTIFICATE_TAG_ID        2

// MT_OtaGeImage options
#define MT_OTA_HW_VER_PRESENT_OPTION        0x01
#define MT_OTA_QUERY_SPECIFIC_OPTION        0x02

// MT OTA Status Indication Types
#define MT_OTA_DL_COMPLETE                  0

#define OTA_INVALID_ID                      0xFF

#define OTA_APP_MAX_ATTRIBUTES              7

// SYS App message format byte positions
#define MT_APP_ENDPOINT_POS                 0
#define MT_APP_COMMAND_POS                  1
#define MT_APP_DATA_POS                     2

// SYS APP Commands for dongle communication with OTA Console
#define OTA_APP_READ_ATTRIBUTE_REQ          0
#define OTA_APP_IMAGE_NOTIFY_REQ            1
#define OTA_APP_DISCOVERY_REQ               2
#define OTA_APP_JOIN_REQ                    3
#define OTA_APP_LEAVE_REQ                   4

#define OTA_APP_READ_ATTRIBUTE_IND          0x80
#define OTA_APP_IMAGE_NOTIFY_RSP            0x81
#define OTA_APP_DEVICE_IND                  0x82
#define OTA_APP_JOIN_IND                    0x83
#define OTA_APP_ENDPOINT_IND                0x85
#define OTA_APP_DONGLE_IND                  0x8A

// Sys App Message Lengths
#define OTA_APP_READ_ATTRIBUTE_REQ_LEN      (8 + OTA_APP_MAX_ATTRIBUTES*2)
#define OTA_APP_IMAGE_NOTIFY_REQ_LEN        15
#define OTA_APP_DISCOVERY_REQ_LEN           2
#define OTA_APP_JOIN_REQ_LEN                5
#define OTA_APP_LEAVE_REQ_LEN               2

#define OTA_APP_READ_ATTRIBUTE_IND_LEN      21
#define OTA_APP_IMAGE_NOTIFY_RSP_LEN        16
#define OTA_APP_DEVICE_IND_LEN              6
#define OTA_APP_JOIN_IND_LEN                4
#define OTA_APP_ENDPOINT_IND_LEN            7
#define OTA_APP_DONGLE_IND_LEN              10

/******************************************************************************
 * TYPEDEFS
 */

// OTA 文件标识：制造商 + 类型 + 版本
typedef struct
{
  uint16 manufacturer;
  uint16 type;
  uint32 version;
} zclOTA_FileID_t;

// OTA 子元素头部
typedef struct
{
  uint16 tag;
  uint32 length;
} OTA_SubElementHdr_t;

// OTA 镜像头部
typedef struct
{
  uint32 magicNumber;
  uint16 headerVersion;
  uint16 headerLength;
  uint16 fieldControl;
  zclOTA_FileID_t fileId;
  uint16 stackVersion;
  uint8  headerString[OTA_HEADER_STR_LEN];
  uint32 imageSize;
  uint8  secCredentialVer;
  uint8  destIEEE[8];
  uint16 minHwVer;
  uint16 maxHwVer;
} OTA_ImageHeader_t;

/******************************************************************************
 * FUNCTIONS
 */

#ifdef __cplusplus
extern "C"
{
#endif

extern uint8 *OTA_WriteHeader(OTA_ImageHeader_t *pHdr, uint8 *pBuf);
extern uint8 *OTA_ParseHeader(OTA_ImageHeader_t *pHdr, uint8 *pBuf);

extern void OTA_GetFileName(char *pName, zclOTA_FileID_t *pFileId, char *text);
extern void OTA_SplitFileName(char *pName, zclOTA_FileID_t *pFileId);

extern uint8 *OTA_StreamToFileId(zclOTA_FileID_t *pFileId, uint8 *pStream);
extern uint8 *OTA_FileIdToStream(zclOTA_FileID_t *pFileId, uint8 *pStream);

#if !defined HAL_OTA_BOOT_CODE
extern uint8 *OTA_AfAddrToStream(afAddrType_t *pAddr, uint8 *pStream);
extern uint8 *OTA_StreamToAfAddr(afAddrType_t *pAddr, uint8 *pStream);
#endif

#ifdef __cplusplus
}
#endif

#endif // OTA_COMMON_H
