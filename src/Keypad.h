#ifndef PICO_KEYPAD_HPP
#define PICO_KEYPAD_HPP

#include "pico/stdlib.h"
#include "pico_rgb_keypad.hpp" // Pimoroni header

namespace Pico {
    struct Keypad {
        pimoroni::PicoRGBKeypad device;
        bool selected[16] = {false};
        int steps[4] = {-1, -1, -1, -1};
        uint16_t prev_buttons = 0;
        uint32_t keypad_timer = 0;
        uint32_t blink_timer = 0;
        bool blink_state = false;

        void clear() {                  // clear steps
            for (int i = 0; i < 16; i++) {
                selected[i] = false;
            }
        }
    };

    extern Keypad keypad;

    void addKeypad(uint sda, uint scl, float brightness = 0.5f);
    void processKeypad(uint32_t now);
    void setKeypadStep(int row, int step);
    void clearKeypad();
}

#endif