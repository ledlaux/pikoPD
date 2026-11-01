#pragma once

#include "hardware/pio.h"

class I2SMasterOutput
{
public:
    const PIO pio;
    const uint8_t sm;
    const uint8_t offset;
    const uint8_t frame_size;

private:
    uint32_t freq_hz;

    I2SMasterOutput(const I2SMasterOutput &) = delete;

public:
    I2SMasterOutput(PIO pio, uint8_t sm, uint8_t bck_pin_base, uint8_t dout_pin, uint32_t freq_hz, uint8_t frame_size);

    void enable();
    void disable();

    void setFrequency(uint32_t freq_hz);
    uint32_t getFrequency() const;

    ~I2SMasterOutput();
};
