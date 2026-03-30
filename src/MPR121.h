/*
  Multi-bus capacitive touch sensor driver for Raspberry Pi Pico

  This header provides a C++ class for managing MPR121 capacitive touch sensors
  over multiple I2C buses. Each sensor is configured individually via the MPR121Config.

  Features:
  - Supports multiple sensors on different I2C buses (i2c0, i2c1, etc.).
  - Non-blocking initialization and soft reset.
  - Touch/release detection for 12 electrodes per sensor using a shared static IRQ callback.
  - Logging includes I2C port and address to uniquely identify each sensor.

  Copyright (c) 2026 Vadims Maksimovs MIT license
  All rights reserved.
*/

#pragma once

#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/gpio.h"

namespace Pico {

// MPR121 sensor configuration
struct MPR121Config {
    i2c_inst_t* i2c_port;   // I2C bus
    uint sda_pin;            // SDA pin
    uint scl_pin;            // SCL pin
    uint irq_pin;            // IRQ pin
    uint8_t addr_index = 0;  // index into ADDR_TABLE
};

static constexpr uint8_t ADDR_TABLE[4] = {0x5A, 0x5B, 0x5C, 0x5D};

class MPR121 {
public:
    explicit MPR121(const MPR121Config& cfg)
        : _cfg(cfg), _last_touched(0), _initialized(false), _irq_flag(false) {}

    bool initialized() const { return _initialized; }
    void set_irq_flag() { _irq_flag = true; }
    
uint16_t getTouched() { return read_touched(); }

    bool tryInit() {
        if (_initialized) return true;

        uint8_t addr = ADDR_TABLE[_cfg.addr_index];

        // Setup I2C pins
        gpio_set_function(_cfg.sda_pin, GPIO_FUNC_I2C);
        gpio_set_function(_cfg.scl_pin, GPIO_FUNC_I2C);
        gpio_pull_up(_cfg.sda_pin);
        gpio_pull_up(_cfg.scl_pin);

        // Setup IRQ pin
        gpio_init(_cfg.irq_pin);
        gpio_set_dir(_cfg.irq_pin, GPIO_IN);
        gpio_pull_up(_cfg.irq_pin);
        gpio_set_irq_enabled_with_callback(_cfg.irq_pin, GPIO_IRQ_EDGE_FALL, true,
                                          &MPR121::irq_callback_static);

        // Test I2C presence
        uint8_t test;
        if (i2c_read_blocking(_cfg.i2c_port, addr, &test, 1, false) < 0) return false;

        // Soft reset
        write_reg(0x5E, 0x00);   // stop mode
        write_reg(0x80, 0x63);   // soft reset
        sleep_ms(10);

        // Touch/release thresholds
        for (int i = 0; i < 12; i++) {
            write_reg(0x41 + i * 2, 6); // touch
            write_reg(0x42 + i * 2, 3); // release
        }

        // Filter / charge settings
        write_reg(0x5B, 0x01);
        write_reg(0x5C, 0x10);
        write_reg(0x5D, 0x20);

        // Enable electrodes
        write_reg(0x5E, 0x8F);
        sleep_ms(10);

        _initialized = true;
        return true;
    }

    // Poll and print touches/releases
    void processMPR121() {
        if (!_irq_flag) return;
        _irq_flag = false;

        uint16_t touched = read_touched();
        if (touched == 0xFFFF) return;

        if (touched != _last_touched) {
            // Print bus pointer + address to uniquely identify the sensor
        //    printf("Sensor on bus %p addr 0x%02X Mask: %04X\n",
                //    _cfg.i2c_port, ADDR_TABLE[_cfg.addr_index], touched);

            for (int i = 0; i < 12; i++) {
                bool now = touched & (1 << i);
                bool before = _last_touched & (1 << i);
                // if (now && !before)
                //     printf("Sensor bus %p addr 0x%02X PAD %d TOUCH\n",
                //            _cfg.i2c_port, ADDR_TABLE[_cfg.addr_index], i);
                // if (!now && before)
                //     printf("Sensor bus %p addr 0x%02X PAD %d RELEASE\n",
                //            _cfg.i2c_port, ADDR_TABLE[_cfg.addr_index], i);
            }
            _last_touched = touched;
        }
    }

private:
    const MPR121Config _cfg;
    uint16_t _last_touched;
    bool _initialized;
    volatile bool _irq_flag;

    uint8_t addr() const { return ADDR_TABLE[_cfg.addr_index]; }

    void write_reg(uint8_t reg, uint8_t val) {
        uint8_t buf[2] = {reg, val};
        i2c_write_blocking(_cfg.i2c_port, addr(), buf, 2, false);
    }

    uint16_t read_touched() {
        uint8_t reg = 0x00;
        uint8_t data[2];
        if (i2c_write_blocking(_cfg.i2c_port, addr(), &reg, 1, true) < 0) return 0xFFFF;
        if (i2c_read_blocking(_cfg.i2c_port, addr(), data, 2, false) < 0) return 0xFFFF;
        return (data[0] | (data[1] << 8)) & 0x0FFF;
    }


    static void irq_callback_static(uint gpio, uint32_t events) {
        (void)events;
        for (int i = 0; i < _num_sensors; ++i) {
            if (sensors[i] && sensors[i]->_cfg.irq_pin == gpio) {
                sensors[i]->set_irq_flag();
            }
        }
    }

public:
    static inline MPR121* sensors[4] = {nullptr}; // max 4 sensors
    static inline int _num_sensors = 0;
};

} // namespace Pico

