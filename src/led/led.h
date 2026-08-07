#pragma once

#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/pwm.h"
#include "hardware/pio.h"
#include <atomic>

#ifdef PICO_ZERO
#include "ws2812.pio.h" 
#endif

namespace Pico {

    struct Led {
        uint32_t pin;
        uint slice;
        uint chan;
        bool is_rgb; 
        uint8_t r, g, b;
    };

    inline Led leds[12];
    inline int n_led = 0;

    inline std::atomic<float> led_vals[12];
    inline std::atomic<float> led_hue[12];        
    inline std::atomic<float> led_intensity[12];
    inline uint32_t led_framebuffer[12] = {0};
    inline float smooth_hue[12] = {0.0f};

    inline void addLed(int index, uint32_t pin) {
        gpio_set_function(pin, GPIO_FUNC_PWM);
        uint slice = pwm_gpio_to_slice_num(pin);
        uint chan = pwm_gpio_to_channel(pin);
        pwm_set_wrap(slice, 255);
        pwm_set_enabled(slice, true);
        
        leds[index].pin = pin;
        leds[index].slice = slice;
        leds[index].chan = chan;
        leds[index].is_rgb = false;
        if (index >= n_led) n_led = index + 1;
    }

    inline void __not_in_flash_func(setLedHardware)(int index, float value) {
        if (index >= 12 || leds[index].is_rgb) return;
        uint16_t level = (uint16_t)(value * value * 255.0f);
        pwm_set_chan_level(leds[index].slice, leds[index].chan, level);
    }

    inline void updateLed(int index, float val) {
        if (index < 12) {
            led_vals[index].store(val, std::memory_order_relaxed);
            if (!leds[index].is_rgb) setLedHardware(index, val);
        }
    }

#ifdef PICO_ZERO
    inline void init_neopixel() {
        static bool initialized = false;
        if (initialized) return;
        PIO pio = pio1;
        if (!pio_can_add_program(pio, &ws2812_program)) return;
        uint offset = pio_add_program(pio, &ws2812_program);
        ws2812_program_init(pio, 0, offset, 16, 800000, false); 
        pio_sm_set_enabled(pio, 0, true);
        initialized = true;
    }

    inline void addRgbLed(int index, uint32_t pin, uint8_t r = 255, uint8_t g = 255, uint8_t b = 255) {
        init_neopixel(); 
        leds[index].pin = pin;
        leds[index].is_rgb = true;
        leds[index].r = r; leds[index].g = g; leds[index].b = b;
        if (index >= n_led) n_led = index + 1;
    }

    inline void updateRGB(int index, float hue, float intensity) {
        if (index < 0 || index >= 12) return;
        float diff = hue - smooth_hue[index];
        if (diff > 0.5f) diff -= 1.0f;
        if (diff < -0.5f) diff += 1.0f;
        smooth_hue[index] += diff * 0.15f; 
        if (smooth_hue[index] >= 1.0f) smooth_hue[index] -= 1.0f;
        if (smooth_hue[index] < 0.0f) smooth_hue[index] += 1.0f;

        float r = 0, g = 0, b = 0;
        float h = smooth_hue[index] * 6.0f;
        int i = (int)h;
        float f = h - i;
        float q = 1.0f - f;
        switch (i % 6) {
            case 0: r = 1.0f; g = f;    b = 0.0f; break;
            case 1: r = q;    g = 1.0f; b = 0.0f; break;
            case 2: r = 0.0f; g = 1.0f; b = f;    break;
            case 3: r = 0.0f; g = q;    b = 1.0f; break;
            case 4: r = f;    g = 0.0f; b = 1.0f; break;
            case 5: r = 1.0f; g = 0.0f; b = q;    break;
        }
        float gamma = intensity * intensity;
        uint8_t uR = (uint8_t)(r * gamma * 255.0f);
        uint8_t uG = (uint8_t)(g * gamma * 255.0f);
        uint8_t uB = (uint8_t)(b * gamma * 255.0f);
        led_framebuffer[index] = ((uint32_t)(uG) << 16) | ((uint32_t)(uR) << 8) | ((uint32_t)(uB));
    }

    inline void showRGB() {
        if (pio_sm_get_tx_fifo_level(pio1, 0) > 4) return;
        pio1->txf[0] = led_framebuffer[0];
        pio1->txf[0] = 0; pio1->txf[0] = 0; pio1->txf[0] = 0;
    }
#endif

} // namespace Pico