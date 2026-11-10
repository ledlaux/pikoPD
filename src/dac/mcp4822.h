
#pragma once

#include <stdint.h>
#include <stdio.h>

#include "pico/stdlib.h"
#include "hardware/spi.h"

namespace Pico {

// ==================================================
// DAC output modes
// ==================================================

enum class MCPMode {
    CV,     // Normalized input: 0.0 - 1.0
    NOTE,   // MIDI note: 36 - 96
    RAW     // DAC value: 0 - 4095
};

enum class MCPChannel {
    A,
    B
};

// ==================================================
// MCP4822
// Dual-channel 12-bit SPI DAC
// ==================================================

class MCP4822 {

private:

    spi_inst_t* spi_port;

    uint8_t cs_pin;
    uint32_t baudrate;

    uint16_t last_raw_value[2];

    bool initialized;

    MCPMode channel_mode[2];

    MCPChannel channel;

    // MCP4822 command bits

    static constexpr uint16_t DAC_CHANNEL_A = (0 << 15);
    static constexpr uint16_t DAC_CHANNEL_B = (1 << 15);

    static constexpr uint16_t DAC_GAIN_1X = (1 << 13);
    static constexpr uint16_t DAC_ACTIVE  = (1 << 12);

    // --------------------------------------------------
    // Channel index
    // --------------------------------------------------

    static uint8_t channelIndex(MCPChannel ch) {
        return (ch == MCPChannel::A) ? 0 : 1;
    }

    // --------------------------------------------------
    // Validate common SPI SCK/MOSI pin pairs
    //
    // SPI0:
    // SCK  2, 6, 18, 22
    // MOSI 3, 7, 19, 23
    //
    // SPI1:
    // SCK  10, 14, 26
    // MOSI 11, 15, 27
    // --------------------------------------------------

    static bool isValidPinPair(
        spi_inst_t* port,
        uint sck_pin,
        uint mosi_pin
    ) {

        if (port == spi0) {

            return (
                (sck_pin == 2  && mosi_pin == 3)  ||
                (sck_pin == 6  && mosi_pin == 7)  ||
                (sck_pin == 18 && mosi_pin == 19) ||
                (sck_pin == 22 && mosi_pin == 23)
            );
        }

        if (port == spi1) {

            return (
                (sck_pin == 10 && mosi_pin == 11) ||
                (sck_pin == 14 && mosi_pin == 15) ||
                (sck_pin == 26 && mosi_pin == 27)
            );
        }

        return false;
    }

    // --------------------------------------------------
    // Automatically resolve SPI controller
    // --------------------------------------------------

    static spi_inst_t* resolveSPI(
        uint sck_pin,
        uint mosi_pin
    ) {

        if (isValidPinPair(spi0, sck_pin, mosi_pin)) {
            return spi0;
        }

        if (isValidPinPair(spi1, sck_pin, mosi_pin)) {
            return spi1;
        }

        return nullptr;
    }

    // --------------------------------------------------
    // Raw SPI write
    // --------------------------------------------------

    void __not_in_flash_func(writeRaw)(
        MCPChannel ch,
        uint16_t value
    ) {

        if (!initialized || !spi_port) {
            return;
        }

        if (value > 4095) {
            value = 4095;
        }

        uint8_t index = channelIndex(ch);

        last_raw_value[index] = value;

        uint16_t command =
            ((ch == MCPChannel::A) ? DAC_CHANNEL_A : DAC_CHANNEL_B) |
            DAC_GAIN_1X |
            DAC_ACTIVE |
            (value & 0x0FFF);

        uint8_t buffer[2] = {
            static_cast<uint8_t>(command >> 8),
            static_cast<uint8_t>(command & 0xFF)
        };

        gpio_put(cs_pin, 0);

        spi_write_blocking(spi_port, buffer, 2);

        gpio_put(cs_pin, 1);
    }

public:

