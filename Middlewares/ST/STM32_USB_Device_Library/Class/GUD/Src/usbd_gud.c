/**
  ******************************************************************************
  * @file    usbd_gud.c
  * @author  Fredrik Andersson
  * @brief   This file provides the high layer firmware functions to manage the
  *          following functionalities of the USB GUD (Generic USB Display) Class:
  *           - Initialization and Configuration of high and low layer
  *           - Enumeration as GUD Device
  *           - GUD vendor requests management
  *           - Pixel data reception on the bulk OUT endpoint
  *           - Error management
  *
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 Fredrik Andersson.
  * All rights reserved.
  *
  * SPDX-License-Identifier: MIT
  *
  ******************************************************************************
  *  @verbatim
  *
  *          ===================================================================
  *                                GUD Class Driver Description
  *          ===================================================================
  *           This driver manages the GUD (Generic USB Display) protocol, as
  *           specified by include/drm/gud.h in the Linux kernel and driven by
  *           the Linux gud DRM driver (Linux 5.13 or later).
  *           This driver implements the following aspects of the protocol:
  *             - Device descriptor management (VID 0x1D50, PID 0x614D)
  *             - Configuration descriptor management
  *             - Enumeration as a vendor specific interface with 1 bulk OUT endpoint
  *             - Display, format, property, connector and mode requests
  *             - State check/commit and controller/display enable requests
  *             - Uncompressed pixel data transfers announced by GUD_REQ_SET_BUFFER
  *
  *           The display is described by the application in the interface
  *           structure (USBD_GUD_ItfTypeDef), which also receives the state
  *           changes and the received rectangles.
  *
  *           Control OUT requests can not be rejected once their data has been
  *           received, so the class announces GUD_DISPLAY_FLAG_STATUS_ON_SET:
  *           the host reads the result of each one with GUD_REQ_GET_STATUS.
  *
  *            This driver doesn't implement the following aspects of the protocol
  *            (but it is possible to manage these features with some modifications on this driver):
  *             - LZ4 compression
  *             - EDID, TV modes and connector properties
  *             - More than one connector
  *
  *  @endverbatim
  *
  ******************************************************************************
  */

/* BSPDependencies
- None
EndBSPDependencies */

/* Includes ------------------------------------------------------------------*/
#include "usbd_gud.h"
#include "usbd_ctlreq.h"


/** @addtogroup STM32_USB_DEVICE_LIBRARY
  * @{
  */


/** @defgroup USBD_GUD
  * @brief usbd core module
  * @{
  */

/** @defgroup USBD_GUD_Private_TypesDefinitions
  * @{
  */
/**
  * @}
  */


/** @defgroup USBD_GUD_Private_Defines
  * @{
  */
#define GUD_RX_IDLE                                 0U
#define GUD_RX_BUSY                                 1U

#define GUD_CMD_NONE                                0xFFU
/**
  * @}
  */


/** @defgroup USBD_GUD_Private_Macros
  * @{
  */
#define GUD_ITF(pdev)    ((USBD_GUD_ItfTypeDef *)(pdev)->pUserData[(pdev)->classId])
/**
  * @}
  */


/** @defgroup USBD_GUD_Private_FunctionPrototypes
  * @{
  */

static uint8_t USBD_GUD_Init(USBD_HandleTypeDef *pdev, uint8_t cfgidx);
static uint8_t USBD_GUD_DeInit(USBD_HandleTypeDef *pdev, uint8_t cfgidx);
static uint8_t USBD_GUD_Setup(USBD_HandleTypeDef *pdev, USBD_SetupReqTypedef *req);
static uint8_t USBD_GUD_DataOut(USBD_HandleTypeDef *pdev, uint8_t epnum);
static uint8_t USBD_GUD_EP0_RxReady(USBD_HandleTypeDef *pdev);
static uint8_t *USBD_GUD_GetFSCfgDesc(uint16_t *length);
static uint8_t *USBD_GUD_GetHSCfgDesc(uint16_t *length);
static uint8_t *USBD_GUD_GetOtherSpeedCfgDesc(uint16_t *length);
uint8_t *USBD_GUD_GetDeviceQualifierDescriptor(uint16_t *length);

