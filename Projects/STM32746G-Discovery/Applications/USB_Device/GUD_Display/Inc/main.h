/**
  ******************************************************************************
  * @file    USB_Device/GUD_Display/Inc/main.h
  * @author  Fredrik Andersson
  * @brief   Header for main.c module
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
#ifndef __MAIN_H
#define __MAIN_H

/* Includes ------------------------------------------------------------------*/
#include <stdio.h>
#include "stm32f7xx_hal.h"
#include "usbd_core.h"
#include "usbd_desc.h"
#include "usbd_gud.h"
#include "usbd_gud_if.h"
#include "stm32746g_discovery.h"
#include "stm32746g_discovery_lcd.h"
#include "stm32746g_discovery_sdram.h"

/* Exported types ------------------------------------------------------------*/
/* Exported constants --------------------------------------------------------*/

/* The RK043FN48H panel of the STM32746G-Discovery */
#define LCD_WIDTH                 480U
#define LCD_HEIGHT                272U
#define LCD_REFRESH_RATE          60U
#define LCD_BYTES_PER_PIXEL       2U  /* RGB565 */
#define LCD_FRAME_SIZE            (LCD_WIDTH * LCD_HEIGHT * LCD_BYTES_PER_PIXEL)

/* SDRAM layout: the LCD frame buffer, then the buffer pixel data is received
   into. The host splits updates into transfers of at most a full frame. */
#define LCD_FRAME_BUFFER          LCD_FB_START_ADDRESS
#define GUD_RX_BUFFER             (LCD_FRAME_BUFFER + 0x00040000U)
#define GUD_RX_BUFFER_SIZE        LCD_FRAME_SIZE

#ifdef USE_USB_HS
#define USB_CORE_STRING           "USB HS, connector CN12"
#else
#define USB_CORE_STRING           "USB FS, connector CN13"
#endif

/* Exported macro ------------------------------------------------------------*/
/* Exported functions ------------------------------------------------------- */
void Error_Handler(void);

#endif /* __MAIN_H */
