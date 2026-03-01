#include "PicoControl.hpp"
#include "hardware/adc.h"
#include "hardware/pwm.h"
#include "hardware/gpio.h"
#include "pico/time.h"
#include <cmath>

namespace Pico {

    Button btns[16];
    Knob knobs[8];
    Led leds[16];
    std::atomic<float> led_vals[16];
    int n_btn = 0, n_knob = 0, n_led = 0;

    void addBtn(int index, uint32_t pin, ButtonMode mode) {
    gpio_init(pin);
    gpio_set_dir(pin, GPIO_IN);
    gpio_pull_up(pin);
    btns[index].pin = pin;
    btns[index].state.store(false);
    btns[index].last = false;
    btns[index].raw_prev = false;
    btns[index].last_time = 0;
    btns[index].toggle_state = false; 
    btns[index].reset_at = 0;         
    btns[index].mode = mode;         
    if (index >= n_btn) n_btn = index + 1; 
}

    void addKnob(int index, uint32_t pin) {
        if (n_knob == 0) adc_init();
        adc_gpio_init(pin);
        knobs[index].adc_ch = pin - 26;
        knobs[index].value.store(0.0f);
        knobs[index].last_val = 0.0f;
        knobs[index].coeff = 0.1f;  // can asign different smoothing for knobs using index
        if (index >= n_knob) n_knob = index + 1;
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

    void update() {
        uint32_t now = to_ms_since_boot(get_absolute_time());
        for (int i = 0; i < n_btn; i++) {
            bool r = !gpio_get(btns[i].pin); 
            if (r != btns[i].raw_prev) { 
                btns[i].last_time = now; 
                btns[i].raw_prev = r; 
            }
            if ((now - btns[i].last_time) > 20) {
                btns[i].state.store(r);
            }
        }
        for (int i = 0; i < n_knob; i++) {
        adc_select_input(knobs[i].adc_ch);
        float raw = (float)adc_read() / 4095.0f;
        float prev = knobs[i].value.load();
        float smoothed = prev + (raw - prev) * knobs[i].coeff;
        knobs[i].value.store(smoothed);
        }
    }
    

    void setLedHardware(int index, float value) {
        pwm_set_chan_level(leds[index].slice, leds[index].chan, (uint16_t)(value * 255.0f));
    }

    void updateLed(int index, float val) {
        if (index < 16) { // or use your n_led variable
            led_vals[index].store(val);
            setLedHardware(index, val);
        }
    }


    bool buttonChanged(int i, bool& outState) {
        bool s = btns[i].state.load();
        if (s != btns[i].last) {
            btns[i].last = s;
            outState = s;
            return true;
        }
        return false;
    }

    bool buttonPressed(int i) {
        bool s = btns[i].state.load();
        if (s && !btns[i].last) { 
            btns[i].last = true;
            return true;
        }
        if (!s) btns[i].last = false; 
        return false;
    }

    bool buttonReleased(int i) {
        bool s = btns[i].state.load();
        if (!s && btns[i].last) { 
            btns[i].last = false;
            return true;
        }
        if (s) btns[i].last = true; 
        return false;
    }

   bool buttonToggled(int i, bool& outState) {
    bool s = btns[i].state.load();
    if (s && !btns[i].last) {
        btns[i].last = true;
        btns[i].toggle_state = !btns[i].toggle_state;
        outState = btns[i].toggle_state;
        return true;
    }
    if (!s) btns[i].last = false;
    return false;
}


void processButton(int i, float &outVal, bool &shouldSend) {
    uint32_t now = to_ms_since_boot(get_absolute_time());
    bool s;
    shouldSend = false;

    switch (btns[i].mode) {
        case BANG:
            if (buttonPressed(i)) {
                outVal = 1.0f;
                shouldSend = true;
                btns[i].reset_at = now + BANG_PULSE_WIDTH_MS;
            } else if (btns[i].reset_at > 0 && now >= btns[i].reset_at) {
                btns[i].reset_at = 0;
                outVal = 0.0f;
                shouldSend = false;
            }
            break;

        case TOGGLE:
            if (buttonToggled(i, s)) {
                outVal = s ? 1.0f : 0.0f;
                shouldSend = true;
            }
            break;

        case SWITCH:
            if (buttonChanged(i, s)) {
                outVal = s ? 1.0f : 0.0f;
                shouldSend = true;
            }
            break;
    }
}

    bool knobChanged(int i, float& outVal) {
        float v = knobs[i].value.load();
        if (std::abs(v - knobs[i].last_val) > 0.005f) {
            knobs[i].last_val = v;
            outVal = v;
            return true;
        }
        return false;
    }
}
