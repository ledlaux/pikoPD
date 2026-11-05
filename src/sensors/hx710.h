/*
    Portions copyright (c) 2026 ledlaux
    Adapted for pikoPD

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
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
*/

#pragma once

#include <stdint.h>
#include <algorithm>

#include "pico/stdlib.h"
#include "hardware/sync.h"
#include "hardware/gpio.h"

namespace Pico {

enum class OutputMode {
    RAW,
    FLOAT,
    MIDI
};

struct HX710Config {
    uint sck_pin;
    uint dout_pin;

    long min_raw = 704000;
    long max_raw = 1004000;

    float release = 0.50f;

    // 0.99	Very slow 
    // 0.95	Slow 
    // 0.80	Medium-fast 
    // 0.50	Fast 
    // 0.20	Very fast 

    uint32_t send_interval = 20;
    uint8_t rise_step = 2;

    OutputMode mode = OutputMode::MIDI;
};

class HX710 {
public:
    explicit HX710(const HX710Config& cfg)
        : _cfg(cfg),
          _last_differential_input(0),
          _initialized(false),
          _last_val(0),
          _last_send(0),
          _has_reading(false) {}

    bool initialized() const {
        return _initialized;
    }

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
        return _initialized && gpio_get(_cfg.dout_pin) == 0;
    }

    // Read one complete 24-bit conversion.
    bool read() {
        if (!_initialized || !isReady()) {
            return false;
        }

        uint32_t value = 0;
        uint32_t save_flags = save_and_disable_interrupts();

        for (int i = 0; i < 24; ++i) {
            gpio_put(_cfg.sck_pin, 1);
            sleep_us(1);

            value <<= 1;

            gpio_put(_cfg.sck_pin, 0);

            if (gpio_get(_cfg.dout_pin)) {
                value++;
            }

            sleep_us(1);
        }

        // 25th pulse selects the next conversion.
        gpio_put(_cfg.sck_pin, 1);
        sleep_us(1);
        gpio_put(_cfg.sck_pin, 0);
        sleep_us(1);

        restore_interrupts(save_flags);

        // Sign-extend 24-bit two's-complement value.
        if (value & 0x800000) {
            value |= 0xFF000000;
        }

        _last_differential_input = static_cast<int32_t>(value);
        _has_reading = true;

        return true;
    }

    long getLastReading() const {
        return _last_differential_input;
    }

    // RAW: signed sensor reading.
    // MIDI: smoothed value from 0 to 127.
    bool process(int32_t* out_val) {
        if (!out_val || !_initialized || !_has_reading) {
            return false;
        }

        if (_cfg.mode == OutputMode::FLOAT) {
            return false;
        }

        const uint32_t now =
            to_ms_since_boot(get_absolute_time());

        if (now - _last_send < _cfg.send_interval) {
            return false;
        }

        if (_cfg.mode == OutputMode::RAW) {
            *out_val = static_cast<int32_t>(_last_differential_input);
            _last_send = now;
            return true;
        }

        if (_cfg.mode != OutputMode::MIDI) {
            return false;
        }

        uint8_t target = 0;

        if (_last_differential_input >= _cfg.min_raw + 5000) {
            target = pressureToValue(_last_differential_input);
        }

        if (target > _last_val) {
            const uint16_t next =
                static_cast<uint16_t>(_last_val) + _cfg.rise_step;

            _last_val = (next > target)
                ? target
                : static_cast<uint8_t>(next);
        } else if (target < _last_val) {
            _last_val = static_cast<uint8_t>(
                static_cast<float>(_last_val) * _cfg.release
            );

            if (_last_val < target) {
                _last_val = target;
            }
        } else {
            return false;
        }

        _last_send = now;
        *out_val = _last_val;

        return true;
    }

    // FLOAT: normalized calibration range from 0.0 to 1.0.
    bool process(float* out_val) {
        if (!out_val || !_initialized || !_has_reading) {
            return false;
        }

        if (_cfg.mode != OutputMode::FLOAT) {
            return false;
        }

        const uint32_t now =
            to_ms_since_boot(get_absolute_time());

        if (now - _last_send < _cfg.send_interval) {
            return false;
        }

        _last_send = now;

        if (_cfg.max_raw <= _cfg.min_raw) {
            *out_val = 0.0f;
            return true;
        }

        const float numerator = static_cast<float>(
            static_cast<int64_t>(_last_differential_input) -
            static_cast<int64_t>(_cfg.min_raw)
        );

        const float denominator = static_cast<float>(
            static_cast<int64_t>(_cfg.max_raw) -
            static_cast<int64_t>(_cfg.min_raw)
        );

        *out_val = std::clamp(
            numerator / denominator,
            0.0f,
            1.0f
        );

        return true;
    }

private:
    const HX710Config _cfg;

    long _last_differential_input;
    bool _initialized;
    uint8_t _last_val;
    uint32_t _last_send;
    bool _has_reading;

    long constrainLong(long value, long minimum, long maximum) const {
        return std::clamp(value, minimum, maximum);
    }

    uint8_t pressureToValue(long raw) const {
        if (_cfg.max_raw <= _cfg.min_raw) {
            return 0;
        }

        const long constrained = constrainLong(
            raw,
            _cfg.min_raw,
            _cfg.max_raw
        );

        const float normalized =
            static_cast<float>(
                static_cast<int64_t>(constrained) - _cfg.min_raw
            ) /
            static_cast<float>(
                static_cast<int64_t>(_cfg.max_raw) - _cfg.min_raw
            );

        return static_cast<uint8_t>(
            std::clamp(normalized * 127.0f, 0.0f, 127.0f)
        );
    }
};

} // namespace Pico
