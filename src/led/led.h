#pragma once

#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/pwm.h"
#include "hardware/pio.h"
#include <atomic>

// --------------------------------------------------
// Pico W / Pico 2 W CYW43 LED support
// --------------------------------------------------

#if defined(PICO_W)
    #define PICO_CYW43_LED 1
#elif defined(PICO2_W)
    #define PICO_CYW43_LED 1
#endif

#ifdef PICO_CYW43_LED
extern "C" void cyw43_arch_gpio_put(uint wl_gpio, bool value);

#ifndef CYW43_WL_GPIO_LED_PIN
#define CYW43_WL_GPIO_LED_PIN 0
#endif
#endif

#ifdef PICO_ZERO
#include "ws2812.pio.h"
#endif

namespace Pico {

constexpr int MAX_LEDS = 12;

struct Led {
    uint32_t pin;
    uint slice;
    uint chan;
    bool is_rgb;
    bool cyw43;
    uint8_t r, g, b;
};

inline Led leds[MAX_LEDS] = {};
inline int n_led = 0;

inline std::atomic<float> led_vals[MAX_LEDS] = {};
inline std::atomic<float> led_hue[MAX_LEDS] = {};
inline std::atomic<float> led_intensity[MAX_LEDS] = {};

inline uint32_t led_framebuffer[MAX_LEDS] = {0};
inline float smooth_hue[MAX_LEDS] = {0.0f};

// --------------------------------------------------
// Standard / PWM LED
// --------------------------------------------------

inline void addLed(int index, uint32_t pin) {
    if (index < 0 || index >= MAX_LEDS) return;

    leds[index].pin = pin;
    leds[index].is_rgb = false;
    leds[index].cyw43 = false;

#if defined(PICO_W)
    // Raspberry Pi Pico W onboard LED
    if (pin == 25) {
        leds[index].cyw43 = true;
        if (index >= n_led) n_led = index + 1;
        return;
    }
#elif defined(PICO2_W)
    // Raspberry Pi Pico 2 W onboard LED
    if (pin == 25) {
        leds[index].cyw43 = true;
        if (index >= n_led) n_led = index + 1;
        return;
    }
#endif

    gpio_set_function(pin, GPIO_FUNC_PWM);

    uint slice = pwm_gpio_to_slice_num(pin);
    uint chan = pwm_gpio_to_channel(pin);

    pwm_set_wrap(slice, 255);
    pwm_set_enabled(slice, true);

    leds[index].slice = slice;
    leds[index].chan = chan;

    if (index >= n_led) n_led = index + 1;
}

inline void __not_in_flash_func(setLedHardware)(int index, float value) {
    if (index < 0 || index >= MAX_LEDS) return;
    if (leds[index].is_rgb || leds[index].cyw43) return;

    if (value < 0.0f) value = 0.0f;
    if (value > 1.0f) value = 1.0f;

    uint16_t level = (uint16_t)(value * value * 255.0f);

    pwm_set_chan_level(
        leds[index].slice,
        leds[index].chan,
        level
    );
}

inline void updateLed(int index, float val) {
    if (index < 0 || index >= MAX_LEDS) return;

    led_vals[index].store(val, std::memory_order_relaxed);

    if (!leds[index].is_rgb && !leds[index].cyw43) {
        setLedHardware(index, val);
    }
}

#ifdef PICO_CYW43_LED

// --------------------------------------------------
// CYW43 onboard LED
// --------------------------------------------------

inline void updateCyw43Led(int index) {
    if (index < 0 || index >= MAX_LEDS) return;
    if (!leds[index].cyw43) return;

    float value = led_vals[index].load(std::memory_order_relaxed);

    cyw43_arch_gpio_put(
        CYW43_WL_GPIO_LED_PIN,
        value > 0.01f
    );
}

#endif

#ifdef PICO_ZERO

// --------------------------------------------------
// WS2812 / NeoPixel
// --------------------------------------------------

inline void init_neopixel() {
    static bool initialized = false;

    if (initialized) return;

    PIO pio = pio1;

    if (!pio_can_add_program(pio, &ws2812_program)) return;

    uint offset = pio_add_program(pio, &ws2812_program);

    ws2812_program_init(
        pio,
        0,
        offset,
        16,
        800000,
        false
    );

    pio_sm_set_enabled(pio, 0, true);

    initialized = true;
}

inline void addRgbLed(
    int index,
    uint32_t pin,
    uint8_t r = 255,
    uint8_t g = 255,
    uint8_t b = 255
) {
    if (index < 0 || index >= MAX_LEDS) return;

    init_neopixel();

    leds[index].pin = pin;
    leds[index].is_rgb = true;
    leds[index].cyw43 = false;

    leds[index].r = r;
    leds[index].g = g;
    leds[index].b = b;

    if (index >= n_led) n_led = index + 1;
}

inline void updateRGB(int index, float hue, float intensity) {
    if (index < 0 || index >= MAX_LEDS) return;

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

    if (intensity < 0.0f) intensity = 0.0f;
    if (intensity > 1.0f) intensity = 1.0f;

    float gamma = intensity * intensity;

    uint8_t uR = (uint8_t)(r * gamma * 255.0f);
    uint8_t uG = (uint8_t)(g * gamma * 255.0f);
    uint8_t uB = (uint8_t)(b * gamma * 255.0f);

    led_framebuffer[index] =
        ((uint32_t)uG << 16) |
        ((uint32_t)uR << 8) |
        ((uint32_t)uB);
}

inline void showRGB() {
    if (pio_sm_get_tx_fifo_level(pio1, 0) > 4) return;

    pio1->txf[0] = led_framebuffer[0];
    pio1->txf[0] = 0;
    pio1->txf[0] = 0;
    pio1->txf[0] = 0;
}

#endif

} // namespace Pico