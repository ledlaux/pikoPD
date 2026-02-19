/**
 * This file implements stdio for TinyUSB-enabled
 * projects that implement a USB Device interface,
 * have CFG_TUD_ENABLED enabled, that have CFG_TUD_CDC
 * enabled, that have implemented a CDC interface in
 * the device descriptor, and that regularly poll
 * tud_task(). 
 *
 * rppicomidi wrote this library by modifying the
 * pico-sdk file
 * pico-sdk/src/rp2_common/pico_stdio_usb/stdio_usb.c
 *
 * That file contains the following copyright notice
 * that should govern the use of this file
 *
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "tusb.h"
#include "cdc_stdio_lib.h"

// these may not be set if the user is providing tud support (i.e. LIB_TINYUSB_DEVICE is 1 because
// the user linked in tinyusb_device) but they haven't selected CDC
#if (CFG_TUD_ENABLED | TUSB_OPT_DEVICE_ENABLED) && CFG_TUD_CDC

#include "pico/binary_info.h"
#include "pico/time.h"
#include "pico/stdio/driver.h"
#include "pico/mutex.h"
#include "hardware/irq.h"
#include "device/usbd_pvt.h" // for usbd_defer_func

#ifndef PICO_STDIO_USB_STDOUT_TIMEOUT_US
#define PICO_STDIO_USB_STDOUT_TIMEOUT_US 500000
#endif
#ifndef PICO_STDIO_USB_DEFAULT_CRLF
#define PICO_STDIO_USB_DEFAULT_CRLF PICO_STDIO_DEFAULT_CRLF
#endif
// Making sure the binary contains the original copyright notice
static const char* original_copyright_notice="Copyright (c) 2020 Raspberry Pi (Trading) Ltd.  SPDX-License-Identifier: BSD-3-Clause";
static mutex_t stdio_usb_mutex;

#if PICO_STDIO_USB_SUPPORT_CHARS_AVAILABLE_CALLBACK
static void (*chars_available_callback)(void*);
static void *chars_available_param;
#endif

static void stdio_usb_out_chars(const char *buf, int length) {
    (void)original_copyright_notice;
    static uint64_t last_avail_time;
    if (!mutex_try_enter_block_until(&stdio_usb_mutex, make_timeout_time_ms(PICO_STDIO_DEADLOCK_TIMEOUT_MS))) {
        return;
    }
    if (tud_cdc_connected()) {
        for (int i = 0; i < length;) {
            int n = length - i;
            int avail = (int) tud_cdc_write_available();
            if (n > avail) n = avail;
            if (n) {
                int n2 = (int) tud_cdc_write(buf + i, (uint32_t)n);
                tud_task();
                tud_cdc_write_flush();
                i += n2;
                last_avail_time = time_us_64();
            } else {
                tud_task();
                tud_cdc_write_flush();
                if (!tud_cdc_connected() ||
                    (!tud_cdc_write_available() && time_us_64() > last_avail_time + PICO_STDIO_USB_STDOUT_TIMEOUT_US)) {
                    break;
                }
            }
        }
    } else {
        // reset our timeout
        last_avail_time = 0;
    }
    mutex_exit(&stdio_usb_mutex);
}

static int stdio_usb_in_chars(char *buf, int length) {
    // note we perform this check outside the lock, to try and prevent possible deadlock conditions
    // with printf in IRQs (which we will escape through timeouts elsewhere, but that would be less graceful).
    //
    // these are just checks of state, so we can call them while not holding the lock.
    // they may be wrong, but only if we are in the middle of a tud_task call, in which case at worst
    // we will mistakenly think we have data available when we do not (we will check again), or
    // tud_task will complete running and we will check the right values the next time.
    //
    int rc = PICO_ERROR_NO_DATA;
    if (tud_cdc_connected() && tud_cdc_available()) {
        if (!mutex_try_enter_block_until(&stdio_usb_mutex, make_timeout_time_ms(PICO_STDIO_DEADLOCK_TIMEOUT_MS))) {
            return PICO_ERROR_NO_DATA; // would deadlock otherwise
        }
        if (tud_cdc_connected() && tud_cdc_available()) {
            int count = (int) tud_cdc_read(buf, (uint32_t) length);
            rc = count ? count : PICO_ERROR_NO_DATA;
        } else {
            // because our mutex use may starve out the background task, run tud_task here (we own the mutex)
            tud_task();
        }
        mutex_exit(&stdio_usb_mutex);
    }
    return rc;
}

#if PICO_STDIO_USB_SUPPORT_CHARS_AVAILABLE_CALLBACK
void tud_cdc_rx_cb(__unused uint8_t itf) {
    if (chars_available_callback) {
        usbd_defer_func(chars_available_callback, chars_available_param, false);
    }
}

static void stdio_usb_set_chars_available_callback(void (*fn)(void*), void *param) {
    chars_available_callback = fn;
    chars_available_param = param;
}
#endif

static stdio_driver_t stdio_usb = {
    .out_chars = stdio_usb_out_chars,
    .in_chars = stdio_usb_in_chars,
#if PICO_STDIO_USB_SUPPORT_CHARS_AVAILABLE_CALLBACK
    .set_chars_available_callback = stdio_usb_set_chars_available_callback,
#endif
#if PICO_STDIO_ENABLE_CRLF_SUPPORT
    .crlf_enabled = PICO_STDIO_USB_DEFAULT_CRLF
#endif

};

bool cdc_stdio_lib_init(void) {

    assert(tud_inited()); // we expect the caller to have initialized if they are using TinyUSB

    mutex_init(&stdio_usb_mutex);
    bool rc = true;
    if (rc) {
        stdio_set_driver_enabled(&stdio_usb, true);
#if PICO_STDIO_USB_CONNECT_WAIT_TIMEOUT_MS
#if PICO_STDIO_USB_CONNECT_WAIT_TIMEOUT_MS > 0
        absolute_time_t until = make_timeout_time_ms(PICO_STDIO_USB_CONNECT_WAIT_TIMEOUT_MS);
#else
        absolute_time_t until = at_the_end_of_time;
#endif
        do {
            if (tud_cdc_connected()) {
#if PICO_STDIO_USB_POST_CONNECT_WAIT_DELAY_MS != 0
                sleep_ms(PICO_STDIO_USB_POST_CONNECT_WAIT_DELAY_MS);
#endif
                break;
            }
            sleep_ms(10);
        } while (!time_reached(until));
#endif
    }
    return rc;
}
#else
#error "This library requires (CFG_TUD_ENABLED | TUSB_OPT_DEVICE_ENABLED) && CFG_TUD_CDC"
#endif