    // ==================================================
    // Constructors
    // ==================================================

    // Automatically select SPI controller from pin pair.

    explicit MCP4822(uint8_t cs)
        : spi_port(nullptr),
          cs_pin(cs),
          baudrate(20000000),
          last_raw_value{0, 0},
          initialized(false),
          channel_mode{MCPMode::CV, MCPMode::CV},
          channel(MCPChannel::A) {
    }

    // Explicit SPI controller selection.

    MCP4822(spi_inst_t* port, uint8_t cs)
        : spi_port(port),
          cs_pin(cs),
          baudrate(20000000),
          last_raw_value{0, 0},
          initialized(false),
          channel_mode{MCPMode::CV, MCPMode::CV},
          channel(MCPChannel::A) {
    }

    // ==================================================
    // Initialization
    // ==================================================

    bool init(
        uint sck_pin,
        uint mosi_pin,
        MCPMode output_mode = MCPMode::CV,
        uint32_t spi_baudrate = 20000000
    ) {

        if (initialized) {
            printf("[MCP4822] Already initialized\n");
            return true;
        }

        // Select SPI controller automatically if necessary.

        if (spi_port == nullptr) {

            spi_port = resolveSPI(sck_pin, mosi_pin);

            if (spi_port == nullptr) {

                printf(
                    "[MCP4822] ERROR: Invalid SPI pin pair: SCK=%u, MOSI=%u\n",
                    sck_pin,
                    mosi_pin
                );

                return false;
            }
        }

        // Validate SPI controller and pin combination.

        if (!isValidPinPair(spi_port, sck_pin, mosi_pin)) {

            printf(
                "[MCP4822] ERROR: Pins SCK=%u, MOSI=%u do not match selected SPI controller\n",
                sck_pin,
                mosi_pin
            );

            return false;
        }

        // Validate CS pin.

        if (cs_pin == sck_pin || cs_pin == mosi_pin) {

            printf(
                "[MCP4822] ERROR: CS pin %u conflicts with SPI pins\n",
                cs_pin
            );

            return false;
        }

        // Set initial mode for both channels.

        channel_mode[0] = output_mode;
        channel_mode[1] = output_mode;

        // Initialize SPI.

        baudrate = spi_init(spi_port, spi_baudrate);

        spi_set_format(
            spi_port,
            8,
            SPI_CPOL_0,
            SPI_CPHA_0,
            SPI_MSB_FIRST
        );

        gpio_set_function(sck_pin, GPIO_FUNC_SPI);
        gpio_set_function(mosi_pin, GPIO_FUNC_SPI);

        // Manual chip select.

        gpio_init(cs_pin);
        gpio_set_dir(cs_pin, GPIO_OUT);
        gpio_put(cs_pin, 1);

        initialized = true;

        printf(
            "[MCP4822] Initialized: SPI%u, SCK=%u, MOSI=%u, CS=%u, baud=%lu\n",
            spi_port == spi0 ? 0u : 1u,
            sck_pin,
            mosi_pin,
            cs_pin,
            static_cast<unsigned long>(baudrate)
        );

        return true;
    }

    // ==================================================
    // Connection status
    // ==================================================

    bool isConnected() const {
        // MCP4822 has no SPI readback.
        return initialized && spi_port != nullptr;
    }

    bool isInitialized() const {
        return initialized;
    }

    // ==================================================
    // SPI information
    // ==================================================

    spi_inst_t* getSPI() const {
        return spi_port;
    }

    uint8_t getCSPin() const {
        return cs_pin;
    }

    uint32_t getBaudrate() const {
        return baudrate;
    }

    // ==================================================
    // Channel selection
    // ==================================================

    void setChannel(MCPChannel ch) {
        channel = ch;
    }

    MCPChannel getChannel() const {
        return channel;
    }

    // ==================================================
    // Independent channel modes
    // ==================================================

