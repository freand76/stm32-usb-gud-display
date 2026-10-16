/**
  ******************************************************************************
  * @file    USB_Device/GUD_Display/Src/main.c
  * @author  Fredrik Andersson
  * @brief   USB device GUD display main file
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
#ifdef USE_USB_HS
#define USB_IRQn                  OTG_HS_IRQn
#else
#define USB_IRQn                  OTG_FS_IRQn
#endif

#define MAIN_LOOP_INTERVAL_MS     20U
#define STATS_INTERVAL_MS         1000U

/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
USBD_HandleTypeDef USBD_Device;
UART_HandleTypeDef UartHandle;

/* Private function prototypes -----------------------------------------------*/
static void MPU_Config(void);
static void SystemClock_Config(void);
static void CPU_CACHE_Enable(void);
static void LCD_Config(void);
static void LCD_ShowSplash(void);
static void UART_Config(void);

/* Private functions ---------------------------------------------------------*/

/**
  * @brief  Main program
  * @param  None
  * @retval None
  */
int main(void)
{
  GUD_DisplayStatusTypeDef status;
  GUD_DisplayStatusTypeDef shown = {0};
  uint8_t display_on;
  uint8_t shown_on = 0U;
  uint32_t next_stats_tick;

  /* Configure the MPU attributes */
  MPU_Config();

  /* Enable the CPU Cache */
  CPU_CACHE_Enable();

  /* STM32F7xx HAL library initialization:
       - Configure the Flash ART accelerator on ITCM interface
       - Configure the Systick to generate an interrupt each 1 msec
       - Set NVIC Group Priority to 4
       - Low Level Initialization
     */
  HAL_Init();

  /* Configure the System clock to have a frequency of 216 MHz */
  SystemClock_Config();

  /* Configure LED1, toggled for each received rectangle */
  BSP_LED_Init(LED1);

  /* Configure the ST-LINK virtual COM port for the log */
  UART_Config();
  printf("\n\nUSB GUD Display, %s\n", USB_CORE_STRING);

  /* Configure the LCD, its frame buffer is drawn on by the host */
  LCD_Config();
  LCD_ShowSplash();

  /* Init Device Library */
  USBD_Init(&USBD_Device, &GUD_Desc, 0);

  /* Add Supported Class */
  USBD_RegisterClass(&USBD_Device, USBD_GUD_CLASS);

  /* Add GUD Interface Class */
  USBD_GUD_RegisterInterface(&USBD_Device, &USBD_GUD_fops);

  /* Start Device Process */
  USBD_Start(&USBD_Device);
  printf("USB: started, waiting for the host\n");

  next_stats_tick = HAL_GetTick() + STATS_INTERVAL_MS;

  /* Run Application (Interrupt mode) */
  while (1)
  {
    uint8_t print_stats = ((int32_t)(HAL_GetTick() - next_stats_tick) >= 0) ? 1U : 0U;

    /* The status is written from the USB interrupt */
    HAL_NVIC_DisableIRQ(USB_IRQn);
    status = GUD_DisplayStatus;
    if (print_stats != 0U)
    {
      GUD_DisplayStatus.Updates = 0U;
      GUD_DisplayStatus.Bytes = 0U;
      GUD_DisplayStatus.Errors = 0U;
    }
    display_on = (status.ControllerEnabled != 0U) && (status.DisplayEnabled != 0U);
    if ((display_on == 0U) && (shown_on != 0U))
    {
      /* Back to the splash screen when the host stops using the display. It
         is drawn with the USB interrupt disabled, the interrupt uses DMA2D */
      LCD_ShowSplash();
    }
    HAL_NVIC_EnableIRQ(USB_IRQn);

    if (status.Configured != shown.Configured)
    {
      printf("USB: %s\n", (status.Configured != 0U) ? "configured" : "deconfigured");
    }
    if (status.ControllerEnabled != shown.ControllerEnabled)
    {
      printf("Display: controller %s\n", (status.ControllerEnabled != 0U) ? "enabled" : "disabled");
    }
    if ((status.Format != shown.Format) && (status.Format != 0U))
    {
      printf("Display: %lux%lu, format 0x%02x\n", (unsigned long)LCD_WIDTH,
             (unsigned long)LCD_HEIGHT, status.Format);
    }
    if (status.DisplayEnabled != shown.DisplayEnabled)
    {
      printf("Display: output %s\n", (status.DisplayEnabled != 0U) ? "enabled" : "disabled");
    }
    shown = status;
    shown_on = display_on;

    if (print_stats != 0U)
    {
      next_stats_tick += STATS_INTERVAL_MS;
      if ((status.Updates > 0U) || (status.Errors > 0U))
      {
        printf("Display: %lu updates, %lu bytes, last %lux%lu at %lu,%lu, %lu errors\n",
               (unsigned long)status.Updates, (unsigned long)status.Bytes,
               (unsigned long)status.LastRect.width, (unsigned long)status.LastRect.height,
               (unsigned long)status.LastRect.x, (unsigned long)status.LastRect.y,
               (unsigned long)status.Errors);
      }
    }

    HAL_Delay(MAIN_LOOP_INTERVAL_MS);
  }
}