static uint8_t USBD_GUD_VendorGet(USBD_HandleTypeDef *pdev, USBD_SetupReqTypedef *req);
static uint8_t USBD_GUD_VendorSet(USBD_HandleTypeDef *pdev, uint8_t cmd,
                                  uint8_t *pbuf, uint16_t length);
static uint8_t USBD_GUD_SetBuffer(USBD_HandleTypeDef *pdev, uint8_t *pbuf, uint16_t length);
static uint8_t USBD_GUD_SetStateCheck(USBD_HandleTypeDef *pdev, uint8_t *pbuf, uint16_t length);
static uint32_t USBD_GUD_LineLength(uint8_t format, uint32_t width);

/* USB Standard Device Descriptor */
__ALIGN_BEGIN static uint8_t USBD_GUD_DeviceQualifierDesc[USB_LEN_DEV_QUALIFIER_DESC] __ALIGN_END =
{
  USB_LEN_DEV_QUALIFIER_DESC,
  USB_DESC_TYPE_DEVICE_QUALIFIER,
  0x00,
  0x02,
  0x00,
  0x00,
  0x00,
  0x40,
  0x01,
  0x00,
};
/**
  * @}
  */

/** @defgroup USBD_GUD_Private_Variables
  * @{
  */


/* GUD interface class callbacks structure */
USBD_ClassTypeDef  USBD_GUD =
{
  USBD_GUD_Init,
  USBD_GUD_DeInit,
  USBD_GUD_Setup,
  NULL,                 /* EP0_TxSent */
  USBD_GUD_EP0_RxReady,
  NULL,                 /* DataIn */
  USBD_GUD_DataOut,
  NULL,                 /* SOF */
  NULL,
  NULL,
  USBD_GUD_GetHSCfgDesc,
  USBD_GUD_GetFSCfgDesc,
  USBD_GUD_GetOtherSpeedCfgDesc,
  USBD_GUD_GetDeviceQualifierDescriptor,
};

/* USB GUD device Configuration Descriptor */
#define USBD_GUD_CFG_DESC(packet_size)                                        \
  {                                                                           \
    /* Configuration Descriptor */                                            \
    0x09,                                   /* bLength: Configuration Descriptor size */ \
    USB_DESC_TYPE_CONFIGURATION,            /* bDescriptorType: Configuration */ \
    USB_GUD_CONFIG_DESC_SIZ,                /* wTotalLength */                \
    0x00,                                                                     \
    0x01,                                   /* bNumInterfaces: 1 interface */ \
    0x01,                                   /* bConfigurationValue */         \
    0x00,                                   /* iConfiguration */              \
    0xC0,                                   /* bmAttributes: self powered */  \
    0x32,                                   /* MaxPower 100 mA */             \
                                                                              \
    /* Interface Descriptor, the gud driver binds to vendor specific interfaces */ \
    0x09,                                   /* bLength: Interface Descriptor size */ \
    USB_DESC_TYPE_INTERFACE,                /* bDescriptorType: Interface */  \
    0x00,                                   /* bInterfaceNumber */            \
    0x00,                                   /* bAlternateSetting */           \
    0x01,                                   /* bNumEndpoints */               \
    0xFF,                                   /* bInterfaceClass: Vendor */     \
    0x00,                                   /* bInterfaceSubClass */          \
    0x00,                                   /* bInterfaceProtocol */          \
    0x00,                                   /* iInterface */                  \
                                                                              \
    /* Endpoint OUT Descriptor, pixel data */                                 \
    0x07,                                   /* bLength: Endpoint Descriptor size */ \
    USB_DESC_TYPE_ENDPOINT,                 /* bDescriptorType: Endpoint */   \
    GUD_OUT_EP,                             /* bEndpointAddress */            \
    0x02,                                   /* bmAttributes: Bulk */          \
    LOBYTE(packet_size),                    /* wMaxPacketSize */              \
    HIBYTE(packet_size),                                                      \
    0x00                                    /* bInterval */                   \
  }

__ALIGN_BEGIN static uint8_t USBD_GUD_CfgHSDesc[USB_GUD_CONFIG_DESC_SIZ] __ALIGN_END =
  USBD_GUD_CFG_DESC(GUD_DATA_HS_MAX_PACKET_SIZE);

__ALIGN_BEGIN static uint8_t USBD_GUD_CfgFSDesc[USB_GUD_CONFIG_DESC_SIZ] __ALIGN_END =
  USBD_GUD_CFG_DESC(GUD_DATA_FS_MAX_PACKET_SIZE);