    void setMode(
        MCPChannel ch,
        MCPMode new_mode
    ) {
        channel_mode[channelIndex(ch)] = new_mode;
    }

    MCPMode getMode(MCPChannel ch) const {
        return channel_mode[channelIndex(ch)];
    }

    // ==================================================
    // RAW DAC output
    // ==================================================

    // Selected channel.

    void __not_in_flash_func(setVoltage)(
        uint16_t value
    ) {
        writeRaw(channel, value);
    }

    // Explicit channel.

    void __not_in_flash_func(writeChannelRaw)(
        MCPChannel ch,
        uint16_t value
    ) {
        writeRaw(ch, value);
    }

    // ==================================================
    // CV output
    // ==================================================

    // Normalized input: 0.0 - 1.0

    void __not_in_flash_func(setNormalized)(
        float normalized_val
    ) {

        if (normalized_val < 0.0f) {
            normalized_val = 0.0f;
        }

        if (normalized_val > 1.0f) {
            normalized_val = 1.0f;
        }

        uint16_t value = static_cast<uint16_t>(
            normalized_val * 4095.0f
        );

        writeRaw(channel, value);
    }

    // Explicit channel CV output.

    void __not_in_flash_func(writeChannelNormalized)(
        MCPChannel ch,
        float normalized_val
    ) {

        if (normalized_val < 0.0f) {
            normalized_val = 0.0f;
        }

        if (normalized_val > 1.0f) {
            normalized_val = 1.0f;
        }

        uint16_t value = static_cast<uint16_t>(
            normalized_val * 4095.0f
        );

        writeRaw(ch, value);
    }

    // ==================================================
    // NOTE output
    // ==================================================

    // MIDI note mapping: 36 - 96

    void __not_in_flash_func(noteOut)(
        float midi_note
    ) {

        if (midi_note < 36.0f) {
            midi_note = 36.0f;
        }

        if (midi_note > 96.0f) {
            midi_note = 96.0f;
        }

        float normalized = (midi_note - 36.0f) / 60.0f;

        setNormalized(normalized);
    }

    // Explicit channel NOTE output.

    void __not_in_flash_func(writeChannelNote)(
        MCPChannel ch,
        float midi_note
    ) {

        if (midi_note < 36.0f) {
            midi_note = 36.0f;
        }

        if (midi_note > 96.0f) {
            midi_note = 96.0f;
        }

        float normalized = (midi_note - 36.0f) / 60.0f;

        writeChannelNormalized(ch, normalized);
    }

    // ==================================================
    // Automatic channel output
    // ==================================================

    // Uses the configured mode of the selected channel.

    void __not_in_flash_func(write)(
        float input_value
    ) {
        writeChannel(channel, input_value);
    }

    // Uses the configured mode of the specified channel.

    void __not_in_flash_func(writeChannel)(
        MCPChannel ch,
        float input_value
    ) {

        if (!initialized) {
            return;
        }

        switch (channel_mode[channelIndex(ch)]) {

            case MCPMode::CV:

                writeChannelNormalized(ch, input_value);
                break;

            case MCPMode::NOTE:

                writeChannelNote(ch, input_value);
                break;

            case MCPMode::RAW:

                if (input_value < 0.0f) {
                    input_value = 0.0f;
                }

                if (input_value > 4095.0f) {
                    input_value = 4095.0f;
                }

                writeRaw(ch, static_cast<uint16_t>(input_value));
                break;

            default:

                writeChannelNormalized(ch, input_value);
                break;
        }
    }

    // ==================================================
    // Last output values
    // ==================================================

    // Selected channel.

    uint16_t getLastValue() const {
        return last_raw_value[channelIndex(channel)];
    }

    // Explicit channel.

    uint16_t getLastValue(MCPChannel ch) const {
        return last_raw_value[channelIndex(ch)];
    }

    // Backward-compatible alias.

    uint16_t getLastError() const {
        return getLastValue();
    }

};

} // namespace Pico
