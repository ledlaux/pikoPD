#include "PicoControl.hpp"
#include "hardware/adc.h"
#include "hardware/pwm.h"
#include "hardware/gpio.h"
#include "pico/time.h"
#include <cmath>

namespace Pico {

    Button btns[12];
    Led leds[12];
    Knob knobs[4];
    Encoder encoders[4];

    std::atomic<float> led_vals[12];

    int n_btn = 0;
    int n_knob = 0;
    int n_led = 0;
    int n_encoder = 0; 

  
    void update(uint32_t now) {
        uint32_t all_pins = gpio_get_all(); 
        
        // --- 1. Buttons ---
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

        // --- 2. Encoders (High Priority) ---
        for (int i = 0; i < n_encoder; i++) {
            bool clk = (all_pins & (1u << encoders[i].pinA)) != 0;
            bool dt  = (all_pins & (1u << encoders[i].pinB)) != 0;

            if (clk != encoders[i].last_clk) {
                // Trigger only on Rising Edge to prevent double-values
                if (clk) { 
                    if (clk != dt) {
                        encoders[i].value.fetch_add(1, std::memory_order_relaxed);
                    } else {
                        encoders[i].value.fetch_sub(1, std::memory_order_relaxed);
                    }
                }
                encoders[i].last_clk = clk;
            }
        }

        // --- 3. Knobs ---
        for (int i = 0; i < n_knob; i++) {
            adc_select_input(knobs[i].adc_ch);
            float raw = (float)adc_read() / 4095.0f;
            float prev = knobs[i].value.load(std::memory_order_relaxed);
            knobs[i].value.store(prev + (raw - prev) * knobs[i].coeff, std::memory_order_relaxed); 
        }
    }
   
    void addPin(int index, uint32_t pin, PinMode mode, uint32_t duration) {
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

    void addKnob(int index, uint32_t pin) {
        if (index == 0) adc_init();
        adc_gpio_init(pin);
        knobs[index].adc_ch = pin - 26;
        knobs[index].value.store(0.0f, std::memory_order_relaxed);
        knobs[index].last_val = 0.0f;
        knobs[index].coeff = 0.1f; 
        if (index >= n_knob) n_knob = index + 1;
    }

    void addCV(int index, uint32_t pin) {
        addKnob(index, pin); 
        knobs[index].coeff = 1.0f; 
    }

    void addEncoder(int index, uint32_t pinA, uint32_t pinB) {
    // 1. Hardware Initialization
    gpio_init(pinA);
    gpio_set_dir(pinA, GPIO_IN);
    gpio_pull_up(pinA);

    gpio_init(pinB);
    gpio_set_dir(pinB, GPIO_IN);
    gpio_pull_up(pinB);

    // 2. Store Pins
    encoders[index].pinA = pinA;
    encoders[index].pinB = pinB;
    
    // 3. Capture Initial States (The "Idea" from the library)
    // We store the current physical state so the first 'diff' isn't a lie
    encoders[index].last_clk = gpio_get(pinA);
    encoders[index].last_dt  = gpio_get(pinB);
    
    // 4. Reset Counters
    encoders[index].value.store(0, std::memory_order_relaxed);
    encoders[index].last_sent_count = 0;

    // 5. Update Global Encoder Count
    if (index >= n_encoder) n_encoder = index + 1;
}

    void addLed(int index, uint32_t pin) {
        gpio_set_function(pin, GPIO_FUNC_PWM);
        uint slice = pwm_gpio_to_slice_num(pin);
        uint chan = pwm_gpio_to_channel(pin);
        pwm_set_wrap(slice, 255);
        pwm_set_enabled(slice, true);
        leds[index].pin = pin;
        leds[index].slice = slice;
        leds[index].chan = chan;
        if (index >= n_led) n_led = index + 1;
    }

    void __not_in_flash_func(setLedHardware)(int index, float value) {
        uint16_t level = (uint16_t)(value * value * 255.0f);
        pwm_set_chan_level(leds[index].slice, leds[index].chan, level);
    }

    void updateLed(int index, float val) {
        if (index < 12) {
            led_vals[index].store(val, std::memory_order_relaxed);
            setLedHardware(index, val);
        }
    }

   void updateGate(int index, float val) {
    if (index < 12 && btns[index].mode == Pico::GATE_OUT) {
        int state = (val > 0.5f) ? 1 : 0;
         
        uint32_t duration = btns[index].pulse_duration; 

        switch (state) {
            case 1:  // Trigger
                gpio_put(btns[index].pin, 1);
                btns[index].state.store(true, std::memory_order_relaxed);
                
                if (duration > 0) {
                    uint32_t now = to_ms_since_boot(get_absolute_time());
                    btns[index].reset_at = now + duration;
                }
                break;

            case 0:    // Gate
                if (duration == 0) {
                    gpio_put(btns[index].pin, 0);
                    btns[index].state.store(false, std::memory_order_relaxed);
                }
                break;
        }
    }
}
   

    bool buttonPressed(int i) {
        bool s = btns[i].state.load(std::memory_order_relaxed);
        if (s && !btns[i].last) {
            btns[i].last = true;
            return true;
        }
        if (!s) btns[i].last = false;
        return false;
    }

    bool buttonToggled(int i, bool& outState) {
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

    bool buttonChanged(int i, bool& outState) {
        bool s = btns[i].state.load(std::memory_order_relaxed);
        if (s != btns[i].last) {
            btns[i].last = s;
            outState = s;
            return true;
        }
        return false;
    }

 bool encoderChanged(int index, float &val) {
    int current_count = encoders[index].value.load(std::memory_order_relaxed);
    int diff = current_count - encoders[index].last_sent_count;

    // Use 1 if your update() loop only triggers on the Rising Edge.
    // Use 2 if your update() loop triggers on both edges of the CLK pin.
    if (abs(diff) >= 1) {
        val = (diff > 0) ? 1.0f : -1.0f;
        
        // Update tracker to match the threshold
        encoders[index].last_sent_count = current_count;
        return true;
    }
    return false;
}
    void processPin(int i, float &outVal, bool &shouldSend) {
        uint32_t now = to_ms_since_boot(get_absolute_time());
        bool s;
        shouldSend = false;

        switch (btns[i].mode) {
            case BANG:
                if (buttonPressed(i)) {
                    outVal = 1.0f;
                    shouldSend = true;
                    btns[i].reset_at = now + 10;  // reset after ms
                } 
                
                if (btns[i].reset_at > 0 && now >= btns[i].reset_at) {
                    btns[i].reset_at = 0; 
                    outVal = 0.0f;
                    shouldSend = true;    
                }
                break;

            case TOGGLE:
                if (buttonToggled(i, s)) {
                    outVal = s ? 1.0f : 0.0f;
                    shouldSend = true;
                }
                break;

            case SWITCH:
            case GATE_IN:
                if (buttonChanged(i, s)) {
                    outVal = s ? 1.0f : 0.0f;
                    shouldSend = true;
                }
                break;

            case GATE_OUT:
                if (buttonChanged(i, s)) {
                    outVal = s ? 1.0f : 0.0f;
                    shouldSend = true;
                    gpio_put(btns[i].pin, s); 
                }
                break;
        }
    }

    bool knobChanged(int i, float& outVal) {
        float v = knobs[i].value.load(std::memory_order_relaxed);
        if (std::abs(v - knobs[i].last_val) > 0.005f) {
            knobs[i].last_val = v;
            outVal = v;
            return true;
        }
        return false;
    }


}