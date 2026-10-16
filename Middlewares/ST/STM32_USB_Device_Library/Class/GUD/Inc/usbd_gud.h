/**
  ******************************************************************************
  * @file    usbd_gud.h
  * @author  Fredrik Andersson
  * @brief   Header file for the usbd_gud.c file.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 Fredrik Andersson.
  * All rights reserved.
  *
  * SPDX-License-Identifier: MIT
  *
  ******************************************************************************
  */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __USB_GUD_H
#define __USB_GUD_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include  "usbd_ioreq.h"

/** @addtogroup STM32_USB_DEVICE_LIBRARY
  * @{
  */

/** @defgroup usbd_gud
  * @brief This file is the Header file for usbd_gud.c
  * @{
  */


/** @defgroup usbd_gud_Exported_Defines
  * @{
  */
#ifndef GUD_OUT_EP
#define GUD_OUT_EP                                  0x01U  /* EP1 for pixel data OUT */
#endif /* GUD_OUT_EP */

#ifndef GUD_DATA_HS_MAX_PACKET_SIZE
#define GUD_DATA_HS_MAX_PACKET_SIZE                 512U   /* Endpoint OUT max packet size */
#endif /* GUD_DATA_HS_MAX_PACKET_SIZE */

#ifndef GUD_DATA_FS_MAX_PACKET_SIZE
#define GUD_DATA_FS_MAX_PACKET_SIZE                 64U    /* Endpoint OUT max packet size */
#endif /* GUD_DATA_FS_MAX_PACKET_SIZE */

/* The OTG core counts at most 1023 packets per transfer, so pixel data is
   received in chunks of this size */
#ifndef GUD_DATA_RX_CHUNK_SIZE
#define GUD_DATA_RX_CHUNK_SIZE                      (32U * 1024U)
#endif /* GUD_DATA_RX_CHUNK_SIZE */

/* Largest control OUT request the class accepts */
#ifndef GUD_REQ_MAX_DATA_SIZE
#define GUD_REQ_MAX_DATA_SIZE                       128U
#endif /* GUD_REQ_MAX_DATA_SIZE */

#define USB_GUD_CONFIG_DESC_SIZ                     25U

/*---------------------------------------------------------------------*/
/*  GUD protocol definitions, see include/drm/gud.h in the Linux kernel */
/*---------------------------------------------------------------------*/

/* The Linux gud driver only binds to these ids */
#define GUD_VID                                     0x1D50U
#define GUD_PID                                     0x614DU

#define GUD_DISPLAY_MAGIC                           0x1D50614DU
#define GUD_PROTOCOL_VERSION                        1U

/* Display descriptor flags */
#define GUD_DISPLAY_FLAG_STATUS_ON_SET              0x01U  /* Host reads status after each SET */
#define GUD_DISPLAY_FLAG_FULL_UPDATE                0x02U  /* Host always sends the full frame */

/* Compression */
#define GUD_COMPRESSION_LZ4                         0x01U

/* Requests, vendor requests to the interface */
#define GUD_REQ_GET_STATUS                          0x00U
#define GUD_REQ_GET_DESCRIPTOR                      0x01U
#define GUD_REQ_GET_FORMATS                         0x40U
#define GUD_REQ_GET_PROPERTIES                      0x41U
#define GUD_REQ_GET_CONNECTORS                      0x50U
#define GUD_REQ_GET_CONNECTOR_PROPERTIES            0x51U
#define GUD_REQ_GET_CONNECTOR_TV_MODE_VALUES        0x52U
#define GUD_REQ_SET_CONNECTOR_FORCE_DETECT          0x53U
#define GUD_REQ_GET_CONNECTOR_STATUS                0x54U
#define GUD_REQ_GET_CONNECTOR_MODES                 0x55U
#define GUD_REQ_GET_CONNECTOR_EDID                  0x56U
#define GUD_REQ_SET_BUFFER                          0x60U
#define GUD_REQ_SET_STATE_CHECK                     0x61U
#define GUD_REQ_SET_STATE_COMMIT                    0x62U
#define GUD_REQ_SET_CONTROLLER_ENABLE               0x63U
#define GUD_REQ_SET_DISPLAY_ENABLE                  0x64U

/* Status of the last request, read with GUD_REQ_GET_STATUS */
#define GUD_STATUS_OK                               0x00U
#define GUD_STATUS_BUSY                             0x01U
#define GUD_STATUS_REQUEST_NOT_SUPPORTED            0x02U
#define GUD_STATUS_PROTOCOL_ERROR                   0x03U
#define GUD_STATUS_INVALID_PARAMETER                0x04U
#define GUD_STATUS_ERROR                            0x05U

/* Pixel formats, little endian like the DRM formats of the same name */
#define GUD_PIXEL_FORMAT_R1                         0x01U
#define GUD_PIXEL_FORMAT_R8                         0x08U
#define GUD_PIXEL_FORMAT_XRGB1111                   0x20U
#define GUD_PIXEL_FORMAT_RGB332                     0x30U
#define GUD_PIXEL_FORMAT_RGB565                     0x40U
#define GUD_PIXEL_FORMAT_RGB888                     0x50U
#define GUD_PIXEL_FORMAT_XRGB8888                   0x80U
#define GUD_PIXEL_FORMAT_ARGB8888                   0x81U