/**
  * @brief  LCD configuration: one RGB565 layer in SDRAM
  * @param  None
  * @retval None
  */
static void LCD_Config(void)
{
  /* LCD initialization, it also initializes the SDRAM */
  BSP_LCD_Init();

  /* LCD layer initialization */
  BSP_LCD_LayerRgb565Init(0, LCD_FRAME_BUFFER);
  BSP_LCD_SelectLayer(0);

  /* Enable the LCD */
  BSP_LCD_DisplayOn();
}

/**
  * @brief  Shows what the display is, until the host draws on it
  * @param  None
  * @retval None
  */
static void LCD_ShowSplash(void)
{
  BSP_LCD_Clear(LCD_COLOR_WHITE);

  /* Title bar */
  BSP_LCD_SetTextColor(LCD_COLOR_DARKBLUE);
  BSP_LCD_FillRect(0, 0, LCD_WIDTH, 80);
  BSP_LCD_SetTextColor(LCD_COLOR_WHITE);
  BSP_LCD_SetBackColor(LCD_COLOR_DARKBLUE);
  BSP_LCD_SetFont(&Font24);
  BSP_LCD_DisplayStringAt(0, 16, (uint8_t *)"USB GUD Display", CENTER_MODE);
  BSP_LCD_SetFont(&Font16);
  BSP_LCD_DisplayStringAt(0, 50, (uint8_t *)USB_CORE_STRING, CENTER_MODE);

  BSP_LCD_SetTextColor(LCD_COLOR_DARKBLUE);
  BSP_LCD_SetBackColor(LCD_COLOR_WHITE);
  BSP_LCD_DisplayStringAt(0, 130, (uint8_t *)"Waiting for the host", CENTER_MODE);
  BSP_LCD_SetFont(&Font12);
  BSP_LCD_DisplayStringAt(0, 160, (uint8_t *)"480x272 RGB565, Linux gud driver", CENTER_MODE);
}

/**
  * @brief  UART configuration: the ST-LINK virtual COM port, 115200 8N1
  * @param  None
  * @retval None
  */
static void UART_Config(void)
{
  UartHandle.Init.BaudRate = 115200;
  UartHandle.Init.WordLength = UART_WORDLENGTH_8B;
  UartHandle.Init.StopBits = UART_STOPBITS_1;
  UartHandle.Init.Parity = UART_PARITY_NONE;
  UartHandle.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  UartHandle.Init.Mode = UART_MODE_TX_RX;
  UartHandle.Init.OverSampling = UART_OVERSAMPLING_16;
  UartHandle.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;

  BSP_COM_Init(COM1, &UartHandle);
}