__ALIGN_BEGIN static uint8_t USBD_GUD_OtherSpeedCfgDesc[USB_GUD_CONFIG_DESC_SIZ] __ALIGN_END =
  USBD_GUD_CFG_DESC(GUD_DATA_FS_MAX_PACKET_SIZE);

/**
  * @}
  */

/** @defgroup USBD_GUD_Private_Functions
  * @{
  */

/**
  * @brief  USBD_GUD_Init
  *         Initialize the GUD interface
  * @param  pdev: device instance
  * @param  cfgidx: Configuration index
  * @retval status
  */
static uint8_t USBD_GUD_Init(USBD_HandleTypeDef *pdev, uint8_t cfgidx)
{
  UNUSED(cfgidx);
  USBD_GUD_HandleTypeDef *hgud;

  hgud = (USBD_GUD_HandleTypeDef *)USBD_malloc(sizeof(USBD_GUD_HandleTypeDef));

  if (hgud == NULL)
  {
    pdev->pClassDataCmsit[pdev->classId] = NULL;
    return (uint8_t)USBD_EMEM;
  }

  (void)USBD_memset(hgud, 0, sizeof(USBD_GUD_HandleTypeDef));

  pdev->pClassDataCmsit[pdev->classId] = (void *)hgud;
  pdev->pClassData = pdev->pClassDataCmsit[pdev->classId];

  if (pdev->dev_speed == USBD_SPEED_HIGH)
  {
    /* Open EP OUT */
    (void)USBD_LL_OpenEP(pdev, GUD_OUT_EP, USBD_EP_TYPE_BULK,
                         GUD_DATA_HS_MAX_PACKET_SIZE);
  }
  else
  {
    /* Open EP OUT */
    (void)USBD_LL_OpenEP(pdev, GUD_OUT_EP, USBD_EP_TYPE_BULK,
                         GUD_DATA_FS_MAX_PACKET_SIZE);
  }
  pdev->ep_out[GUD_OUT_EP & 0xFU].is_used = 1U;

  hgud->CmdOpCode = GUD_CMD_NONE;
  hgud->Status = GUD_STATUS_OK;
  hgud->RxState = GUD_RX_IDLE;
  hgud->RxBuffer = NULL;

  /* Init physical Interface components, it sets the receive buffer */
  (void)GUD_ITF(pdev)->Init();

  if (hgud->RxBuffer == NULL)
  {
    return (uint8_t)USBD_EMEM;
  }

  /* The OUT endpoint is armed by each GUD_REQ_SET_BUFFER, until then it NAKs */
  return (uint8_t)USBD_OK;
}

/**
  * @brief  USBD_GUD_DeInit
  *         DeInitialize the GUD layer
  * @param  pdev: device instance
  * @param  cfgidx: Configuration index
  * @retval status
  */
static uint8_t USBD_GUD_DeInit(USBD_HandleTypeDef *pdev, uint8_t cfgidx)
{
  UNUSED(cfgidx);

  /* Close EP OUT */
  (void)USBD_LL_CloseEP(pdev, GUD_OUT_EP);
  pdev->ep_out[GUD_OUT_EP & 0xFU].is_used = 0U;

  /* DeInit physical Interface components */
  if (pdev->pClassDataCmsit[pdev->classId] != NULL)
  {
    (void)GUD_ITF(pdev)->DeInit();
    (void)USBD_free(pdev->pClassDataCmsit[pdev->classId]);
    pdev->pClassDataCmsit[pdev->classId] = NULL;
    pdev->pClassData = NULL;
  }

  return (uint8_t)USBD_OK;
}

/**
  * @brief  USBD_GUD_Setup
  *         Handle the GUD specific requests
  * @param  pdev: instance
  * @param  req: usb requests
  * @retval status
  */
