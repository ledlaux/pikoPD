 /**
  * @brief Provide support for stdin/stdout over USB serial (CDC)
  * 
  * To use this library, in tusb_config.h implement
  * a USB Device interface, enable CFG_TUD_ENABLED and CFG_TUD_CDC
  * enabled. Add a CDC interface in the application's device descriptor.
  * The application must tud_init() or tusb_init(), then call
  * the function cdc_stdio_lib_init() below.
  * Make sure the application regularly calls tud_task().
  *
  * rppicomidi wrote this library by modifying the
  * pico-sdk file
  * pico-sdk/src/rp2_common/pico_stdio_usb/include/pico/stdio_usb.h
  *
  * That file contains the following copyright notice
  * that should govern the use of this file
  *
  * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
  *
  * SPDX-License-Identifier: BSD-3-Clause
  */
 
#pragma once
#include "pico/stdio.h"
#ifdef __cplusplus
extern "C" {
#endif
/**
 * @brief add the existing CDC interface to the stdio input and output
 *
 */
bool cdc_stdio_lib_init(void);
#ifdef __cplusplus
}
#endif
