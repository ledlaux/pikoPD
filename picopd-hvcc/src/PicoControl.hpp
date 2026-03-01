#ifndef PICO_CONTROL_HPP
#define PICO_CONTROL_HPP

#include "pico/stdlib.h"
#include <atomic>
#include <cmath>

namespace Pico {

    enum PinMode {
        BANG   = 0,
        SWITCH = 1,  
        TOGGLE = 2,
        GATE_IN = 3, 
        GATE_OUT = 4   
    };

    struct Button {
        uint32_t pin;
        uint32_t mask;      
        std::atomic<bool> state;
        bool last;   
     // bool inverted;     
        bool raw_prev;   
        uint32_t last_time;
        PinMode mode;
        bool toggle_state;
        uint32_t reset_at = 0;
        uint32_t pulse_duration;
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

    struct Encoder {
        uint32_t pinA;
        uint32_t pinB;
        bool last_clk;      
        bool last_dt;      
       // int8_t lastState;
        int last_sent_count;      
        std::atomic<int> value;
        };

    extern Button btns[12];
    extern Knob knobs[4];
    extern Led leds[12];
    extern Encoder encoder[4];

    extern std::atomic<float> led_vals[12];
    
    extern int n_btn;
    extern int n_knob;
    extern int n_led;
    extern int n_encoder;

    void addPin(int index, uint32_t pin, PinMode mode, uint32_t duration = 0);
    void addKnob(int index, uint32_t pin); 
    void addCV(int index, uint32_t pin);   
    void addEncoder(int index, uint32_t pinA, uint32_t pinB);
    void addLed(int index, uint32_t pin);   
    void update(uint32_t now);
    void updateLed(int index, float val); 
    void updateGate(int index, float val);
    void processPin(int i, float &outVal, bool &shouldSend);
    bool knobChanged(int i, float& outVal);
    bool buttonChanged(int i, bool& outState);
    bool encoderChanged(int index, float &val);
    bool buttonPressed(int i);
    bool buttonReleased(int i);
    bool buttonToggled(int i, bool& outState);
    void __not_in_flash_func(setLedHardware)(int index, float value);

}

#endif