/**
  * @brief  System Clock Configuration
  *         The system Clock is configured as follow :
  *            System Clock source            = PLL (HSE)
  *            SYSCLK(Hz)                     = 216000000
  *            HCLK(Hz)                       = 216000000
  *            AHB Prescaler                  = 1
  *            APB1 Prescaler                 = 4
  *            APB2 Prescaler                 = 2
  *            HSE Frequency(Hz)              = 25000000
  *            PLL_M                          = 25
  *            PLL_N                          = 432
  *            PLL_P                          = 2
  *            PLL_Q                          = 9 (48 MHz for USB FS)
  *            VDD(V)                         = 3.3
  *            Main regulator output voltage  = Scale1 mode
  *            Flash Latency(WS)              = 7
  * @param  None
  * @retval None
  */
static void SystemClock_Config(void)
{
  RCC_ClkInitTypeDef RCC_ClkInitStruct;
  RCC_OscInitTypeDef RCC_OscInitStruct;

  /* Enable HSE Oscillator and activate PLL with HSE as source */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSIState = RCC_HSI_OFF;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 25;
  RCC_OscInitStruct.PLL.PLLN = 432;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /* Activate the OverDrive to reach the 216 Mhz Frequency */
  if (HAL_PWREx_EnableOverDrive() != HAL_OK)
  {
    Error_Handler();
  }

  /* Select PLL as system clock source and configure the HCLK, PCLK1 and PCLK2
     clocks dividers */
  RCC_ClkInitStruct.ClockType = (RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK |
                                 RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2);
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_7) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @param  None
  * @retval None
  */
void Error_Handler(void)
{
  /* User may add here some code to deal with this error */
  while (1)
  {
  }
}

/**
  * @brief  CPU L1-Cache enable.
  * @param  None
  * @retval None
  */
static void CPU_CACHE_Enable(void)
{
  /* Enable I-Cache */
  SCB_EnableICache();

  /* Enable D-Cache */
  SCB_EnableDCache();
}

/**
  * @brief  Configure the MPU attributes
  * @param  None
  * @retval None
  */
static void MPU_Config(void)
{
  MPU_Region_InitTypeDef MPU_InitStruct;

  /* Disable the MPU */
  HAL_MPU_Disable();

  /* Configure the MPU as Strongly ordered for not defined regions. The
     subregions of the internal memories, the peripherals, the FMC registers
     (0xA0000000) and the Cortex-M7 registers keep the default map */
  MPU_InitStruct.Enable = MPU_REGION_ENABLE;
  MPU_InitStruct.BaseAddress = 0x00;
  MPU_InitStruct.Size = MPU_REGION_SIZE_4GB;
  MPU_InitStruct.AccessPermission = MPU_REGION_NO_ACCESS;
  MPU_InitStruct.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;
  MPU_InitStruct.IsCacheable = MPU_ACCESS_NOT_CACHEABLE;
  MPU_InitStruct.IsShareable = MPU_ACCESS_SHAREABLE;
  MPU_InitStruct.Number = MPU_REGION_NUMBER0;
  MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL0;
  MPU_InitStruct.SubRegionDisable = 0xA7;
  MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE;

  HAL_MPU_ConfigRegion(&MPU_InitStruct);

  /* Configure the SDRAM as Write Through: the CPU writes received pixel data
     there, which DMA2D and LTDC read directly from the memory */
  MPU_InitStruct.Enable = MPU_REGION_ENABLE;
  MPU_InitStruct.BaseAddress = SDRAM_DEVICE_ADDR;
  MPU_InitStruct.Size = MPU_REGION_SIZE_8MB;
  MPU_InitStruct.AccessPermission = MPU_REGION_FULL_ACCESS;
  MPU_InitStruct.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;
  MPU_InitStruct.IsCacheable = MPU_ACCESS_CACHEABLE;
  MPU_InitStruct.IsShareable = MPU_ACCESS_NOT_SHAREABLE;
  MPU_InitStruct.Number = MPU_REGION_NUMBER1;
  MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL0;
  MPU_InitStruct.SubRegionDisable = 0x00;
  MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE;

  HAL_MPU_ConfigRegion(&MPU_InitStruct);

  /* Enable the MPU */
  HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t* file, uint32_t line)
{
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */

  /* Infinite loop */
  while (1)
  {
  }
}
#endif