static uint8_t USBD_GUD_Setup(USBD_HandleTypeDef *pdev,
                              USBD_SetupReqTypedef *req)
{
  USBD_GUD_HandleTypeDef *hgud = (USBD_GUD_HandleTypeDef *)pdev->pClassDataCmsit[pdev->classId];
  uint8_t ifalt = 0U;
  uint16_t status_info = 0U;
  USBD_StatusTypeDef ret = USBD_OK;

  if (hgud == NULL)
  {
    return (uint8_t)USBD_FAIL;
  }

  switch (req->bmRequest & USB_REQ_TYPE_MASK)
  {
    case USB_REQ_TYPE_VENDOR:
      if ((req->bmRequest & 0x80U) != 0U)
      {
        ret = (USBD_StatusTypeDef)USBD_GUD_VendorGet(pdev, req);
      }
      else if (req->wLength == 0U)
      {
        ret = (USBD_StatusTypeDef)USBD_GUD_VendorSet(pdev, req->bRequest, NULL, 0U);
      }
      else if (req->wLength <= GUD_REQ_MAX_DATA_SIZE)
      {
        /* The request is handled in EP0_RxReady once its data has arrived */
        hgud->CmdOpCode = req->bRequest;
        hgud->CmdLength = req->wLength;

        (void)USBD_CtlPrepareRx(pdev, (uint8_t *)hgud->data, hgud->CmdLength);
      }
      else
      {
        hgud->Status = GUD_STATUS_PROTOCOL_ERROR;
        USBD_CtlError(pdev, req);
        ret = USBD_FAIL;
      }
      break;

    case USB_REQ_TYPE_STANDARD:
      switch (req->bRequest)
      {
        case USB_REQ_GET_STATUS:
          if (pdev->dev_state == USBD_STATE_CONFIGURED)
          {
            (void)USBD_CtlSendData(pdev, (uint8_t *)&status_info, 2U);
          }
          else
          {
            USBD_CtlError(pdev, req);
            ret = USBD_FAIL;
          }
          break;

        case USB_REQ_GET_INTERFACE:
          if (pdev->dev_state == USBD_STATE_CONFIGURED)
          {
            (void)USBD_CtlSendData(pdev, &ifalt, 1U);
          }
          else
          {
            USBD_CtlError(pdev, req);
            ret = USBD_FAIL;
          }
          break;

        case USB_REQ_SET_INTERFACE:
          if (pdev->dev_state != USBD_STATE_CONFIGURED)
          {
            USBD_CtlError(pdev, req);
            ret = USBD_FAIL;
          }
          break;

        case USB_REQ_CLEAR_FEATURE:
          break;

        default:
          USBD_CtlError(pdev, req);
          ret = USBD_FAIL;
          break;
      }
      break;

    default:
      USBD_CtlError(pdev, req);
      ret = USBD_FAIL;
      break;
  }

  return (uint8_t)ret;
}

/**
  * @brief  USBD_GUD_VendorGet
  *         Answer a GUD request with data to the host
  * @param  pdev: device instance
  * @param  req: usb request
  * @retval status
  */
