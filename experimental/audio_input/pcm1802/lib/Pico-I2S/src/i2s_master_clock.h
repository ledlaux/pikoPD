#pragma once

#include "hardware/pio.h"

class I2SMasterClock
{
public:
    const PIO pio;
    const uint8_t sm;
    const uint8_t offset;
    const uint16_t multiplier;

private:
    uint32_t freq_hz;

    I2SMasterClock(const I2SMasterClock &) = delete;

public:
    I2SMasterClock(PIO pio, uint8_t sm, uint8_t out_pin, uint32_t freq_hz, uint16_t multiplier);

    void enable();
    void disable();

    void setFrequency(uint32_t freq_hz);
    uint32_t getFrequency() const;

    ~I2SMasterClock();
};
