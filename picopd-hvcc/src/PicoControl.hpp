
#ifndef PICO_CONTROL_HPP
#define PICO_CONTROL_HPP

#include "pico/stdlib.h"
#include <atomic>
#include <cmath>

namespace Pico {

    enum ButtonMode {
        MODE_TOGGLE = 0, 
        MODE_BANG   = 1  
    };

    struct Button {
        uint32_t pin;
        std::atomic<bool> state;
        bool last;        
        bool raw_prev;   
        uint32_t last_time;
        ButtonMode mode;
        bool toggle_state;
    };

    struct Knob {
        uint32_t adc_ch;
        std::atomic<float> value;
        float last_val;  
        float coeff; 
    };

    struct Led {
        uint32_t pin;
        uint slice;
        uint chan;
    };

    extern Button btns[16];
    extern Knob knobs[8];
    extern Led leds[16];
    extern std::atomic<float> led_vals[16];
    extern int n_btn, n_knob, n_led;

    void update();
    void addBtn(int index, uint32_t pin);   
    void addKnob(int index, uint32_t pin);  
    void addLed(int index, uint32_t pin);   
    void setLedHardware(int index, float value);

    bool buttonChanged(int i, bool& outState);
    bool buttonPressed(int i);
    bool buttonReleased(int i);
    bool buttonToggled(int i, bool& outState);
    bool knobChanged(int i, float& outVal);
}

#endif
