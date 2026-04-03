#include "Keypad.h"
#include "hardware/i2c.h"
#include "pico_rgb_keypad.hpp"

namespace Pico {
    Keypad keypad;

    void addKeypad(uint sda, uint scl, float brightness) {
       
        sleep_ms(100);    // Give hardware a time to stabilize

        gpio_init(sda);
        gpio_init(scl);
        gpio_set_dir(sda, GPIO_IN);
        gpio_set_dir(scl, GPIO_IN);
        gpio_pull_up(sda);
        gpio_pull_up(scl);
        sleep_ms(10); 

        i2c_init(i2c1, 400000); 
        gpio_set_function(sda, GPIO_FUNC_I2C);
        gpio_set_function(scl, GPIO_FUNC_I2C);
        gpio_pull_up(sda);
        gpio_pull_up(scl);

        bool success = false;
        for (int retry = 0; retry < 5; retry++) {
            if (keypad.device.init()) {
                success = true;
                break;
            }
            sleep_ms(50); // Small wait before the next attempt
        }

        if (success) {
            keypad.device.set_brightness(brightness);
            // Optional: Quick flash of the LEDs to show it's alive
            for(int i=0; i<16; i++) keypad.device.illuminate(i, 20, 20, 20);
            keypad.device.update();
        } else {
            printf("RGB Keypad failed to respond on i2c1\n");
        }
    }

    void setKeypadStep(int row, int step) {
        if (row >= 0 && row < 4) keypad.steps[row] = step;
    }

    void clearKeypad() {
        keypad.clear(); // Call the internal method
    }

    void processKeypad(uint32_t now) {
        // 1. Button Scan (5ms)
        if (now - keypad.keypad_timer >= 5) {
            keypad.keypad_timer = now;
            uint16_t buttons = keypad.device.get_button_states();
            uint16_t pressed = buttons & ~keypad.prev_buttons;
            keypad.prev_buttons = buttons;

            for (int i = 0; i < 16; i++) {
                if (pressed & (1 << i)) keypad.selected[i] = !keypad.selected[i];
            }
        }

        if (now - keypad.blink_timer >= 200) {
            keypad.blink_timer = now;
            keypad.blink_state = !keypad.blink_state;
        }

        for (int row = 0; row < 4; row++) {
            for (int col = 0; col < 4; col++) {
                int i = (row * 4) + col;
                bool active = (col == keypad.steps[row]);
                bool sel = keypad.selected[i];

                uint8_t r = active ? 0 : (sel ? (keypad.blink_state ? 255 : 0) : 20);
                uint8_t g = active ? 255 : 0;
                uint8_t b = active ? 0 : (sel ? 0 : 80);

                keypad.device.illuminate(i, r, g, b);
            }
        }
        keypad.device.update();
    }
}




//     void processKeypad(uint32_t now) {
//         if (now - keypad.blink_timer >= 200) {
//             keypad.blink_timer = now;
//             keypad.blink_state = !keypad.blink_state;
//         }

//         uint16_t buttons = keypad.device.get_button_states();
//         uint16_t just_pressed = buttons & ~keypad.prev_buttons;
//         keypad.prev_buttons = buttons;

//         for (int i = 0; i < 16; i++) {
//             if (just_pressed & (1 << i)) {
//                 keypad.selected[i] = !keypad.selected[i];
//             }
//         }

//         // 3. LED Matrix Rendering
//         for (int row = 0; row < 4; row++) {
//             for (int col = 0; col < 4; col++) {
//                 int i = (row * 4) + col; // Index into the 16-element arrays
                
//                 bool is_playhead = (col == keypad.steps[row]);
//                 bool is_selected = keypad.selected[i];

//                 uint8_t r = 0, g = 0, b = 0;

//                 if (is_playhead) {
//                     // Playhead is Bright Green
//                     g = 255; 
//                 } else if (is_selected) {
//                     // Selected steps are Blue (Blinking)
//                     b = keypad.blink_state ? 250 : 60; 
//                 } else {
//                     // Empty background is Dim Blue
//                     b = 15; 
//                 }

//                 keypad.device.illuminate(i, r, g, b);
//             }
//         }
        
//         keypad.device.update();
//     }
// }
