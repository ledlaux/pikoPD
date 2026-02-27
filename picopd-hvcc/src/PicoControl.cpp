#include "PicoControl.hpp"
#include <algorithm>
#include "hardware/adc.h"
#include "hardware/pwm.h"
#include "hardware/irq.h"
#include "hardware/gpio.h"
#include "pico/time.h"

namespace Pico {

// ----------------------------------------
// Internal Extended Structure for Debouncing
// ----------------------------------------
struct ButtonInternal : Button {
    uint32_t last_debounce_time = 0;
    bool last_raw_state = false;
};

// ----------------------------------------
// Globals
// ----------------------------------------
std::map<std::string, std::atomic<float>> hvAtomicMap;
std::vector<Button> buttons; // Changed to Internal type
std::vector<Pot> pots;
std::vector<Encoder> encoders;
std::vector<Led> leds;
std::map<std::string, size_t> buttonIndexMap;

// ----------------------------------------
// Init
// ----------------------------------------
void init() {
    buttons.reserve(16);
    pots.reserve(8);
    encoders.reserve(8);
    leds.reserve(16);
    adc_init();
}

// ----------------------------------------
// BUTTONS
// ----------------------------------------
void buttonInit(const std::string &name, uint32_t pin, bool pullup) {
    gpio_init(pin);
    gpio_set_dir(pin, GPIO_IN);
    if (pullup) gpio_pull_up(pin);
    else gpio_pull_down(pin);

    ButtonInternal b;
    b.pin   = pin;
    b.last_raw_state = !gpio_get(pin);
    b.state = b.last_raw_state;
    b.last  = b.state;
    b.last_debounce_time = 0;
    
    buttons.push_back(b);
    buttonIndexMap[name] = buttons.size() - 1;
    hvAtomicMap[name] = 0.0f;
}

bool button(int id) {
    return (id >= 0 && id < (int)buttons.size()) ? buttons[id].state : false;
}

// ID-based pressed helper for main loop
bool buttonPressed(int id) {
    if (id < 0 || id >= (int)buttons.size()) return false;
    return buttons[id].state && !buttons[id].last;
}

// String-based pressed helper (overload)
bool buttonPressed(const std::string &name) {
    int id = buttonIndex(name);
    return buttonPressed(id);
}

bool buttonReleased(int id) {
    if (id < 0 || id >= (int)buttons.size()) return false;
    return !buttons[id].state && buttons[id].last;
}

// ----------------------------------------
// POTS
// ----------------------------------------
void potInit(const std::string &name, uint32_t pin) {
    adc_gpio_init(pin);
    Pot p{};
    p.adc = pin - 26; 
    p.value = 0.0f;
    pots.push_back(p);
    hvAtomicMap[name] = 0.0f;
}

float pot(int id) {
    return (id >= 0 && id < (int)pots.size()) ? pots[id].value : 0.0f;
}

// ----------------------------------------
// ENCODERS
// ----------------------------------------
void encoderInit(const std::string &name, uint32_t pinA, uint32_t pinB) {
    gpio_init(pinA); gpio_set_dir(pinA, GPIO_IN); gpio_pull_up(pinA);
    gpio_init(pinB); gpio_set_dir(pinB, GPIO_IN); gpio_pull_up(pinB);

    Encoder e{};
    e.a = pinA;
    e.b = pinB;
    e.lastState = (gpio_get(pinA) << 1) | gpio_get(pinB);
    e.delta = 0;
    encoders.push_back(e);
    hvAtomicMap[name] = 0.0f;
}

int encoder(int id) {
    return (id >= 0 && id < (int)encoders.size()) ? encoders[id].delta : 0;
}

// ----------------------------------------
// LEDS
// ----------------------------------------
void ledInit(const std::string &name, uint32_t pin) {
    gpio_init(pin);
    gpio_set_dir(pin, GPIO_OUT);
    gpio_put(pin, 0);

    Led l{};
    l.name = name;
    l.pin  = pin;
    l.is_pwm = (pin != 25); 
    leds.push_back(l);

    hvAtomicMap[name] = 0.0f;

    if (l.is_pwm) {
        gpio_set_function(pin, GPIO_FUNC_PWM);
        uint slice = pwm_gpio_to_slice_num(pin);
        pwm_set_wrap(slice, 255);
        pwm_set_chan_level(slice, pwm_gpio_to_channel(pin), 0);
        pwm_set_enabled(slice, true);
    }
}

void led(const std::string &name, float value) {
    for (auto &l : leds) {
        if (l.name == name) {
            float val = std::clamp(value, 0.0f, 1.0f);
            if (l.is_pwm) {
                pwm_set_gpio_level(l.pin, (uint16_t)(val * 255.0f));
            } else {
                gpio_put(l.pin, val > 0.5f);
            }
            break;
        }
    }
}

// ----------------------------------------
// Atomic access
// ----------------------------------------
std::atomic<float>& getAtomic(const std::string &name) {
    return hvAtomicMap[name];
}

// ----------------------------------------
// UPDATE ALL CONTROLS
// ----------------------------------------
void update() {
    uint32_t now = to_ms_since_boot(get_absolute_time());

    // 1. Debounced Buttons
    for (auto &b : buttons) {
        bool raw = !gpio_get(b.pin);
        if (raw != b.last_raw_state) {
            b.last_debounce_time = now;
        }
        if ((now - b.last_debounce_time) > 20) {
            b.last = b.state;
            b.state = raw;
        }
        b.last_raw_state = raw;
    }

    // 2. Pots with Smoothing (LPF)
    for (auto &p : pots) {
        if (p.adc < 0) continue;
        adc_select_input(p.adc);
        float raw = static_cast<float>(adc_read()) / 4095.0f;
        p.value = (p.value * 0.9f) + (raw * 0.1f);
    }

    // 3. Encoders
    for (auto &e : encoders) {
        int state = (gpio_get(e.a) << 1) | gpio_get(e.b);
        if (state != e.lastState) {
            if ((e.lastState == 0 && state == 1) || (e.lastState == 1 && state == 3) || 
                (e.lastState == 3 && state == 2) || (e.lastState == 2 && state == 0)) {
                e.delta++;
            } else if ((e.lastState == 0 && state == 2) || (e.lastState == 2 && state == 3) || 
                       (e.lastState == 3 && state == 1) || (e.lastState == 1 && state == 0)) {
                e.delta--;
            }
            e.lastState = state;
        }
    }
}

} // namespace Pico