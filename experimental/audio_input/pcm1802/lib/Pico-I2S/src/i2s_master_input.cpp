#include <stdio.h>
#include "pico/stdlib.h"
#include "i2s.pio.h"
#include "i2s_common.h"
#include "i2s_master_input.h"

I2SMasterInput::I2SMasterInput(PIO pio, uint8_t sm, uint8_t bck_pin_base, uint8_t din_pin, uint32_t freq_hz, uint8_t frame_size)
    : pio(pio), sm(sm), offset(pio_add_program(pio, &i2s_master_input_program)), freq_hz(freq_hz), frame_size(frame_size)
{
    pio_gpio_init(pio, bck_pin_base);
    pio_gpio_init(pio, bck_pin_base + 1);
    pio_gpio_init(pio, din_pin);

    pio_sm_config config = i2s_master_input_program_get_default_config(offset);
    sm_config_set_sideset_pins(&config, bck_pin_base);
    sm_config_set_in_pins(&config, din_pin);
    sm_config_set_in_shift(&config, false, true, 32);
    sm_config_set_fifo_join(&config, PIO_FIFO_JOIN_RX);

    float divider = get_clkdiv(freq_hz * frame_size * 2);
    sm_config_set_clkdiv(&config, divider);

#ifndef NDEBUG
    printf("bck frequency = %u Hz, divider = %f\n", freq_hz * frame_size * 2, divider);
#endif

    pio_sm_init(pio, sm, offset, &config);

    pio_sm_set_consecutive_pindirs(pio, sm, bck_pin_base, 2, true);
    pio_sm_set_consecutive_pindirs(pio, sm, din_pin, 1, false);
    pio_sm_set_pins(pio, sm, 0);
    pio_sm_clear_fifos(pio, sm);

    pio_sm_exec(pio, sm, pio_encode_jmp(offset + i2s_master_input_offset_entry_point));
}

void I2SMasterInput::enable()
{
    pio_sm_set_enabled(pio, sm, true);
}

void I2SMasterInput::disable()
{
    pio_sm_set_enabled(pio, sm, false);
}

void I2SMasterInput::setFrequency(uint32_t freq_hz)
{
    this->freq_hz = freq_hz;

    float divider = get_clkdiv(freq_hz * frame_size * 2);
    pio_sm_set_clkdiv(pio, sm, divider);

#ifndef NDEBUG
    printf("bck frequency = %u Hz, divider = %f\n", freq_hz * frame_size * 2, divider);
#endif
}

uint32_t I2SMasterInput::getFrequency() const
{
    return this->freq_hz;
}

I2SMasterInput::~I2SMasterInput()
{
    disable();
    pio_remove_program(pio, &i2s_master_input_program, offset);
    pio_sm_unclaim(pio, sm);
}
