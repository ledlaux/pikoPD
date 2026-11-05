/*
 *  Simple HX711 load cell driver
 *  for Raspberry Pi Pico (RP2040/RP2350).
 *
 *  Supports raw readings, normalized weight values and MIDI output.
 *
 *  Configuration parameters:
 *    sck_pin           - HX711 clock (SCK) GPIO pin.
 *    dout_pin          - HX711 data (DOUT) GPIO pin.
 *    min_raw           - Minimum raw reading (unloaded sensor).
 *    max_raw           - Maximum raw reading (full weight).
 *    release_drop_rate - MIDI release speed (units per update).
 *    send_interval     - Minimum output interval in milliseconds.
 *    mode              - Output mode: RAW, FLOAT or MIDI.
 *    gain              - HX711 gain: A128, B32 or A64.
 *    sensitivity       - Sensor sensitivity multiplier.
 *
 */

#pragma once

#include <stdint.h>
#include <algorithm>

#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/sync.h"
#include "pico/time.h"

namespace Pico {

enum class OutputMode {
    RAW,
    FLOAT,
    MIDI
};

enum class HX711Gain : uint8_t {
    A128 = 1,
    B32  = 2,
    A64  = 3
};

struct HX711Config {
    uint sck_pin;
    uint dout_pin;

    // HX711 readings (manually calibrated with RAW)
    long min_raw = -131945;
    long max_raw = 2200000;

    uint8_t release_drop_rate = 10;
    uint32_t send_interval = 20;

    OutputMode mode = OutputMode::MIDI;
    HX711Gain gain = HX711Gain::A128;

    float sensitivity = 1.0f;
};

class HX711 {
public:

    explicit HX711(const HX711Config& config)
        : _cfg(config) {
    }

    bool initialized() const {
        return _initialized;
    }

    bool calibrated() const {
        return _cfg.max_raw > _cfg.min_raw;
    }

    bool tryInit() {
        gpio_init(_cfg.sck_pin);
        gpio_set_dir(_cfg.sck_pin, GPIO_OUT);
        gpio_put(_cfg.sck_pin, 0);

        gpio_init(_cfg.dout_pin);
        gpio_set_dir(_cfg.dout_pin, GPIO_IN);
        gpio_pull_up(_cfg.dout_pin);

        _initialized = true;
        _last_send_time = currentTime();

        return true;
    }

    bool isReady() const {
        return _initialized && gpio_get(_cfg.dout_pin) == 0;
    }


    bool read() {
        if (!isReady()) {
            return false;
        }

        uint32_t raw = 0;

        const uint32_t irq_state = save_and_disable_interrupts();

        for (int i = 0; i < 24; ++i) {
            gpio_put(_cfg.sck_pin, 1);
            busy_wait_us_32(1);

            raw <<= 1;

            if (gpio_get(_cfg.dout_pin)) {
                raw |= 1;
            }

            gpio_put(_cfg.sck_pin, 0);
            busy_wait_us_32(1);
        }

        // Select the next conversion gain.
        const uint8_t pulses = static_cast<uint8_t>(_cfg.gain);

        for (uint8_t i = 0; i < pulses; ++i) {
            gpio_put(_cfg.sck_pin, 1);
            busy_wait_us_32(1);
            gpio_put(_cfg.sck_pin, 0);
            busy_wait_us_32(1);
        }

        restore_interrupts(irq_state);

        // Sign-extend the 24-bit reading.
        int32_t signed_value;

        if (raw & 0x800000) {
            signed_value = static_cast<int32_t>(raw | 0xFF000000);
        } else {
            signed_value = static_cast<int32_t>(raw);
        }

        // Preserve the original sensor polarity.
        _last_reading =
            static_cast<long>(-static_cast<int64_t>(signed_value));

        return true;
    }

    long getLastReading() const {
        return _last_reading;
    }

    long getFilteredReading() const {
        return _last_reading;
    }

    // Difference from the unloaded raw reading.
    // Uses signed 64-bit arithmetic to prevent overflow.
    long getNetReading() const {
        const int64_t net =
            static_cast<int64_t>(_last_reading) -
            static_cast<int64_t>(_cfg.min_raw);

        if (net <= 0) {
            return 0;
        }

        if (net > INT32_MAX) {
            return INT32_MAX;
        }

        return static_cast<long>(net);
    }

    long getZeroOffset() const {
        return _cfg.min_raw;
    }

    void setZeroOffset(long value) {
        _cfg.min_raw = value;
    }

    void setRange(long minimum, long maximum) {
        _cfg.min_raw = minimum;
        _cfg.max_raw = maximum;
    }

    // Manual tare: current raw reading becomes the new minimum.
    void tare() {
        if (_initialized) {
            _cfg.min_raw = _last_reading;
        }
    }

    void tare(long value) {
        _cfg.min_raw = value;
    }


    float calculateLoad() const {
        if (!calibrated()) {
            return 0.0f;
        }

        const int64_t raw = static_cast<int64_t>(_last_reading);
        const int64_t minimum = static_cast<int64_t>(_cfg.min_raw);
        const int64_t maximum = static_cast<int64_t>(_cfg.max_raw);

        if (raw <= minimum) {
            return 0.0f;
        }

        if (raw >= maximum) {
            return 1.0f;
        }

        const float normalized =
            static_cast<float>(raw - minimum) /
            static_cast<float>(maximum - minimum);

        return std::clamp(
            normalized * _cfg.sensitivity,
            0.0f,
            1.0f
        );
    }

    float getValue() const {
        return calculateLoad();
    }

    uint8_t getMidiValue() const {
        return _midi_value;
    }

    // RAW output
    bool process(int32_t* output) {
        if (!output || !shouldSend()) {
            return false;
        }

        *output = static_cast<int32_t>(_last_reading);
        return true;
    }

    // FLOAT output
    bool process(float* output) {
        if (!output || !shouldSend()) {
            return false;
        }

        *output = calculateLoad();
        return true;
    }

    // MIDI output with immediate attack and stepped release
    bool process(uint8_t* output) {
        if (!output || !shouldSend()) {
            return false;
        }

        const float load = calculateLoad();

        const uint8_t target = static_cast<uint8_t>(
            std::clamp(load * 127.0f, 0.0f, 127.0f)
        );

        if (target > _midi_value) {
            // Immediate attack
            _midi_value = target;
        } else if (target < _midi_value) {
            // Gradual release
            const uint8_t drop = _cfg.release_drop_rate;

            if (_midi_value - target <= drop) {
                _midi_value = target;
            } else {
                _midi_value -= drop;
            }
        }

        *output = _midi_value;
        return true;
    }

private:

    HX711Config _cfg;

    bool _initialized = false;

    long _last_reading = 0;

    uint8_t _midi_value = 0;

    uint32_t _last_send_time = 0;

    static uint32_t currentTime() {
        return to_ms_since_boot(get_absolute_time());
    }

    bool shouldSend() {
        if (!_initialized) {
            return false;
        }

        const uint32_t now = currentTime();

        if (now - _last_send_time < _cfg.send_interval) {
            return false;
        }

        _last_send_time = now;
        return true;
    }
};

} // namespace Pico
