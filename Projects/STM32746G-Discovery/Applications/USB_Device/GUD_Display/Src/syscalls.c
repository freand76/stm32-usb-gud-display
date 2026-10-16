/**
  ******************************************************************************
  * @file    USB_Device/GUD_Display/Src/syscalls.c
  * @author  Fredrik Andersson
  * @brief   Minimal system calls for newlib: printf goes to the ST-LINK
  *          virtual COM port, there are no files.
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
#include <errno.h>
#include <sys/stat.h>
#include "main.h"

/* Private variables ---------------------------------------------------------*/
extern UART_HandleTypeDef UartHandle;

/* Private functions ---------------------------------------------------------*/

/**
  * @brief  Writes to the virtual COM port, newlines become CR LF
  * @param  file: file descriptor
  * @param  ptr: data
  * @param  len: data length
  * @retval number of bytes written
  */
int _write(int file, char *ptr, int len)
{
  UNUSED(file);

  for (int idx = 0; idx < len; idx++)
  {
    if (ptr[idx] == '\n')
    {
      HAL_UART_Transmit(&UartHandle, (uint8_t *)"\r", 1, 0xFFFF);
    }
    HAL_UART_Transmit(&UartHandle, (uint8_t *)&ptr[idx], 1, 0xFFFF);
  }

  return len;
}

int _read(int file, char *ptr, int len)
{
  UNUSED(file);
  UNUSED(ptr);
  UNUSED(len);
  errno = ENOSYS;
  return -1;
}

int _close(int file)
{
  UNUSED(file);
  return -1;
}

int _fstat(int file, struct stat *st)
{
  UNUSED(file);
  st->st_mode = S_IFCHR;
  return 0;
}

int _isatty(int file)
{
  UNUSED(file);
  return 1;
}

int _lseek(int file, int ptr, int dir)
{
  UNUSED(file);
  UNUSED(ptr);
  UNUSED(dir);
  return 0;
}
