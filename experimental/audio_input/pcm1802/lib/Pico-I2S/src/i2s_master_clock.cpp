#include <stdio.h>
#include "pico/stdlib.h"
#include "i2s.pio.h"
#include "i2s_common.h"
#include "i2s_master_clock.h"

I2SMasterClock::I2SMasterClock(PIO pio, uint8_t sm, uint8_t out_pin, uint32_t freq_hz, uint16_t multiplier)
    : pio(pio), sm(sm), offset(pio_add_program(pio, &i2s_master_clock_program)), freq_hz(freq_hz), multiplier(multiplier)
{
    pio_sm_config config = i2s_master_clock_program_get_default_config(offset);
    sm_config_set_sideset_pins(&config, out_pin);

    float divider = get_clkdiv(freq_hz * multiplier);
    sm_config_set_clkdiv(&config, divider);

#ifndef NDEBUG
    printf("sck frequency = %u Hz, divider = %f\n", freq_hz * multiplier, divider);
#endif

    pio_gpio_init(pio, out_pin);
    pio_sm_set_consecutive_pindirs(pio, sm, out_pin, 1, true);
    pio_sm_init(pio, sm, offset, &config);
}

void I2SMasterClock::enable()
{
    pio_sm_set_enabled(pio, sm, true);
}

void I2SMasterClock::disable()
{
    pio_sm_set_enabled(pio, sm, false);
}

void I2SMasterClock::setFrequency(uint32_t freq_hz)
{
    this->freq_hz = freq_hz;

    float divider = get_clkdiv(freq_hz * multiplier);
    pio_sm_set_clkdiv(pio, sm, divider);

#ifndef NDEBUG
    printf("sck frequency = %u Hz, divider = %f\n", freq_hz * multiplier, divider);
#endif
}

uint32_t I2SMasterClock::getFrequency() const
{
    return this->freq_hz;
}

I2SMasterClock::~I2SMasterClock()
{
    disable();
    pio_remove_program(pio, &i2s_master_clock_program, offset);
    pio_sm_unclaim(pio, sm);
}
