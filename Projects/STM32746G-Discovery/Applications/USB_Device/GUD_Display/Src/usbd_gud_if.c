/**
  ******************************************************************************
  * @file    USB_Device/GUD_Display/Src/usbd_gud_if.c
  * @author  Fredrik Andersson
  * @brief   USB Device GUD interface: the display of the STM32746G-Discovery.
  *          Received rectangles are copied into the LCD frame buffer by DMA2D.
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

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
#define DMA2D_TIMEOUT_MS          100U

/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
extern USBD_HandleTypeDef USBD_Device;

GUD_DisplayStatusTypeDef GUD_DisplayStatus;

static DMA2D_HandleTypeDef hdma2d_gud;
static uint32_t ConfiguredTick;

/* Pixel formats offered to the host, in order of preference */
static const uint8_t GUD_Formats[] =
{
  GUD_PIXEL_FORMAT_RGB565,
};

static const USBD_GUD_DisplayTypeDef GUD_Display =
{
  LCD_WIDTH,
  LCD_HEIGHT,
  LCD_REFRESH_RATE,
  GUD_Formats,
  (uint8_t)sizeof(GUD_Formats),
};

/* Private function prototypes -----------------------------------------------*/
static int8_t GUD_Itf_Init(void);
static int8_t GUD_Itf_DeInit(void);
static int8_t GUD_Itf_SetState(uint8_t format, USBD_GUD_DisplayModeReqTypeDef *mode);
static int8_t GUD_Itf_Enable(uint8_t controller, uint8_t display);
static int8_t GUD_Itf_GetConnectorStatus(uint8_t *status);
static int8_t GUD_Itf_Flush(USBD_GUD_SetBufferReqTypeDef *rect, uint8_t *pbuf);

USBD_GUD_ItfTypeDef USBD_GUD_fops =
{
  &GUD_Display,
  GUD_Itf_Init,
  GUD_Itf_DeInit,
  GUD_Itf_SetState,
  GUD_Itf_Enable,
  GUD_Itf_GetConnectorStatus,
  GUD_Itf_Flush,
};

/* Private functions ---------------------------------------------------------*/

/**
  * @brief  Initializes the GUD display interface, called when the host
  *         configures the device
  * @param  None
  * @retval Result of the operation: USBD_OK if all operations are OK else USBD_FAIL
  */
static int8_t GUD_Itf_Init(void)
{
  ConfiguredTick = HAL_GetTick();

  GUD_DisplayStatus.ControllerEnabled = 0U;
  GUD_DisplayStatus.DisplayEnabled = 0U;
  GUD_DisplayStatus.Format = 0U;
  GUD_DisplayStatus.Configured = 1U;

  /* Pixel data is received into SDRAM */
  USBD_GUD_SetRxBuffer(&USBD_Device, (uint8_t *)GUD_RX_BUFFER, GUD_RX_BUFFER_SIZE);

  return (USBD_OK);
}

/**
  * @brief  DeInitializes the GUD display interface
  * @param  None
  * @retval Result of the operation: USBD_OK if all operations are OK else USBD_FAIL
  */
static int8_t GUD_Itf_DeInit(void)
{
  GUD_DisplayStatus.ControllerEnabled = 0U;
  GUD_DisplayStatus.DisplayEnabled = 0U;
  GUD_DisplayStatus.Configured = 0U;

  return (USBD_OK);
}

/**
  * @brief  The host has committed a new display state
  * @param  format: GUD pixel format
  * @param  mode: display mode
  * @retval Result of the operation: USBD_OK if all operations are OK else USBD_FAIL
  */
static int8_t GUD_Itf_SetState(uint8_t format, USBD_GUD_DisplayModeReqTypeDef *mode)
{
  UNUSED(mode);

  /* The class only accepts the one mode and the offered formats */
  GUD_DisplayStatus.Format = format;

  return (USBD_OK);
}

/**
  * @brief  The host switches the display controller or output on or off
  * @param  controller: 1 when the controller is enabled
  * @param  display: 1 when the display output is enabled
  * @retval Result of the operation: USBD_OK if all operations are OK else USBD_FAIL
  */
static int8_t GUD_Itf_Enable(uint8_t controller, uint8_t display)
{
  GUD_DisplayStatus.ControllerEnabled = controller;
  GUD_DisplayStatus.DisplayEnabled = display;

  return (USBD_OK);
}

/**
  * @brief  Reports the connector status, polled by the host every 10 seconds
  * @param  status: GUD connector status
  * @retval Result of the operation: USBD_OK if all operations are OK else USBD_FAIL
  */
static int8_t GUD_Itf_GetConnectorStatus(uint8_t *status)
{
  if ((HAL_GetTick() - ConfiguredTick) >= GUD_CONNECTOR_CONNECT_DELAY_MS)
  {
    *status = GUD_CONNECTOR_STATUS_CONNECTED;
  }
  else
  {
    *status = GUD_CONNECTOR_STATUS_DISCONNECTED;
  }

  return (USBD_OK);
}

/**
  * @brief  A rectangle of pixel data has been received, copy it into the frame
  *         buffer. Called from the USB interrupt, which the main loop disables
  *         while it draws on the LCD itself, so DMA2D is never shared.
  * @param  rect: the rectangle
  * @param  pbuf: pixel data of the rectangle, lines without padding
  * @retval Result of the operation: USBD_OK if all operations are OK else USBD_FAIL
  */
static int8_t GUD_Itf_Flush(USBD_GUD_SetBufferReqTypeDef *rect, uint8_t *pbuf)
{
  uint32_t destination = LCD_FRAME_BUFFER +
                         (((rect->y * LCD_WIDTH) + rect->x) * LCD_BYTES_PER_PIXEL);
  int8_t ret = USBD_FAIL;

  /* Memory to memory copy of the rectangle into the frame buffer */
  hdma2d_gud.Instance = DMA2D;
  hdma2d_gud.Init.Mode = DMA2D_M2M;
  hdma2d_gud.Init.ColorMode = DMA2D_OUTPUT_RGB565;
  hdma2d_gud.Init.OutputOffset = LCD_WIDTH - rect->width;

  hdma2d_gud.LayerCfg[1].InputOffset = 0U;
  hdma2d_gud.LayerCfg[1].InputColorMode = DMA2D_INPUT_RGB565;
  hdma2d_gud.LayerCfg[1].AlphaMode = DMA2D_NO_MODIF_ALPHA;
  hdma2d_gud.LayerCfg[1].InputAlpha = 0xFFU;

  if ((HAL_DMA2D_Init(&hdma2d_gud) == HAL_OK) &&
      (HAL_DMA2D_ConfigLayer(&hdma2d_gud, 1U) == HAL_OK) &&
      (HAL_DMA2D_Start(&hdma2d_gud, (uint32_t)pbuf, destination,
                       rect->width, rect->height) == HAL_OK) &&
      (HAL_DMA2D_PollForTransfer(&hdma2d_gud, DMA2D_TIMEOUT_MS) == HAL_OK))
  {
    GUD_DisplayStatus.Updates++;
    GUD_DisplayStatus.Bytes += rect->length;
    GUD_DisplayStatus.LastRect = *rect;
    ret = USBD_OK;
  }
  else
  {
    GUD_DisplayStatus.Errors++;
  }

  BSP_LED_Toggle(LED1);

  return ret;
}