static uint8_t USBD_GUD_VendorGet(USBD_HandleTypeDef *pdev, USBD_SetupReqTypedef *req)
{
  USBD_GUD_HandleTypeDef *hgud = (USBD_GUD_HandleTypeDef *)pdev->pClassDataCmsit[pdev->classId];
  const USBD_GUD_DisplayTypeDef *display = GUD_ITF(pdev)->pDisplay;
  uint8_t *pbuf = (uint8_t *)hgud->data;
  uint16_t len = 0U;
  uint8_t status = GUD_STATUS_OK;

  switch (req->bRequest)
  {
    case GUD_REQ_GET_STATUS:
      /* Reports the last request, so it must not change the status itself */
      (void)USBD_CtlSendData(pdev, &hgud->Status, MIN(1U, req->wLength));
      return (uint8_t)USBD_OK;

    case GUD_REQ_GET_DESCRIPTOR:
    {
      USBD_GUD_DisplayDescReqTypeDef *desc = (USBD_GUD_DisplayDescReqTypeDef *)pbuf;
      desc->magic = GUD_DISPLAY_MAGIC;
      desc->version = GUD_PROTOCOL_VERSION;
      desc->flags = GUD_DISPLAY_FLAG_STATUS_ON_SET;
      desc->compression = 0U;
      desc->max_buffer_size = hgud->RxBufferSize;
      desc->min_width = display->Width;
      desc->max_width = display->Width;
      desc->min_height = display->Height;
      desc->max_height = display->Height;
      len = sizeof(USBD_GUD_DisplayDescReqTypeDef);
      break;
    }

    case GUD_REQ_GET_FORMATS:
      len = MIN(display->NumFormats, GUD_REQ_MAX_DATA_SIZE);
      (void)USBD_memcpy(pbuf, display->pFormats, len);
      break;

    case GUD_REQ_GET_PROPERTIES:
    {
      /* Rotation 0 is mandatory */
      USBD_GUD_PropertyReqTypeDef *prop = (USBD_GUD_PropertyReqTypeDef *)pbuf;
      prop->prop = GUD_PROPERTY_ROTATION;
      prop->val = GUD_ROTATION_0;
      len = sizeof(USBD_GUD_PropertyReqTypeDef);
      break;
    }

    case GUD_REQ_GET_CONNECTORS:
    {
      USBD_GUD_ConnectorDescReqTypeDef *connector = (USBD_GUD_ConnectorDescReqTypeDef *)pbuf;
      connector->connector_type = GUD_CONNECTOR_TYPE_PANEL;
      connector->flags = GUD_CONNECTOR_FLAGS_POLL_STATUS;
      len = sizeof(USBD_GUD_ConnectorDescReqTypeDef);
      break;
    }

    case GUD_REQ_GET_CONNECTOR_PROPERTIES:
    case GUD_REQ_GET_CONNECTOR_STATUS:
    case GUD_REQ_GET_CONNECTOR_MODES:
      /* Connector requests carry the connector index in wValue, there is one connector */
      if (req->wValue != 0U)
      {
        status = GUD_STATUS_INVALID_PARAMETER;
      }
      else if (req->bRequest == GUD_REQ_GET_CONNECTOR_STATUS)
      {
        pbuf[0] = GUD_CONNECTOR_STATUS_CONNECTED;
        if (GUD_ITF(pdev)->GetConnectorStatus != NULL)
        {
          (void)GUD_ITF(pdev)->GetConnectorStatus(&pbuf[0]);
        }
        len = 1U;
      }
      else if (req->bRequest == GUD_REQ_GET_CONNECTOR_MODES)
      {
        /* No blanking, the clock gives the refresh rate */
        USBD_GUD_DisplayModeReqTypeDef *mode = (USBD_GUD_DisplayModeReqTypeDef *)pbuf;
        mode->clock = ((uint32_t)display->Width * display->Height * display->RefreshRate) / 1000U;
        mode->hdisplay = display->Width;
        mode->hsync_start = display->Width;
        mode->hsync_end = display->Width;
        mode->htotal = display->Width;
        mode->vdisplay = display->Height;
        mode->vsync_start = display->Height;
        mode->vsync_end = display->Height;
        mode->vtotal = display->Height;
        mode->flags = GUD_DISPLAY_MODE_FLAG_PREFERRED;
        len = sizeof(USBD_GUD_DisplayModeReqTypeDef);
      }
      else
      {
        /* No connector properties, an empty answer */
        len = 0U;
      }
      break;

    default:
      /* EDID and TV modes are not supported */
      status = GUD_STATUS_REQUEST_NOT_SUPPORTED;
      break;
  }

  hgud->Status = status;

  if (status != GUD_STATUS_OK)
  {
    /* The host reads the status after the stall */
    USBD_CtlError(pdev, req);
    return (uint8_t)USBD_FAIL;
  }

  /* An empty answer is a zero length data stage */
  (void)USBD_CtlSendData(pdev, pbuf, MIN(len, req->wLength));

  return (uint8_t)USBD_OK;
}

/**
  * @brief  USBD_GUD_VendorSet
  *         Handle a GUD request with its data from the host
  * @param  pdev: device instance
  * @param  cmd: request number
  * @param  pbuf: request data
  * @param  length: request data length
  * @retval status
  */
