#pragma once

#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include <stdio.h> 

namespace Pico {

    enum class MCPMode {
        CV,
        NOTE
    };

    class MCP4725 {
    private:
        i2c_inst_t* i2c_port;
        uint8_t address;
        uint32_t last_raw_value;
        bool initialized;
        MCPMode mode;

    public:
        // Default constructor: port resolved dynamically in init() based on SDA pin
        MCP4725(uint8_t addr = 0x60) 
            : i2c_port(nullptr), address(addr), last_raw_value(0), initialized(false), mode(MCPMode::CV) {}

        // Explicit constructor allowing manual i2c0 or i2c1 selection
        MCP4725(i2c_inst_t* port, uint8_t addr = 0x60) 
            : i2c_port(port), address(addr), last_raw_value(0), initialized(false), mode(MCPMode::CV) {}

        bool init(uint sda_pin, uint scl_pin, MCPMode output_mode = MCPMode::CV, uint32_t baudrate = 400000) {
            mode = output_mode;

            // Resolve i2c0 or i2c1 dynamically based on RP2040 GPIO pin assignments
            if (i2c_port == nullptr) {
                i2c_port = ((sda_pin >> 1) & 1) ? i2c1 : i2c0;
            }

            // Initialize I2C peripheral hardware
            i2c_init(i2c_port, baudrate);
            gpio_set_function(sda_pin, GPIO_FUNC_I2C);
            gpio_set_function(scl_pin, GPIO_FUNC_I2C);
            gpio_pull_up(sda_pin);
            gpio_pull_up(scl_pin);

            // Ping device with a 5ms timeout to prevent hanging if hardware is missing
            uint8_t dummy = 0;
            int result = i2c_read_timeout_us(i2c_port, address, &dummy, 1, false, 5000);
            initialized = (result >= 0);
            
            // printf("MCP4725 Init: Addr=0x%02X, Initialized=%d\n", address, initialized);

            return initialized;
        }

        bool isConnected() {
            if (!initialized || !i2c_port) return false;
            uint8_t dummy = 0;
            bool connected = (i2c_read_timeout_us(i2c_port, address, &dummy, 1, false, 2000) >= 0);
            // printf("MCP4725 Check Connection: Addr=0x%02X, Connected=%d\n", address, connected);
            return connected;
        }

        void __not_in_flash_func(setVoltage)(uint16_t value) {
            if (!initialized || !i2c_port) {
                // printf("MCP4725 Error: Attempted write to uninitialized device at 0x%02X\n", address);
                return;
            }

            if (value > 4095) value = 4095;
            last_raw_value = value;

            uint8_t buf[2];
            buf[0] = (value >> 8) & 0x0F;
            buf[1] = value & 0xFF;

            // 2ms timeout to prevent deadlocking if I2C bus fails
            int result = i2c_write_timeout_us(i2c_port, address, buf, 2, false, 2000);
            
            // printf("MCP4725 Write: Addr=0x%02X, RawVal=%u, I2CResult=%d\n", address, value, result);
        }

        void __not_in_flash_func(setNormalized)(float normalized_val) {
            if (normalized_val < 0.0f) normalized_val = 0.0f;
            if (normalized_val > 1.0f) normalized_val = 1.0f;
            
            // printf("MCP4725 Normalized Input: %.4f\n", normalized_val);
            
            uint16_t val = (uint16_t)(normalized_val * 4095.0f);
            setVoltage(val);
        }

        // Dedicated note-out helper function (maps MIDI note space 36.0 to 96.0 -> 0.0 to 1.0)
        void __not_in_flash_func(noteOut)(float midi_note) {
            if (midi_note < 36.0f) midi_note = 36.0f;
            if (midi_note > 96.0f) midi_note = 96.0f;
            
            // printf("MCP4725 Note Input: %.2f\n", midi_note);
            
            float normalized = (midi_note - 36.0f) / 60.0f;
            setNormalized(normalized);
        }

        // Standardized processing function handling both modes internally
        void __not_in_flash_func(write)(float input_value) {
            if (!initialized) return;

            if (mode == MCPMode::NOTE) {
                noteOut(input_value);
            } else {
                // Direct CV mode
                setNormalized(input_value);
            }
        }

        uint16_t getLastError() const {
            return last_raw_value;
        }
    };

} // namespace Pico