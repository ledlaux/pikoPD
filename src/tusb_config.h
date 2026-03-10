#ifndef _TUSB_CONFIG_H_
#define _TUSB_CONFIG_H_

#ifdef __cplusplus
 extern "C" {
#endif

#ifndef CFG_TUSB_MCU
  #define CFG_TUSB_MCU OPT_MCU_RP2040
#endif

#ifndef CFG_TUSB_OS
  #define CFG_TUSB_OS           OPT_OS_NONE
#endif

#ifdef MIDI_HOST
  #ifndef CFG_TUD_ENABLED
    #define CFG_TUD_ENABLED     0
  #endif
  #ifndef CFG_TUH_ENABLED
    #define CFG_TUH_ENABLED     1
  #endif
  #ifndef CFG_TUSB_RHPORT0_MODE
    #define CFG_TUSB_RHPORT0_MODE OPT_MODE_HOST
  #endif
  
  #define CFG_TUH_DEVICE_MAX  1
  #define CFG_TUH_MIDI        1
  #define CFG_TUH_ENUMERATION_BUFSIZE 256
#else
  #ifndef CFG_TUD_ENABLED
    #define CFG_TUD_ENABLED     1
  #endif
  #ifndef CFG_TUH_ENABLED
    #define CFG_TUH_ENABLED     0
  #endif
  #ifndef CFG_TUSB_RHPORT0_MODE
    #define CFG_TUSB_RHPORT0_MODE OPT_MODE_DEVICE
  #endif

  #ifndef CFG_TUD_ENDPOINT0_SIZE
    #define CFG_TUD_ENDPOINT0_SIZE    64
  #endif

  #define CFG_TUD_CDC               1
  #define CFG_TUD_CDC_RX_BUFSIZE    64
  #define CFG_TUD_CDC_TX_BUFSIZE    64
  #define CFG_TUD_CDC_EP_BUFSIZE    256
  
  #define CFG_TUD_MSC               0
  #define CFG_TUD_HID               0
  #define CFG_TUD_MIDI              1
  #define CFG_TUD_VENDOR            0

  #define CFG_TUD_MIDI_RX_BUFSIZE   (TUD_OPT_HIGH_SPEED ? 512 : 64)
  #define CFG_TUD_MIDI_TX_BUFSIZE   (TUD_OPT_HIGH_SPEED ? 512 : 64)
#endif

#ifndef CFG_TUSB_MEM_SECTION
  #define CFG_TUSB_MEM_SECTION
#endif

#ifndef CFG_TUSB_MEM_ALIGN
  #define CFG_TUSB_MEM_ALIGN        __attribute__ ((aligned(4)))
#endif

#ifdef __cplusplus
 }
#endif

#endif