static uint8_t USBD_GUD_VendorSet(USBD_HandleTypeDef *pdev, uint8_t cmd,
                                  uint8_t *pbuf, uint16_t length)
{
  USBD_GUD_HandleTypeDef *hgud = (USBD_GUD_HandleTypeDef *)pdev->pClassDataCmsit[pdev->classId];
  uint8_t status = GUD_STATUS_OK;

  switch (cmd)
  {
    case GUD_REQ_SET_CONNECTOR_FORCE_DETECT:
      break;

    case GUD_REQ_SET_BUFFER:
      status = USBD_GUD_SetBuffer(pdev, pbuf, length);
      break;

    case GUD_REQ_SET_STATE_CHECK:
      status = USBD_GUD_SetStateCheck(pdev, pbuf, length);
      break;

    case GUD_REQ_SET_STATE_COMMIT:
      if (hgud->StateChecked == 0U)
      {
        status = GUD_STATUS_PROTOCOL_ERROR;
        break;
      }
      hgud->Format = hgud->CheckedState.format;
      hgud->StateChecked = 0U;
      if (GUD_ITF(pdev)->SetState != NULL)
      {
        (void)GUD_ITF(pdev)->SetState(hgud->Format, &hgud->CheckedState.mode);
      }
      break;

    case GUD_REQ_SET_CONTROLLER_ENABLE:
    case GUD_REQ_SET_DISPLAY_ENABLE:
      if (length != 1U)
      {
        status = GUD_STATUS_PROTOCOL_ERROR;
        break;
      }
      if (cmd == GUD_REQ_SET_CONTROLLER_ENABLE)
      {
        hgud->ControllerEnabled = (pbuf[0] != 0U) ? 1U : 0U;
      }
      else
      {
        hgud->DisplayEnabled = (pbuf[0] != 0U) ? 1U : 0U;
      }
      if (GUD_ITF(pdev)->Enable != NULL)
      {
        (void)GUD_ITF(pdev)->Enable(hgud->ControllerEnabled, hgud->DisplayEnabled);
      }
      break;

    default:
      status = GUD_STATUS_REQUEST_NOT_SUPPORTED;
      break;
  }

  hgud->Status = status;

  /* The status stage is sent by the core, the host reads the result with GUD_REQ_GET_STATUS */
  return (uint8_t)USBD_OK;
}

/**
  * @brief  USBD_GUD_LineLength
  *         Bytes per line of a rectangle, lines below 8 bits per pixel are byte aligned
  * @param  format: GUD pixel format
  * @param  width: rectangle width
  * @retval line length in bytes, 0 for an unknown format
  */
static uint32_t USBD_GUD_LineLength(uint8_t format, uint32_t width)
{
  uint32_t bpp;

  switch (format)
  {
    case GUD_PIXEL_FORMAT_R1:
      bpp = 1U;
      break;
    case GUD_PIXEL_FORMAT_XRGB1111:
      bpp = 4U;
      break;
    case GUD_PIXEL_FORMAT_R8:
    case GUD_PIXEL_FORMAT_RGB332:
      bpp = 8U;
      break;
    case GUD_PIXEL_FORMAT_RGB565:
      bpp = 16U;
      break;
    case GUD_PIXEL_FORMAT_RGB888:
      bpp = 24U;
      break;
    case GUD_PIXEL_FORMAT_XRGB8888:
    case GUD_PIXEL_FORMAT_ARGB8888:
      bpp = 32U;
      break;
    default:
      bpp = 0U;
      break;
  }

  return ((width * bpp) + 7U) / 8U;
}

/**
  * @brief  USBD_GUD_SetBuffer
  *         Check the rectangle of the next bulk transfer and arm the OUT endpoint
  * @param  pdev: device instance
  * @param  pbuf: request data
  * @param  length: request data length
  * @retval GUD status
  */
static uint8_t USBD_GUD_SetBuffer(USBD_HandleTypeDef *pdev, uint8_t *pbuf, uint16_t length)
{
  USBD_GUD_HandleTypeDef *hgud = (USBD_GUD_HandleTypeDef *)pdev->pClassDataCmsit[pdev->classId];
  const USBD_GUD_DisplayTypeDef *display = GUD_ITF(pdev)->pDisplay;
  USBD_GUD_SetBufferReqTypeDef req;

  if (length != sizeof(req))
  {
    return GUD_STATUS_PROTOCOL_ERROR;
  }
  (void)USBD_memcpy(&req, pbuf, sizeof(req));

  if ((req.width == 0U) || (req.height == 0U) ||
      (req.x >= display->Width) || (req.y >= display->Height) ||
      (req.width > (display->Width - req.x)) || (req.height > (display->Height - req.y)) ||
      (hgud->Format == 0U) ||
      (req.length != (USBD_GUD_LineLength(hgud->Format, req.width) * req.height)))
  {
    return GUD_STATUS_INVALID_PARAMETER;
  }

  /* No compression is announced */
  if ((req.compression != 0U) || (req.length > hgud->RxBufferSize))
  {
    return GUD_STATUS_INVALID_PARAMETER;
  }

  hgud->BufferReq = req;
  hgud->RxLength = req.length;
  hgud->RxCount = 0U;
  hgud->RxChunk = MIN(hgud->RxLength, GUD_DATA_RX_CHUNK_SIZE);
  hgud->RxState = GUD_RX_BUSY;

  /* Prepare Out endpoint to receive the pixel data */
  (void)USBD_LL_PrepareReceive(pdev, GUD_OUT_EP, hgud->RxBuffer, hgud->RxChunk);

  return GUD_STATUS_OK;
}

