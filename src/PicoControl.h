#ifndef PICO_CONTROL_HPP
#define PICO_CONTROL_HPP

#include "pico/stdlib.h"
#include <atomic>
#include <cmath>

#ifdef MIDI_HOST
    #ifndef CFG_TUH_ENABLED
        #define CFG_TUH_ENABLED 1
    #endif
    #ifndef CFG_TUH_MIDI
        #define CFG_TUH_MIDI 1
    #endif
    #ifndef BOARD_TUH_RHPORT
        #define BOARD_TUH_RHPORT 0
    #endif

    #include "tusb_config.h"
    #include "tusb.h"
    #include "host/usbh.h"
    #include "class/midi/midi_host.h"
#else
    #include "tusb_config.h"
    #include "tusb.h"
    #include "cdc_stdio_lib.h" 
#endif



#define MIDI_RB_SIZE 1024

struct MidiBuffer {
    uint8_t data[MIDI_RB_SIZE];
    std::atomic<uint32_t> head{0};
    std::atomic<uint32_t> tail{0};

    uint32_t available() const {
        return head.load() - tail.load();
    }

    bool is_full() const {
        return available() >= MIDI_RB_SIZE;
    }
};


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

    extern Button btns[12];
    extern Knob knobs[4];
    extern Led leds[12];
    extern Encoder encoder[4];
    extern Joystick joystick[2];

    extern std::atomic<float> led_vals[12];
    
    extern int n_btn;
    extern int n_knob;
    extern int n_led;
    extern int n_encoder;
    extern int n_joystick;

    extern MidiBuffer midi_rb;

    void addPin(int index, uint32_t pin, PinMode mode, uint32_t duration = 0);
    void addKnob(int index, uint32_t pin); 
    void addCV(int index, uint32_t pin);   
    void addEncoder(int index, uint32_t pinA, uint32_t pinB);
    void addJoystick(int index, uint32_t pinX, uint32_t pinY);
    void addLed(int index, uint32_t pin);
    void updateLed(int index, float val); 

    void updateGate(int index, float val);
    void processPin(int i, float &outVal, bool &shouldSend);
    bool processKnob(int i, float& outVal);
    bool processEnc(int index, float &val);
    bool processJoystick(int id, float &outX, float &outY, bool &cX, bool &cY, bool midi_range = false);
    bool buttonChanged(int i, bool& outState);
    bool buttonPressed(int i);
    bool buttonReleased(int i);
    bool buttonToggled(int i, bool& outState);
    void __not_in_flash_func(setLedHardware)(int index, float value);
#ifdef PICO_ZERO
    void init_neopixel();
    void addRgbLed(int index, uint32_t pin, uint8_t r = 255, uint8_t g = 255, uint8_t b = 255);    
    void set_rgb_led(uint32_t color);
    uint32_t urgb_u32(uint8_t r, uint8_t g, uint8_t b);
    void updateRGB(int index, float hue, float intensity);
#endif
    void update(uint32_t now);

    enum AudioMode { I2S, PWM };
    typedef void (*AudioProcessCallback)(float* buffer, int frames);

    void setupAudio(AudioMode mode, AudioProcessCallback callback, 
                    int sample_rate, uint data_pin, uint bclk_pin, int buffer_size);

    void __not_in_flash_func(core1_audio_entry)();


    void midi_push(uint8_t byte);
    bool midi_pop(uint8_t &byte);
    void parse_raw_midi_byte(uint8_t byte, void (*handler)(uint8_t, uint8_t, uint8_t));
    void usb_init();
    void uart_midi_init();
    void midi_task();
    void midi_task_uart();
}

#endif