#pragma once

#include <cstdint>
#include <atomic>
#include <map>
#include <vector>
#include <string>
#include "hardware/gpio.h"

namespace Pico {

struct Led {
    std::string name;
    uint32_t pin = 255;
    uint slice = 0;
    uint chan = 0;
    bool is_pwm = true;
};

struct Button {
    uint32_t pin = 255;
    bool state = false;
    bool last = false;
    uint32_t last_debounce_time = 0;
    bool last_raw_state = false;
};

struct Pot { 
    int adc = -1; 
    float value = 0.0f; 
};

struct Encoder { 
    uint32_t a = 255;
    uint32_t b = 255;
    int lastState = 0; 
    int delta = 0; 
};

// ----------------------------------------
// Globals
// ----------------------------------------
extern std::map<std::string, std::atomic<float>> hvAtomicMap;
extern std::vector<Button> buttons;
extern std::vector<Pot> pots;
extern std::vector<Encoder> encoders;
extern std::vector<Led> leds;
extern std::map<std::string, size_t> buttonIndexMap;

// ----------------------------------------
// Initialization
// ----------------------------------------
void init();

// ----------------------------------------
// Button 
// ----------------------------------------
void buttonInit(const std::string &name, uint32_t pin, bool pullup); 
bool button(int id);              
bool buttonPressed(int id);        
bool buttonReleased(int id); 

inline int buttonIndex(const std::string &name) {
    if (name == "BTN") return 0;
    if (name.rfind("BTN", 0) == 0) {
        return std::stoi(name.substr(3));
    }
    return -1;
}

inline int ledIndex(const std::string &name) {
    if (name == "LED") return 0;
    if (name.rfind("LED", 0) == 0) { 
        return std::stoi(name.substr(3));
    }
    return -1;
}

// ----------------------------------------
// Pot 
// ----------------------------------------
void potInit(const std::string &name, uint32_t pin);
float pot(int id);

// ----------------------------------------
// Encoder 
// ----------------------------------------
void encoderInit(const std::string &name, uint32_t pinA, uint32_t pinB);
int encoder(int id);

// ----------------------------------------
// LED 
// ----------------------------------------
void ledInit(const std::string &name, uint32_t pin);
inline void ledInit(int idx, const std::string &name, uint32_t pin) {
    (void)idx;          // index unused but keeps API uniform
    ledInit(name, pin);
}
void led(const std::string &name, float value);
inline void led(int idx, float value) {
    if (idx < 0 || idx >= (int)leds.size()) return;  
    led(leds[idx].name, value);                      
}

// ----------------------------------------
// Atomic access (thread-safe)
// ----------------------------------------
std::atomic<float>& getAtomic(const std::string &name);

void update();

} // namespace Pico