/**
  * @brief  USBD_GUD_SetStateCheck
  *         Check a display state, it is applied by GUD_REQ_SET_STATE_COMMIT
  * @param  pdev: device instance
  * @param  pbuf: request data
  * @param  length: request data length
  * @retval GUD status
  */
static uint8_t USBD_GUD_SetStateCheck(USBD_HandleTypeDef *pdev, uint8_t *pbuf, uint16_t length)
{
  USBD_GUD_HandleTypeDef *hgud = (USBD_GUD_HandleTypeDef *)pdev->pClassDataCmsit[pdev->classId];
  const USBD_GUD_DisplayTypeDef *display = GUD_ITF(pdev)->pDisplay;
  USBD_GUD_StateReqTypeDef state;
  USBD_GUD_PropertyReqTypeDef prop;
  uint32_t offset;
  uint8_t idx;
  uint8_t offered = 0U;

  if ((length < sizeof(state)) ||
      (((length - sizeof(state)) % sizeof(USBD_GUD_PropertyReqTypeDef)) != 0U))
  {
    return GUD_STATUS_PROTOCOL_ERROR;
  }
  (void)USBD_memcpy(&state, pbuf, sizeof(state));

  for (idx = 0U; idx < display->NumFormats; idx++)
  {
    if (display->pFormats[idx] == state.format)
    {
      offered = 1U;
    }
  }

  if ((state.mode.hdisplay != display->Width) || (state.mode.vdisplay != display->Height) ||
      (offered == 0U) || (state.connector != 0U))
  {
    return GUD_STATUS_INVALID_PARAMETER;
  }

  for (offset = sizeof(state); offset < length; offset += sizeof(prop))
  {
    (void)USBD_memcpy(&prop, &pbuf[offset], sizeof(prop));
    if ((prop.prop == GUD_PROPERTY_ROTATION) && (prop.val != GUD_ROTATION_0))
    {
      return GUD_STATUS_INVALID_PARAMETER;
    }
  }

  hgud->CheckedState = state;
  hgud->StateChecked = 1U;

  return GUD_STATUS_OK;
}

/**
  * @brief  USBD_GUD_DataOut
  *         Data received on non-control Out endpoint
  * @param  pdev: device instance
  * @param  epnum: endpoint number
  * @retval status
  */
static uint8_t USBD_GUD_DataOut(USBD_HandleTypeDef *pdev, uint8_t epnum)
{
  USBD_GUD_HandleTypeDef *hgud = (USBD_GUD_HandleTypeDef *)pdev->pClassDataCmsit[pdev->classId];
  uint32_t count;

  if (hgud == NULL)
  {
    return (uint8_t)USBD_FAIL;
  }

  /* Get the received data length */
  count = USBD_LL_GetRxDataSize(pdev, epnum);

  if (hgud->RxState != GUD_RX_BUSY)
  {
    /* Pixel data without a matching GUD_REQ_SET_BUFFER, drop it */
    return (uint8_t)USBD_OK;
  }

  hgud->RxCount += count;

  if ((count == hgud->RxChunk) && (hgud->RxCount < hgud->RxLength))
  {
    /* Prepare Out endpoint to receive the next chunk */
    hgud->RxChunk = MIN(hgud->RxLength - hgud->RxCount, GUD_DATA_RX_CHUNK_SIZE);
    (void)USBD_LL_PrepareReceive(pdev, GUD_OUT_EP, &hgud->RxBuffer[hgud->RxCount],
                                 hgud->RxChunk);
    return (uint8_t)USBD_OK;
  }

  hgud->RxState = GUD_RX_IDLE;

  if (hgud->RxCount != hgud->RxLength)
  {
    /* A short transfer, the rectangle is dropped */
    hgud->Status = GUD_STATUS_ERROR;
    return (uint8_t)USBD_OK;
  }

  (void)GUD_ITF(pdev)->Flush(&hgud->BufferReq, hgud->RxBuffer);

  return (uint8_t)USBD_OK;
}

