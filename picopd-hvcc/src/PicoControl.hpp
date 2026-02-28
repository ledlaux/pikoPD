
#ifndef PICO_CONTROL_HPP
#define PICO_CONTROL_HPP

#include "pico/stdlib.h"
#include <atomic>
#include <cmath>

namespace Pico {

    const uint32_t BANG_PULSE_WIDTH_MS = 50;

    enum ButtonMode {
        BANG   = 0,
        SWITCH   = 1,  
        TOGGLE = 2    
    };

    struct Button {
        uint32_t pin;
        std::atomic<bool> state;
        bool last;        
        bool raw_prev;   
        uint32_t last_time;
        ButtonMode mode;
        bool toggle_state;
        uint32_t reset_at = 0;
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
    void addBtn(int index, uint32_t pin, ButtonMode mode);  
    void addKnob(int index, uint32_t pin);  
    void addLed(int index, uint32_t pin);   
    void setLedHardware(int index, float value);
    void processButton(int i, float &outVal, bool &shouldSend);

    bool buttonChanged(int i, bool& outState);
    bool buttonPressed(int i);
    bool buttonReleased(int i);
    bool buttonToggled(int i, bool& outState);
    bool knobChanged(int i, float& outVal);
}

#endif
