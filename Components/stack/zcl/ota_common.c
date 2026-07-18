/******************************************************************************
  Filename:       ota_common.c
  Revised:        $Date: 2011-08-16 16:45:58 -0700 (Tue, 16 Aug 2011) $
  Revision:       $Revision: 27200 $

  Description:    This file contains code common to the OTA server,
                  client, and dongle.

  基于 TI 官方 Z-Stack ota_common.c，适配 Z-Stack 3.0.2（CC2530）：
  - 使用 OSAL 类型 uint8/uint16/uint32 而非 C99 类型
  - 使用 osal_memcpy 替代 OsalPort_memcpy
  - 移除 rom_jt_154.h 与 _WIN32 条件分支

  Copyright 2010-2011 Texas Instruments Incorporated.
  All rights reserved not granted herein. Limited License.
******************************************************************************/

/******************************************************************************
 * INCLUDES
 */
#include "hal_types.h"
#include "OSAL.h"
#include "ota_common.h"

#include <string.h>

/******************************************************************************
 * MACROS
 */
#define UINT16_BREAK_HEX(a, b)   (((a) >> (12-(4*(b)))) & 0xF)
#define UINT32_BREAK_HEX(a, b)   (((a) >> (28-(4*(b)))) & 0xF)

/*********************************************************************
 * CONSTANTS
 */

/******************************************************************************
 * LOCAL VARIABLES
 */
static char HEX_char[] = "0123456789ABCDEF";

/******************************************************************************
 * LOCAL FUNCTIONS
 */
static uint8 char2uint(char c);

/******************************************************************************
 * @fn      char2uint
 *
 * @brief   Converts a hex character to a uint8
 *
 * @param   c - Character to convert
 *
 * @return  uint8 value of c
 */
static uint8 char2uint(char c)
{
  if (c >= '0' && c <= '9')
  {
    return c - '0';
  }
  if (c >= 'a' && c <= 'f')
  {
    return 0xA + c - 'a';
  }
  if (c >= 'A' && c <= 'F')
  {
    return 0xA + c - 'A';
  }

  return 0;
}

/******************************************************************************
 * @fn      OTA_ParseHeader
 *
 * @brief   Reads the OTA header from the input buffer.
 *
 * @param   pHdr - pointer to the header information
 * @param   pBuf - pointer to the input buffer
 *
 * @return  new buffer pointer
 */
uint8 *OTA_ParseHeader(OTA_ImageHeader_t *pHdr, uint8 *pBuf)
{
  uint8 i;

  // Get the Magic Number
  pHdr->magicNumber = BUILD_UINT32(pBuf[0], pBuf[1], pBuf[2], pBuf[3]);
  pBuf += 4;

  // Get the Header Version
  pHdr->headerVersion = BUILD_UINT16(pBuf[0], pBuf[1]);
  pBuf += 2;

  // Get the Header Length
  pHdr->headerLength = BUILD_UINT16(pBuf[0], pBuf[1]);
  pBuf += 2;

  // Get the Field Control
  pHdr->fieldControl = BUILD_UINT16(pBuf[0], pBuf[1]);
  pBuf += 2;

  // Get the Manufacturer ID
  pHdr->fileId.manufacturer = BUILD_UINT16(pBuf[0], pBuf[1]);
  pBuf += 2;

  // Get the Image Type
  pHdr->fileId.type = BUILD_UINT16(pBuf[0], pBuf[1]);
  pBuf += 2;

  // Get the File Version
  pHdr->fileId.version = BUILD_UINT32(pBuf[0], pBuf[1], pBuf[2], pBuf[3]);
  pBuf += 4;

  // Get the Stack Version
  pHdr->stackVersion = BUILD_UINT16(pBuf[0], pBuf[1]);
  pBuf += 2;

  // Get the Header string
  for (i=0; i<OTA_HEADER_STR_LEN; i++)
  {
    pHdr->headerString[i] = *pBuf++;
  }

  // Get the Image Size
  pHdr->imageSize = BUILD_UINT32(pBuf[0], pBuf[1], pBuf[2], pBuf[3]);
  pBuf += 4;

  // Get the Security Credential Version
  if (pHdr->fieldControl & OTA_FC_SCV_PRESENT)
  {
    pHdr->secCredentialVer = *pBuf++;
  }

  // Get the Upgrade File Destination
  if (pHdr->fieldControl & OTA_FC_DSF_PRESENT)
  {
    for (i=0; i<Z_EXTADDR_LEN; i++)
    {
      pHdr->destIEEE[i] = *pBuf++;
    }
  }

  // Get the Min and Max Hardware Versions
  if (pHdr->fieldControl & OTA_FC_HWV_PRESENT)
  {
    pHdr->minHwVer = BUILD_UINT16(pBuf[0], pBuf[1]);
    pBuf += 2;
    pHdr->maxHwVer = BUILD_UINT16(pBuf[0], pBuf[1]);
    pBuf += 2;
  }

  return pBuf;
}

