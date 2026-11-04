#pragma once

#include "hardware/pio.h"

class I2SMasterInput
{
public:
    const PIO pio;
    const uint8_t sm;
    const uint8_t offset;
    const uint8_t frame_size;

private:
    uint32_t freq_hz;

    I2SMasterInput(const I2SMasterInput &) = delete;

public:
    I2SMasterInput(PIO pio, uint8_t sm, uint8_t bck_pin_base, uint8_t din_pin, uint32_t freq_hz, uint8_t frame_size);

    void enable();
    void disable();

    void setFrequency(uint32_t freq_hz);
    uint32_t getFrequency() const;

    ~I2SMasterInput();
};
