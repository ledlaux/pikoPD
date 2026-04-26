#pragma once

#include "pico/stdlib.h"
#include <atomic>
#include <cmath>

#ifdef MPR121_ENABLED
    #include "sensors/MPR121.h"
#endif

#ifdef DISTANCE_SENSOR_ENABLED
    #include "sensors/HC-SR04.h"
#endif

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
        bool is_rgb; 
        uint8_t r, g, b;
    };

    struct Encoder {
        uint32_t pinA;
        uint32_t pinB;
        bool last_clk;      
        bool last_dt;      
        int last_sent_count;      
        std::atomic<int> value;
        };

    struct Joystick {
        uint8_t adcX;
        uint8_t adcY;
        uint16_t centerX = 2048;
        uint16_t centerY = 2048;
        float smoothX = 2048.0f;
        float smoothY = 2048.0f;
        std::atomic<int16_t> x;
        std::atomic<int16_t> y;
        float lastSentX = -1.0f;
        float lastSentY = -1.0f;
    };

    struct CNY70 {
        int adc_ch;          // ADC channel
        float smooth_value;  // smoothed analog reading
        int last_val;        // last sent value
        float alpha;         // smoothing factor
        int threshold;       // below this = 0
        int max_sensor;      // maximum raw reading
        int dead_zone;       // minimal change to trigger
        int output_id;       // optional ID for PD or USB
    };

    extern Button btns[12];
    extern Knob knobs[4];
    extern Led leds[12];
    extern Encoder encoder[4];
    extern Joystick joystick[2];
    extern CNY70 cny70[1];

    extern int n_btn;
    extern int n_knob;
    extern int n_led;
    extern int n_encoder;
    extern int n_joystick;
    extern int n_cny70;

    extern std::atomic<float> led_vals[12];
    extern std::atomic<float> led_hue[12];        
    extern std::atomic<float> led_intensity[12];

    void start_adc();
    void addPin(int index, uint32_t pin, PinMode mode, uint32_t duration = 0);
    void addKnob(int index, uint32_t pin); 
    void addCV(int index, uint32_t pin);   
    void addEncoder(int index, uint32_t pinA, uint32_t pinB);
    void addJoystick(int index, uint32_t pinX, uint32_t pinY);
    void addCNY70(int pin, int threshold, int max_sensor, 
              float alpha, int dead_zone, int output_id);
  
    void processPin(int i, float &outVal, bool &shouldSend);
    bool processKnob(int i, float& outVal);
    bool processEnc(int index, float &val);
    bool processJoystick(int id, float &outX, float &outY, bool &cX, bool &cY, bool midi_range = false);
    bool processCNY70(int i, float &outVal, float &rawOut);

    bool buttonChanged(int i, bool& outState);
    bool buttonPressed(int i);
    bool buttonReleased(int i);
    bool buttonToggled(int i, bool& outState);
    void updateGate(int index, float val);
    void update(uint32_t now);

    void addLed(int index, uint32_t pin);
    void updateLed(int index, float val); 
    void __not_in_flash_func(setLedHardware)(int index, float value);

    #ifdef PICO_ZERO
    void init_neopixel();
    void addRgbLed(int index, uint32_t pin, uint8_t r = 255, uint8_t g = 255, uint8_t b = 255);    
    void set_rgb_led(uint32_t color);
    void updateRGB(int index, float hue, float intensity);
    void showRGB();
    #endif

}

 

    

