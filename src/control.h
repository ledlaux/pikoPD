#pragma once

#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"
#include "hardware/i2c.h"
#include "led.h"
#include "pico/time.h"
#include <atomic>
#include <cmath>
#include <cstdio>

#if PICO_ZERO
#include "ws2812.pio.h" 
#endif

#if MPR121_ENABLED
#include "mpr121.h"
#endif

#if DISTANCE_SENSOR_ENABLED
#include "hc-sr04.h"
#endif

#if HX710_ENABLED
#include "hx710.h"
#endif

#if CNY70_ENABLED
#include "cny70.h"
#endif

#include "mcp4725.h" 


namespace Pico {

    // --- Enums and Structs ---

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

    // --- State Storage ---

    inline Button btns[12];
    inline Knob knobs[4];
    inline Encoder encoder[4];
    inline Joystick joystick[2];
 
    inline int n_btn = 0;
    inline int n_knob = 0;
    inline int n_encoder = 0;
    inline int n_joystick = 0;

    inline bool adc_initialized = false;

    inline void start_adc() {
        if (!adc_initialized) {
            adc_init();
            adc_initialized = true;
        }
    }

    inline void addPin(int index, uint32_t pin, PinMode mode, uint32_t duration = 0) {
        gpio_init(pin);
        btns[index].pin = pin;
        btns[index].mode = mode;
        btns[index].mask = (1u << pin);
        btns[index].pulse_duration = duration;

        if (mode == GATE_OUT) {
            gpio_set_dir(pin, GPIO_OUT);
            gpio_put(pin, 0);
            btns[index].state.store(false, std::memory_order_relaxed);
            btns[index].last = false;
        } else {
            gpio_set_dir(pin, GPIO_IN);
            gpio_pull_up(pin);
            bool is_pressed = !gpio_get(pin);
            btns[index].state.store(is_pressed, std::memory_order_relaxed);
            btns[index].last = is_pressed;
            btns[index].raw_prev = is_pressed;
        }
        btns[index].last_time = 0;
        btns[index].toggle_state = false;
        btns[index].reset_at = 0;
        if (index >= n_btn) n_btn = index + 1;
    }

    inline void addKnob(int index, uint32_t pin) {
        start_adc();
        adc_gpio_init(pin);
        knobs[index].adc_ch = pin - 26;
        knobs[index].value.store(0.0f, std::memory_order_relaxed);
        knobs[index].last_val = 0.0f;
        knobs[index].coeff = 0.1f; 
        if (index >= n_knob) n_knob = index + 1;
    }

    inline void addCV(int index, uint32_t pin) {
        addKnob(index, pin); 
        knobs[index].coeff = 1.0f; 
    }

    inline void addEncoder(int index, uint32_t pinA, uint32_t pinB) {
        gpio_init(pinA);
        gpio_set_dir(pinA, GPIO_IN);
        gpio_pull_up(pinA);
        gpio_init(pinB);
        gpio_set_dir(pinB, GPIO_IN);
        gpio_pull_up(pinB);

        encoder[index].pinA = pinA;
        encoder[index].pinB = pinB;
        encoder[index].last_clk = gpio_get(pinA);
        encoder[index].last_dt  = gpio_get(pinB);
        encoder[index].value.store(0, std::memory_order_relaxed);
        encoder[index].last_sent_count = 0;
        if (index >= n_encoder) n_encoder = index + 1;
    }

    inline void addJoystick(int index, uint32_t pinX, uint32_t pinY) {
        start_adc();
        adc_gpio_init(pinX);
        adc_gpio_init(pinY);
        joystick[index].adcX = pinX - 26;
        joystick[index].adcY = pinY - 26;
        adc_select_input(joystick[index].adcX);
        joystick[index].centerX = adc_read();
        adc_select_input(joystick[index].adcY);
        joystick[index].centerY = adc_read();
        joystick[index].smoothX = (float)joystick[index].centerX;
        joystick[index].smoothY = (float)joystick[index].centerY;
        joystick[index].x.store(0, std::memory_order_relaxed);
        joystick[index].y.store(0, std::memory_order_relaxed);
        if (index >= n_joystick) n_joystick = index + 1;
    }

    inline void update(uint32_t now) {
        uint32_t all_pins = gpio_get_all(); 
        for (int i = 0; i < n_btn; i++) {
            if (btns[i].mode == GATE_OUT) {
                if (btns[i].reset_at > 0 && now >= btns[i].reset_at) {
                    gpio_put(btns[i].pin, 0);
                    btns[i].state.store(false, std::memory_order_relaxed);
                    btns[i].reset_at = 0; 
                }
                continue; 
            }
            bool r = (all_pins & btns[i].mask) == 0;
            if (btns[i].mode == GATE_IN) {
                btns[i].state.store(r, std::memory_order_relaxed);
            } else {
                if (r != btns[i].raw_prev) {
                    btns[i].last_time = now;
                    btns[i].raw_prev = r;
                } else if ((now - btns[i].last_time) > 20) {
                    btns[i].state.store(r, std::memory_order_relaxed);
                }
            }
        }
        for (int i = 0; i < n_encoder; i++) {
            bool clk = (all_pins & (1u << encoder[i].pinA)) != 0;
            bool dt  = (all_pins & (1u << encoder[i].pinB)) != 0;
            if (clk != encoder[i].last_clk) {
                if (clk) { 
                    if (clk != dt) encoder[i].value.fetch_sub(1, std::memory_order_relaxed);
                    else encoder[i].value.fetch_add(1, std::memory_order_relaxed);
                }
                encoder[i].last_clk = clk;
            }
        }
        for (int i = 0; i < n_knob; i++) {
            adc_select_input(knobs[i].adc_ch);
            float raw = (float)adc_read() / 4095.0f;
            float prev = knobs[i].value.load(std::memory_order_relaxed);
            if (fabsf(raw - prev) > 0.001f) {
                float next_val = prev + (raw - prev) * knobs[i].coeff;
                knobs[i].value.store(next_val, std::memory_order_relaxed);
            }
        }
        for (int i = 0; i < n_joystick; i++) {
            adc_select_input(joystick[i].adcX);
            float rawX = (float)adc_read();
            adc_select_input(joystick[i].adcY);
            float rawY = (float)adc_read();
            if (fabsf(rawX - joystick[i].smoothX) > 1.0f) joystick[i].smoothX += (rawX - joystick[i].smoothX) * 0.1f; 
            if (fabsf(rawY - joystick[i].smoothY) > 1.0f) joystick[i].smoothY += (rawY - joystick[i].smoothY) * 0.1f;
            int16_t dx = (int16_t)joystick[i].centerX - (int16_t)joystick[i].smoothX;
            int16_t dy = (int16_t)joystick[i].centerY - (int16_t)joystick[i].smoothY;
            joystick[i].x.store((abs(dx) > 60) ? dx : 0, std::memory_order_relaxed);
            joystick[i].y.store((abs(dy) > 60) ? dy : 0, std::memory_order_relaxed);
        }
    }   