/**
  * @brief  USBD_GUD_EP0_RxReady
  *         Handle EP0 Rx Ready event
  * @param  pdev: device instance
  * @retval status
  */
static uint8_t USBD_GUD_EP0_RxReady(USBD_HandleTypeDef *pdev)
{
  USBD_GUD_HandleTypeDef *hgud = (USBD_GUD_HandleTypeDef *)pdev->pClassDataCmsit[pdev->classId];

  if (hgud == NULL)
  {
    return (uint8_t)USBD_FAIL;
  }

  if (hgud->CmdOpCode != GUD_CMD_NONE)
  {
    (void)USBD_GUD_VendorSet(pdev, hgud->CmdOpCode, (uint8_t *)hgud->data, hgud->CmdLength);
    hgud->CmdOpCode = GUD_CMD_NONE;
  }

  return (uint8_t)USBD_OK;
}

/**
  * @brief  USBD_GUD_GetFSCfgDesc
  *         Return configuration descriptor
  * @param  length : pointer data length
  * @retval pointer to descriptor buffer
  */
static uint8_t *USBD_GUD_GetFSCfgDesc(uint16_t *length)
{
  *length = (uint16_t)sizeof(USBD_GUD_CfgFSDesc);
  return USBD_GUD_CfgFSDesc;
}

/**
  * @brief  USBD_GUD_GetHSCfgDesc
  *         Return configuration descriptor
  * @param  length : pointer data length
  * @retval pointer to descriptor buffer
  */
static uint8_t *USBD_GUD_GetHSCfgDesc(uint16_t *length)
{
  *length = (uint16_t)sizeof(USBD_GUD_CfgHSDesc);
  return USBD_GUD_CfgHSDesc;
}

/**
  * @brief  USBD_GUD_GetOtherSpeedCfgDesc
  *         Return configuration descriptor
  * @param  length : pointer data length
  * @retval pointer to descriptor buffer
  */
static uint8_t *USBD_GUD_GetOtherSpeedCfgDesc(uint16_t *length)
{
  *length = (uint16_t)sizeof(USBD_GUD_OtherSpeedCfgDesc);
  return USBD_GUD_OtherSpeedCfgDesc;
}

/**
  * @brief  USBD_GUD_GetDeviceQualifierDescriptor
  *         return Device Qualifier descriptor
  * @param  length : pointer data length
  * @retval pointer to descriptor buffer
  */
uint8_t *USBD_GUD_GetDeviceQualifierDescriptor(uint16_t *length)
{
  *length = (uint16_t)sizeof(USBD_GUD_DeviceQualifierDesc);

  return USBD_GUD_DeviceQualifierDesc;
}

/**
  * @brief  USBD_GUD_RegisterInterface
  * @param  pdev: device instance
  * @param  fops: GUD Interface callback
  * @retval status
  */
uint8_t USBD_GUD_RegisterInterface(USBD_HandleTypeDef *pdev,
                                   USBD_GUD_ItfTypeDef *fops)
{
  if ((fops == NULL) || (fops->pDisplay == NULL) || (fops->Flush == NULL))
  {
    return (uint8_t)USBD_FAIL;
  }

  pdev->pUserData[pdev->classId] = fops;

  return (uint8_t)USBD_OK;
}

/**
  * @brief  USBD_GUD_SetRxBuffer
  *         Set the buffer pixel data is received into, its size is the
  *         largest transfer the host sends
  * @param  pdev: device instance
  * @param  pbuff: Rx Buffer
  * @param  size: Rx Buffer size
  * @retval status
  */
uint8_t USBD_GUD_SetRxBuffer(USBD_HandleTypeDef *pdev, uint8_t *pbuff, uint32_t size)
{
  USBD_GUD_HandleTypeDef *hgud = (USBD_GUD_HandleTypeDef *)pdev->pClassDataCmsit[pdev->classId];

  if (hgud == NULL)
  {
    return (uint8_t)USBD_FAIL;
  }

  hgud->RxBuffer = pbuff;
  hgud->RxBufferSize = size;

  return (uint8_t)USBD_OK;
}
/**
  * @}
  */

/**
  * @}
  */

/**
  * @}
  */
