/*
    Portions copyright (c) 2026 ledlaux 
    Rewritten and adapted 

    Original library:
    https://github.com/PBernalPolo/HX710
    
    Copyright (c) 2022 PBernalPolo

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
    SOFTWARE.
*/

#pragma once

#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/sync.h"
#include "hardware/gpio.h"

namespace Pico {

enum class OutputMode {
    RAW,
    MIDI
};

struct HX710Config {
    uint sck_pin;
    uint dout_pin;
    long min_raw;
    long max_raw;
    float fall_factor;
    uint32_t send_interval;
    uint8_t rise_step;
    OutputMode mode;
};

class HX710 {
public:
    explicit HX710(const HX710Config& cfg)
        : _cfg(cfg), _last_differential_input(0), _initialized(false),
          _min_threshold(cfg.min_raw + 5000), _last_val(0), _last_send(0) {}

    bool initialized() const { return _initialized; }

    bool tryInit() {
        if (_initialized) return true;

        gpio_init(_cfg.sck_pin);
        gpio_set_dir(_cfg.sck_pin, GPIO_OUT);
        gpio_put(_cfg.sck_pin, 0);

        gpio_init(_cfg.dout_pin);
        gpio_set_dir(_cfg.dout_pin, GPIO_IN);

        _initialized = true;
        return true;
    }

    bool isReady() const {
        return gpio_get(_cfg.dout_pin) == 0;
    }

    void read() {
        if (!_initialized) return;

        uint32_t value = 0;
        uint32_t save_flags = save_and_disable_interrupts();

        for (int i = 0; i < 24; i++) {
            gpio_put(_cfg.sck_pin, 1);
            sleep_us(1);
            value = (value << 1);
            gpio_put(_cfg.sck_pin, 0);
            if (gpio_get(_cfg.dout_pin)) {
                value++;
            }
            sleep_us(1);
        }

        gpio_put(_cfg.sck_pin, 1);
        sleep_us(1);
        gpio_put(_cfg.sck_pin, 0);
        sleep_us(1);

        restore_interrupts(save_flags);

        if (value & 0x800000) {
            value |= 0xFF000000;
        }

        _last_differential_input = (long)value;
    }

    long getLastReading() const {
        return _last_differential_input;
    }

    bool process(int32_t *out_val) {
        long raw = _last_differential_input;

        if (_cfg.mode == OutputMode::RAW) {
            uint32_t current_time = to_ms_since_boot(get_absolute_time());
            if (current_time - _last_send >= _cfg.send_interval) {
                _last_send = current_time;
                *out_val = raw;
                return true;
            }
            return false;
        }

        uint8_t target_val = 0;
        if (raw >= _min_threshold) {
            target_val = pressure_to_value(raw);
        }

        uint32_t current_time = to_ms_since_boot(get_absolute_time());

        if (current_time - _last_send >= _cfg.send_interval) {
            if (target_val > _last_val) {
                uint16_t temp = _last_val + _cfg.rise_step;
                _last_val = (temp > target_val) ? target_val : (uint8_t)temp;
            } else if (target_val < _last_val) {
                _last_val = (uint8_t)((float)_last_val * _cfg.fall_factor);
                if (_last_val < target_val) _last_val = target_val;
            } else if (target_val == 0 && _last_val != 0) {
                _last_val = 0;
            } else {
                return false;
            }

            _last_send = current_time;
            *out_val = _last_val;
            return true;
        }

        return false;
    }

private:
    const HX710Config _cfg;
    long _last_differential_input;
    bool _initialized;
    long _min_threshold;
    uint8_t _last_val;
    uint32_t _last_send;

    long constrain_long(long x, long xm, long xx) const {
        if (x < xm) return xm;
        if (x > xx) return xx;
        return x;
    }

    long map_long(long x, long in_min, long in_max, long out_min, long out_max) const {
        return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
    }

    uint8_t pressure_to_value(long raw) const {
        long constrained = constrain_long(raw, _cfg.min_raw, _cfg.max_raw);
        return (uint8_t)map_long(constrained, _cfg.min_raw, _cfg.max_raw, 0, 127);
    }
};

} // namespace Pico

