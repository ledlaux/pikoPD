#pragma once

#include "hardware/clocks.h"
#include "hardware/pio.h"

static inline float get_clkdiv(uint32_t freq_hz)
{
    return clock_get_hz(clk_sys) / (float)(freq_hz * 2);
}

static inline gpio_function_t get_gpio_function(PIO pio)
{
    if (pio == pio0)
    {
        return GPIO_FUNC_PIO0;
    }

    if (pio == pio1)
    {
        return GPIO_FUNC_PIO1;
    }

    // RP2350 has 3 PIO blocks
#ifdef GPIO_FUNC_PIO2
    if (pio == pio2)
    {
        return GPIO_FUNC_PIO2;
    }
#endif

    return GPIO_FUNC_NULL;
}