/* Plane rotation property, its value is a mask of supported rotations */
#define GUD_PROPERTY_ROTATION                       50U
#define GUD_ROTATION_0                              0x01U

/* Connector */
#define GUD_CONNECTOR_TYPE_PANEL                    0U
#define GUD_CONNECTOR_FLAGS_POLL_STATUS             0x01U  /* Host polls status every 10 s */
#define GUD_CONNECTOR_STATUS_DISCONNECTED           0x00U
#define GUD_CONNECTOR_STATUS_CONNECTED              0x01U

/* Display mode flags */
#define GUD_DISPLAY_MODE_FLAG_PREFERRED             0x400U
/**
  * @}
  */


/** @defgroup USBD_GUD_Exported_TypesDefinitions
  * @{
  */

/* GUD_REQ_GET_DESCRIPTOR: what the display is and supports */
typedef __PACKED_STRUCT
{
  uint32_t magic;
  uint8_t  version;
  uint32_t flags;
  uint8_t  compression;
  uint32_t max_buffer_size;
  uint32_t min_width;
  uint32_t max_width;
  uint32_t min_height;
  uint32_t max_height;
} USBD_GUD_DisplayDescReqTypeDef;

/* A property and its value */
typedef __PACKED_STRUCT
{
  uint16_t prop;
  uint64_t val;
} USBD_GUD_PropertyReqTypeDef;

/* A display mode */
typedef __PACKED_STRUCT
{
  uint32_t clock;   /* kHz */
  uint16_t hdisplay;
  uint16_t hsync_start;
  uint16_t hsync_end;
  uint16_t htotal;
  uint16_t vdisplay;
  uint16_t vsync_start;
  uint16_t vsync_end;
  uint16_t vtotal;
  uint32_t flags;
} USBD_GUD_DisplayModeReqTypeDef;

/* A connector */
typedef __PACKED_STRUCT
{
  uint8_t  connector_type;
  uint32_t flags;
} USBD_GUD_ConnectorDescReqTypeDef;

/* GUD_REQ_SET_BUFFER: the rectangle the next bulk transfer updates */
typedef __PACKED_STRUCT
{
  uint32_t x;
  uint32_t y;
  uint32_t width;
  uint32_t height;
  uint32_t length;
  uint8_t  compression;
  uint32_t compressed_length;
} USBD_GUD_SetBufferReqTypeDef;

/* GUD_REQ_SET_STATE_CHECK, followed by an array of USBD_GUD_PropertyReqTypeDef */
typedef __PACKED_STRUCT
{
  USBD_GUD_DisplayModeReqTypeDef mode;
  uint8_t  format;
  uint8_t  connector;
} USBD_GUD_StateReqTypeDef;

/* The display, as the application describes it */
typedef struct
{
  uint16_t Width;
  uint16_t Height;
  uint16_t RefreshRate;                   /* Hz */
  const uint8_t *pFormats;                /* Pixel formats, in order of preference */
  uint8_t  NumFormats;
} USBD_GUD_DisplayTypeDef;

typedef struct _USBD_GUD_Itf
{
  const USBD_GUD_DisplayTypeDef *pDisplay;
  int8_t (* Init)(void);
  int8_t (* DeInit)(void);
  int8_t (* SetState)(uint8_t format, USBD_GUD_DisplayModeReqTypeDef *mode);
  int8_t (* Enable)(uint8_t controller, uint8_t display);
  int8_t (* GetConnectorStatus)(uint8_t *status);
  int8_t (* Flush)(USBD_GUD_SetBufferReqTypeDef *rect, uint8_t *pbuf);
} USBD_GUD_ItfTypeDef;


typedef struct
{
  uint32_t data[GUD_REQ_MAX_DATA_SIZE / 4U];      /* Force 32-bit alignment */
  uint8_t  CmdOpCode;
  uint16_t CmdLength;
  uint8_t  Status;                                /* Result of the last request */
  uint8_t  StateChecked;
  USBD_GUD_StateReqTypeDef CheckedState;
  uint8_t  Format;                                /* Committed format, 0 when none */
  uint8_t  ControllerEnabled;
  uint8_t  DisplayEnabled;

  USBD_GUD_SetBufferReqTypeDef BufferReq;
  uint8_t  *RxBuffer;
  uint32_t RxBufferSize;
  uint32_t RxLength;
  uint32_t RxCount;
  uint32_t RxChunk;
  __IO uint32_t RxState;
} USBD_GUD_HandleTypeDef;


/** @defgroup USBD_GUD_Exported_Macros
  * @{
  */

/**
  * @}
  */

/** @defgroup USBD_GUD_Exported_Variables
  * @{
  */

extern USBD_ClassTypeDef USBD_GUD;
#define USBD_GUD_CLASS &USBD_GUD
/**
  * @}
  */

/** @defgroup USB_GUD_Exported_Functions
  * @{
  */
uint8_t USBD_GUD_RegisterInterface(USBD_HandleTypeDef *pdev,
                                   USBD_GUD_ItfTypeDef *fops);

uint8_t USBD_GUD_SetRxBuffer(USBD_HandleTypeDef *pdev, uint8_t *pbuff, uint32_t size);
/**
  * @}
  */

#ifdef __cplusplus
}
#endif

#endif /* __USB_GUD_H */
/**
  * @}
  */

/**
  * @}
  */
