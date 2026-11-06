#if !defined(MIDI_HOST)

#include "tusb.h"
#include "pico/bootrom.h"

extern "C" void tud_cdc_line_coding_cb(
    uint8_t instance,
    cdc_line_coding_t const *coding)
{
    (void)instance;

    if (coding && coding->bit_rate == 1200) {
        reset_usb_boot(0, 0);
    }
}

#endif // !MIDI_HOST