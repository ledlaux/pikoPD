#pragma once

#include <stdio.h>
#include <math.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"

class BreathController {
public:
    enum ScalingMode {
        LINEAR,     // Standard map from raw to MIDI
        EXPONENTIAL // Uses a curve power for natural "feel"
    };

    struct Settings {
        // Hardware Pins
        uint8_t pinData;
        uint8_t pinClock;
        uint8_t pinPot;

        // Calibration & Scaling
        int32_t rawMin;
        int32_t rawMax;
        float curve;           // 1.0 = Linear, >1.0 = Log-like (softer start)
        ScalingMode mode;

        // Smoothing & MIDI
        uint8_t riseStep;
        uint8_t midiCC;
        uint8_t midiChan;
    };

    // Constructor accepts the settings struct
    BreathController(Settings settings) : _s(settings) {}

    void begin() {
        gpio_init(_s.pinData);
        gpio_set_dir(_s.pinData, GPIO_IN);
        gpio_pull_up(_s.pinData);

        gpio_init(_s.pinClock);
        gpio_set_dir(_s.pinClock, GPIO_OUT);
        gpio_put(_s.pinClock, false);

        adc_init();
        adc_gpio_init(_s.pinPot);
        adc_select_input(_s.pinPot - 26);

        _lastSend = to_ms_since_boot(get_absolute_time());
    }

    void update() {
        if (gpio_get(_s.pinData) == 0) {
            int32_t raw = fetchRaw();
            uint8_t target = scaleToMidi(raw);

            uint16_t potRaw = adc_read();
            float fall = mapF(potRaw, 0, 4095, 0.90f, 0.99f);
            uint32_t interval = (uint32_t)mapL(potRaw, 0, 4095, 10, 50);

            uint32_t now = to_ms_since_boot(get_absolute_time());
            if (now - _lastSend >= interval) {
                applySmoothing(target, fall);
                _lastSend = now;
            }
        }
    }

private:
    Settings _s;
    uint8_t _lastVal = 0;
    uint32_t _lastSend = 0;

    int32_t fetchRaw() {
        int32_t val = 0;
        for (int i = 0; i < 24; i++) {
            gpio_put(_s.pinClock, true);
            sleep_us(1);
            val <<= 1;
            if (gpio_get(_s.pinData)) val++;
            gpio_put(_s.pinClock, false);
            sleep_us(1);
        }
        // HX710B 40Hz Differential Pulses (25, 26, 27)
        for (int i = 0; i < 3; i++) {
            gpio_put(_s.pinClock, true); sleep_us(1);
            gpio_put(_s.pinClock, false); sleep_us(1);
        }
        if (val & 0x800000) val |= 0xFF000000;
        return val;
    }

    uint8_t scaleToMidi(int32_t raw) {
        if (_s.mode == LINEAR) {
            if (raw < _s.rawMin) return 0;
            if (raw > _s.rawMax) return 127;
            return (uint8_t)((raw - _s.rawMin) * 127 / (_s.rawMax - _s.rawMin));
        } 
        else { // EXPONENTIAL
            float norm = (float)(raw - _s.rawMin) / (float)(_s.rawMax - _s.rawMin);
            if (norm < 0.0f) norm = 0.0f;
            if (norm > 1.0f) norm = 1.0f;
            return (uint8_t)(powf(norm, _s.curve) * 127.0f);
        }
    }

    void applySmoothing(uint8_t target, float fall) {
        if (target > _lastVal) {
            _lastVal = (target < (_lastVal + _s.riseStep)) ? target : (_lastVal + _s.riseStep);
        } else if (target < _lastVal) {
            _lastVal = (target == 0) ? 0 : (uint8_t)(_lastVal * fall);
            if (_lastVal < target) _lastVal = target;
        } else {
            return;
        }
        printf("MIDI: %d\n", _lastVal); // Replace with MIDI send call
    }

    float mapF(float x, float in_min, float in_max, float out_min, float out_max) {
        return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
    }

    long mapL(long x, long in_min, long in_max, long out_min, long out_max) {
        return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
    }
};
