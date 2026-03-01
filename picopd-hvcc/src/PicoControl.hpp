#ifndef PICO_CONTROL_HPP
#define PICO_CONTROL_HPP

#include "pico/stdlib.h"
#include <atomic>
#include <cmath>

namespace Pico {

    enum ButtonMode {
        BANG   = 0,
        SWITCH = 1,  
        TOGGLE = 2,
        GATE_IN = 3    
    };

    struct Button {
        uint32_t pin;
        uint32_t mask;      
        std::atomic<bool> state;
        bool last;   
    //    bool inverted;     
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

    extern Button btns[12];
    extern Knob knobs[4];
    extern Led leds[12];
    extern std::atomic<float> led_vals[12];
    
    extern int n_btn;
    extern int n_knob;
    extern int n_led;

    void addPin(int index, uint32_t pin, ButtonMode mode);
    void addKnob(int index, uint32_t pin); 
    void addCV(int index, uint32_t pin);   
    void addLed(int index, uint32_t pin);   
    void update(uint32_t now);
    void updateLed(int index, float val); 
    void processPin(int i, float &outVal, bool &shouldSend);
    bool knobChanged(int i, float& outVal);
    bool buttonChanged(int i, bool& outState);
    bool buttonPressed(int i);
    bool buttonReleased(int i);
    bool buttonToggled(int i, bool& outState);
    void __not_in_flash_func(setLedHardware)(int index, float value);

}

#endif