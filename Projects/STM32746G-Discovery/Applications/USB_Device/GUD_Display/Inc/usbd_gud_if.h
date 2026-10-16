/**
  ******************************************************************************
  * @file    USB_Device/GUD_Display/Inc/usbd_gud_if.h
  * @author  Fredrik Andersson
  * @brief   Header for usbd_gud_if.c file.
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
#ifndef __USBD_GUD_IF_H
#define __USBD_GUD_IF_H

/* Includes ------------------------------------------------------------------*/
#include "usbd_gud.h"

/* Exported types ------------------------------------------------------------*/

/* What the host has done with the display, read by the main loop */
typedef struct
{
  __IO uint8_t  Configured;          /* The host has configured the device */
  __IO uint8_t  ControllerEnabled;
  __IO uint8_t  DisplayEnabled;
  __IO uint8_t  Format;              /* Committed pixel format, 0 when none */
  __IO uint32_t Updates;             /* Rectangles drawn */
  __IO uint32_t Bytes;               /* Pixel data bytes drawn */
  __IO uint32_t Errors;              /* Rectangles that could not be drawn */
  USBD_GUD_SetBufferReqTypeDef LastRect;
} GUD_DisplayStatusTypeDef;

/* Exported constants --------------------------------------------------------*/

/* The connector is reported disconnected for this long after the host has
   configured the device. A desktop like GNOME only picks up the connector of
   a hotplugged display when its status changes, and misses the change while
   the display is being set up. */
#define GUD_CONNECTOR_CONNECT_DELAY_MS    5000U

/* Exported macro ------------------------------------------------------------*/
/* Exported variables --------------------------------------------------------*/
extern USBD_GUD_ItfTypeDef USBD_GUD_fops;
extern GUD_DisplayStatusTypeDef GUD_DisplayStatus;

/* Exported functions ------------------------------------------------------- */

#endif /* __USBD_GUD_IF_H */
