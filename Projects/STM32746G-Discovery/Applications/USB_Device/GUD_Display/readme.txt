/**
  @page GUD_Display USB Device GUD (Generic USB Display) application

  @verbatim
  ******************************************************************************
  * @file    USB_Device/GUD_Display/readme.txt
  * @author  Fredrik Andersson
  * @brief   Description of the USB GUD display application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 Fredrik Andersson.
  * All rights reserved.
  *
  * SPDX-License-Identifier: MIT
  *
  ******************************************************************************
  @endverbatim

@par Application Description

Use of the USB device application based on the GUD (Generic USB Display) protocol.

This is a typical application on how to use the STM32F746xx USB OTG Device peripheral where the STM32 MCU is
enumerated as a GUD display, driven by the native Linux gud DRM driver (Linux 5.13 or later), to which the
STM32746G-Discovery board is connected. The 480x272 LCD of the board becomes a monitor of the host: it shows
up as a DRM device with a framebuffer (/dev/fbN), and a desktop can extend onto it.

The device uses the vendor and product ids 0x1D50/0x614D, the only ids the Linux gud driver binds to. It
offers one display mode, 480x272, and the RGB565 pixel format, which is also the format of the LTDC layer.

The GUD class (Middlewares/ST/STM32_USB_Device_Library/Class/GUD) answers the GUD vendor requests on the
control endpoint and receives the pixel data on a bulk OUT endpoint. Before each bulk transfer the host
sends GUD_REQ_SET_BUFFER with the rectangle the transfer updates. The pixel data is received into SDRAM and
copied into the LCD frame buffer by DMA2D (usbd_gud_if.c).

At the beginning of the main program the HAL_Init() function is called to reset all the peripherals,
initialize the Flash interface and the systick. The user is provided with the SystemClock_Config()
function to configure the system clock (SYSCLK) to run at 216 MHz. The Full Speed (FS) USB module uses
internally a 48-MHz clock which is coming from the main PLL (USBCLK = PLLVCO/PLLQ).
In the High Speed (HS) mode the USB clock (60 MHz) is driven by the ULPI.

The SDRAM holds the LCD frame buffer (0xC0000000) and the receive buffer (0xC0040000). It is configured by
the MPU as Write Through, so the pixel data the CPU reads out of the USB FIFO is in the memory when DMA2D
and LTDC read it.

Until the host switches the display on, and whenever it switches it off, a splash screen shows the
selected USB core. LED1 toggles for each received rectangle. A log is printed on the ST-LINK virtual COM
port (115200 8N1):
  USB GUD Display, USB HS, connector CN12
  USB: started, waiting for the host
  USB: configured
  Display: controller enabled
  Display: 480x272, format 0x40
  Display: output enabled
  Display: 16 updates, 4177920 bytes, last 480x272 at 0,0, 0 errors

The connector is reported disconnected for the first 5 seconds after the host configures the device and
connected after that. The host polls the connector status every 10 seconds and sees the change: a desktop
like GNOME only picks up the connector of a hotplugged display when its status changes.

@note Control OUT requests can not be rejected once their data has been received, so the class announces
      GUD_DISPLAY_FLAG_STATUS_ON_SET: the host reads the result of each one with GUD_REQ_GET_STATUS.

@note The DMA2D copy runs in the USB interrupt. The main loop disables the USB interrupt while it draws
      the splash screen, which also uses DMA2D. The SysTick interrupt has a higher priority than the
      USB interrupt, so the HAL timeouts work in the USB interrupt.

For more details about the STM32Cube USB Device library, please refer to UM1734
"STM32Cube USB Device library".


@par USB Library Configuration

To select the appropriate USB Core to work with, user must add the following macro defines within the
compiler preprocessor (done by the Makefile, "make USB=HS" or "make USB=FS"):
      - "USE_USB_HS" when using USB High Speed (HS) Core
      - "USE_USB_FS" when using USB Full Speed (FS) Core


@par Keywords

Connectivity, USB_Device, USB, GUD, Display, LCD, LTDC, DMA2D, SDRAM, Full Speed, High Speed

@par Directory contents

  - USB_Device/GUD_Display/Src/main.c                  Main program
  - USB_Device/GUD_Display/Src/system_stm32f7xx.c      STM32F7xx system clock configuration file
  - USB_Device/GUD_Display/Src/stm32f7xx_it.c          Interrupt handlers
  - USB_Device/GUD_Display/Src/syscalls.c              printf to the virtual COM port
  - USB_Device/GUD_Display/Src/usbd_conf.c             General low level driver configuration
  - USB_Device/GUD_Display/Src/usbd_desc.c             USB device GUD descriptor
  - USB_Device/GUD_Display/Src/usbd_gud_if.c           USB device GUD interface, the LCD
  - USB_Device/GUD_Display/Inc/main.h                  Main program header file
  - USB_Device/GUD_Display/Inc/stm32f7xx_it.h          Interrupt handlers header file
  - USB_Device/GUD_Display/Inc/stm32f7xx_hal_conf.h    HAL configuration file
  - USB_Device/GUD_Display/Inc/usbd_conf.h             USB device driver Configuration file
  - USB_Device/GUD_Display/Inc/usbd_desc.h             USB device GUD descriptor header file
  - USB_Device/GUD_Display/Inc/usbd_gud_if.h           USB device GUD interface header file
  - USB_Device/GUD_Display/GCC/STM32F746NGHx_FLASH.ld  Linker script


@par Hardware and Software environment

  - This application runs on STM32F746xx devices.

  - This application has been tested with STMicroelectronics STM32746G-Discovery
    boards and can be easily tailored to any other supported device and development
    board.

  - STM32746G-Discovery Set-up
    - Connect the ST-LINK connector CN14 to the PC, for programming and the log.
    - Connect the STM32746G-Discovery board to the PC through 'USB micro A-Male
      to A-Male' cable to the connector:
      - CN12 : to use USB High Speed (HS)
      - CN13: to use USB Full Speed (FS)

@par How to use it ?

In order to make the program work, you must do the following :
 - make toolchain          (once, downloads the Arm GNU Toolchain and OpenOCD)
 - make flash USB=HS       or "make flash USB=FS"
 - Run the application

 */