    inline void updateGate(int index, float val) {
        if (index < 12 && btns[index].mode == GATE_OUT) {
            int state = (val > 0.5f) ? 1 : 0;
            uint32_t duration = btns[index].pulse_duration; 
            if (state == 1) {
                gpio_put(btns[index].pin, 1);
                btns[index].state.store(true, std::memory_order_relaxed);
                if (duration > 0) btns[index].reset_at = to_ms_since_boot(get_absolute_time()) + duration;
            } else if (duration == 0) {
                gpio_put(btns[index].pin, 0);
                btns[index].state.store(false, std::memory_order_relaxed);
            }
        }
    }
   
    inline bool buttonPressed(int i) {
        bool s = btns[i].state.load(std::memory_order_relaxed);
        if (s && !btns[i].last) { btns[i].last = true; return true; }
        if (!s) btns[i].last = false;
        return false;
    }

    inline bool buttonToggled(int i, bool& outState) {
        bool s = btns[i].state.load(std::memory_order_relaxed);
        if (s && !btns[i].last) {
            btns[i].last = true;
            btns[i].toggle_state = !btns[i].toggle_state;
            outState = btns[i].toggle_state;
            return true;
        }
        if (!s) btns[i].last = false;
        return false;
    }

    inline bool buttonChanged(int i, bool& outState) {
        bool s = btns[i].state.load(std::memory_order_relaxed);
        if (s != btns[i].last) { btns[i].last = s; outState = s; return true; }
        return false;
    }

    inline bool processEnc(int index, float &val) {
        int current_count = encoder[index].value.load(std::memory_order_relaxed);
        int diff = current_count - encoder[index].last_sent_count;
        if (abs(diff) >= 1) {
            val = (diff > 0) ? 1.0f : -1.0f;
            encoder[index].last_sent_count = current_count;
            return true;
        }
        return false;
    }

    inline void processPin(int i, float &outVal, bool &shouldSend) {
        uint32_t now = to_ms_since_boot(get_absolute_time());
        bool s; shouldSend = false;
        switch (btns[i].mode) {
            case BANG:
                if (buttonPressed(i)) { outVal = 1.0f; shouldSend = true; btns[i].reset_at = now + 10; } 
                if (btns[i].reset_at > 0 && now >= btns[i].reset_at) { btns[i].reset_at = 0; outVal = 0.0f; shouldSend = true; }
                break;
            case TOGGLE: if (buttonToggled(i, s)) { outVal = s ? 1.0f : 0.0f; shouldSend = true; } break;
            case SWITCH:
            case GATE_IN: if (buttonChanged(i, s)) { outVal = s ? 1.0f : 0.0f; shouldSend = true; } break;
            case GATE_OUT: if (buttonChanged(i, s)) { outVal = s ? 1.0f : 0.0f; shouldSend = true; gpio_put(btns[i].pin, s); } break;
        }
    }

    inline bool processKnob(int i, float& outVal) {
        float v = knobs[i].value.load(std::memory_order_relaxed);
        if (std::abs(v - knobs[i].last_val) > 0.005f) { knobs[i].last_val = v; outVal = v; return true; }
        return false;
    }

    inline bool processJoystick(int id, float &outX, float &outY, bool &cX, bool &cY, bool midi_range = false) {
        if (id >= n_joystick) return false;
        float newX = fmaxf(-1.0f, fminf(1.0f, (float)joystick[id].x.load() / 2048.0f));
        float newY = fmaxf(-1.0f, fminf(1.0f, (float)joystick[id].y.load() / 2048.0f));
        if (midi_range) {
            newX = (float)((int)((newX + 1.0f) * 0.5f * 126.0f) + 1);
            newY = (float)((int)((newY + 1.0f) * 0.5f * 126.0f) + 1);
        }
        float threshold = midi_range ? 2.0f : 0.05f; 
        cX = (std::abs(newX - joystick[id].lastSentX) > threshold);
        cY = (std::abs(newY - joystick[id].lastSentY) > threshold);
        if (cX) { outX = newX; joystick[id].lastSentX = newX; }
        if (cY) { outY = newY; joystick[id].lastSentY = newY; }
        return (cX || cY);
    }
}