/******************************************************************************
 * @fn      OTA_WriteHeader
 *
 * @brief   Writes the OTA header to the output buffer.
 *
 * @param   pHdr - pointer to the header information
 * @param   pBuf - pointer to the output buffer
 *
 * @return  new buffer pointer
 */
uint8 *OTA_WriteHeader(OTA_ImageHeader_t *pHdr, uint8 *pBuf)
{
  uint8 i;

  // Output the Magic Number
  *pBuf++ = BREAK_UINT32(pHdr->magicNumber, 0);
  *pBuf++ = BREAK_UINT32(pHdr->magicNumber, 1);
  *pBuf++ = BREAK_UINT32(pHdr->magicNumber, 2);
  *pBuf++ = BREAK_UINT32(pHdr->magicNumber, 3);

  // Output the Header Version
  *pBuf++ = LO_UINT16(pHdr->headerVersion);
  *pBuf++ = HI_UINT16(pHdr->headerVersion);

  // Output the Header Length
  *pBuf++ = LO_UINT16(pHdr->headerLength);
  *pBuf++ = HI_UINT16(pHdr->headerLength);

  // Output the Field Control
  *pBuf++ = LO_UINT16(pHdr->fieldControl);
  *pBuf++ = HI_UINT16(pHdr->fieldControl);

  // Output the Manufacturer ID
  *pBuf++ = LO_UINT16(pHdr->fileId.manufacturer);
  *pBuf++ = HI_UINT16(pHdr->fileId.manufacturer);

  // Output the Image Type
  *pBuf++ = LO_UINT16(pHdr->fileId.type);
  *pBuf++ = HI_UINT16(pHdr->fileId.type);

  // Output the File Version
  *pBuf++ = BREAK_UINT32(pHdr->fileId.version, 0);
  *pBuf++ = BREAK_UINT32(pHdr->fileId.version, 1);
  *pBuf++ = BREAK_UINT32(pHdr->fileId.version, 2);
  *pBuf++ = BREAK_UINT32(pHdr->fileId.version, 3);

  // Output the Stack Version
  *pBuf++ = LO_UINT16(pHdr->stackVersion);
  *pBuf++ = HI_UINT16(pHdr->stackVersion);

  // Output the Header string
  for (i=0; i<OTA_HEADER_STR_LEN; i++)
  {
    *pBuf++ = pHdr->headerString[i];
  }

  // Output the Image Size
  *pBuf++ = BREAK_UINT32(pHdr->imageSize, 0);
  *pBuf++ = BREAK_UINT32(pHdr->imageSize, 1);
  *pBuf++ = BREAK_UINT32(pHdr->imageSize, 2);
  *pBuf++ = BREAK_UINT32(pHdr->imageSize, 3);

  // Output the Security Credential Version
  if (pHdr->fieldControl & OTA_FC_SCV_PRESENT)
  {
    *pBuf++ = pHdr->secCredentialVer;
  }

  // Output the Upgrade File Destination
  if (pHdr->fieldControl & OTA_FC_DSF_PRESENT)
  {
    for (i=0; i<Z_EXTADDR_LEN; i++)
    {
      *pBuf++ = pHdr->destIEEE[i];
    }
  }

  // Output the Min and Max Hardware Versions
  if (pHdr->fieldControl & OTA_FC_HWV_PRESENT)
  {
    *pBuf++ = LO_UINT16(pHdr->minHwVer);
    *pBuf++ = HI_UINT16(pHdr->minHwVer);

    *pBuf++ = LO_UINT16(pHdr->maxHwVer);
    *pBuf++ = HI_UINT16(pHdr->maxHwVer);
  }

  return pBuf;
}

/******************************************************************************
 * @fn      OTA_GetFileName
 *
 * @brief   Get the text of a filename from the file ID (zclOTA_FileID_t)
 *
 * @param   pName - Buffer to hold the name
 *          pFileID - Pointer to File ID structure
 *          text - Text description of file
 *
 * @return  none
 */
void OTA_GetFileName(char *pName, zclOTA_FileID_t *pFileId, char *text)
{
  int8 i, len;

  // Insert the manufacturer ID
  if (pFileId->manufacturer == 0xFFFF)
  {
    *pName++ = '*';
  }
  else
  {
    for (i=0; i<4; i++)
    {
      *pName++ = HEX_char[UINT16_BREAK_HEX(pFileId->manufacturer, i)];
    }
  }

  *pName++ = '-';

  // Insert the type
  if (pFileId->type == 0xFFFF)
  {
    *pName++ = '*';
  }
  else
  {
    for (i=0; i<4; i++)
    {
      *pName++ = HEX_char[UINT16_BREAK_HEX(pFileId->type, i)];
    }
  }

  *pName++ = '-';

  // Insert the version
  if (pFileId->version == 0xFFFFFFFF)
  {
    *pName++ = '*';
  }
  else
  {
    for (i=0; i<8; i++)
    {
      *pName++ = HEX_char[UINT32_BREAK_HEX(pFileId->version, i)];
    }
  }

  if (text)
  {
    len = (uint8) strlen(text);

    if (len)
    {
      *pName++ = '-';
      osal_memcpy(pName, text, len);
      pName += len;
    }
  }

  osal_memcpy(pName, ".zigbee", 8);
}

/******************************************************************************
 * @fn      OTA_SplitFileName
 *
 * @brief   Get the file ID of an image from the filename text
 *
 * @param   pName - Buffer to hold the name
 *          pFileID - Pointer to File ID structure
 *
 * @return  none
 */
void OTA_SplitFileName(char *pName, zclOTA_FileID_t *pFileId)
{
  if (pName && pFileId)
  {
    uint8 len = (uint8) strlen(pName);

    if (len >= 19)
    {
      uint8 i;

      pFileId->manufacturer = 0;
      for (i=0; i<4; i++)
      {
        pFileId->manufacturer |= ((uint16) char2uint(*pName++)) << (12 - (4*i));
      }

      pName++;
      pFileId->type = 0;
      for (i=0; i<4; i++)
      {
        pFileId->type |= ((uint16) char2uint(*pName++)) << (12 - (4*i));
      }

      pName++;
      pFileId->version = 0;
      for (i=0; i<8; i++)
      {
        pFileId->version |= ((uint32) char2uint(*pName++)) << (28 - (4*i));
      }
    }
  }
}

/******************************************************************************
 * @fn      OTA_FileIdToStream
 *
 * @brief   Writes a file ID to a stream
 *
 * @param   pFileId - File ID
 *          pStream - Stream
 *
 * @return  new stream pointer
 */
uint8 *OTA_FileIdToStream(zclOTA_FileID_t *pFileId, uint8 *pStream)
{
  if (pStream)
  {
    *pStream++ = LO_UINT16(pFileId->manufacturer);
    *pStream++ = HI_UINT16(pFileId->manufacturer);

    *pStream++ = LO_UINT16(pFileId->type);
    *pStream++ = HI_UINT16(pFileId->type);

    *pStream++ = BREAK_UINT32(pFileId->version, 0);
    *pStream++ = BREAK_UINT32(pFileId->version, 1);
    *pStream++ = BREAK_UINT32(pFileId->version, 2);
    *pStream++ = BREAK_UINT32(pFileId->version, 3);
  }

  return pStream;
}

/******************************************************************************
 * @fn      OTA_StreamToFileId
 *
 * @brief   Reads a file ID from a stream
 *
 * @param   pFileId - File ID
 *          pStream - Stream
 *
 * @return  new stream pointer
 */
uint8 *OTA_StreamToFileId(zclOTA_FileID_t *pFileId, uint8 *pStream)
{
  if (pStream)
  {
    pFileId->manufacturer = BUILD_UINT16(pStream[0], pStream[1]);
    pStream += 2;
    pFileId->type = BUILD_UINT16(pStream[0], pStream[1]);
    pStream += 2;
    pFileId->version = BUILD_UINT32(pStream[0], pStream[1], pStream[2], pStream[3]);
    pStream += 4;
  }

  return pStream;
}

/******************************************************************************
 * @fn      OTA_AfAddrToStream
 *
 * @brief   Writes an afAddrType_t to a stream.
 *
 * @param   pAddr - Address
 *          pStream - Stream
 *
 * @return  new stream pointer
 *****************************************************************************/
uint8 *OTA_AfAddrToStream(afAddrType_t *pAddr, uint8 *pStream)
{
  if (pAddr && pStream)
  {
    *pStream++ = pAddr->addrMode;

    if (pAddr->addrMode == afAddr16Bit)
    {
      *pStream++ = LO_UINT16(pAddr->addr.shortAddr);
      *pStream++ = HI_UINT16(pAddr->addr.shortAddr);
    }
    else if (pAddr->addrMode == afAddr64Bit)
    {
      osal_memcpy(pStream, pAddr->addr.extAddr, Z_EXTADDR_LEN);
      pStream += Z_EXTADDR_LEN;
    }

    *pStream++ = pAddr->endPoint;

    *pStream++ = LO_UINT16(pAddr->panId);
    *pStream++ = HI_UINT16(pAddr->panId);
  }

  return pStream;
}

/******************************************************************************
 * @fn      OTA_StreamToAfAddr
 *
 * @brief   Reads an afAddrType_t from a stream.
 *
 * @param   pAddr - Address
 *          pStream - Stream
 *
 * @return  new stream pointer
 *****************************************************************************/
uint8 *OTA_StreamToAfAddr(afAddrType_t *pAddr, uint8 *pStream)
{
  if (pAddr && pStream)
  {
    pAddr->addrMode = (afAddrMode_t) *pStream++;

    if (pAddr->addrMode == afAddr16Bit)
    {
      pAddr->addr.shortAddr = BUILD_UINT16(pStream[0], pStream[1]);
      pStream+= 2;
    }
    else if (pAddr->addrMode == afAddr64Bit)
    {
      osal_memcpy(pAddr->addr.extAddr, pStream, Z_EXTADDR_LEN);
      pStream += Z_EXTADDR_LEN;
    }

    pAddr->endPoint = *pStream++;
    pAddr->panId = BUILD_UINT16(pStream[0], pStream[1]);
    pStream+= 2;
  }

  return pStream;
}
