#include "PicoControl.h"
#include "pico/stdlib.h"      
#include "hardware/gpio.h"
#include "hardware/adc.h"
#include "hardware/pwm.h"
#include "hardware/pio.h"     
#include "pico/time.h"       
#include <atomic>
#include <cmath>
#include <cstdio>            

#ifdef PICO_ZERO
#include "ws2812.pio.h" 
#endif


namespace Pico {

    
    std::atomic<float> led_vals[12];
    std::atomic<float> led_hue[12];        
    std::atomic<float> led_intensity[12];
    static uint32_t led_framebuffer[12] = {0};
    static float smooth_hue[12] = {0.0f};

    const float release_coeff = 0.0005f; // Smaller = Slower release

    static bool adc_initialized = false;

    inline void start_adc() {
        if (!adc_initialized) {
            adc_init();
            adc_initialized = true;
            }
        }
    


    Button btns[12];
    Led leds[12];
    Knob knobs[4];
    Encoder encoder[4];
    Joystick joystick[2];
    CNY70 cny70[1];

    int n_btn = 0;
    int n_knob = 0;
    int n_led = 0;
    int n_encoder = 0; 
    int n_joystick = 0;
    int n_cny70 = 0;

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
        start_adc();
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


    void addLed(int index, uint32_t pin) {
        gpio_set_function(pin, GPIO_FUNC_PWM);
        uint slice = pwm_gpio_to_slice_num(pin);
        uint chan = pwm_gpio_to_channel(pin);
        pwm_set_wrap(slice, 255);
        pwm_set_enabled(slice, true);
        
        leds[index].pin = pin;
        leds[index].slice = slice;
        leds[index].chan = chan;
        leds[index].is_rgb = false;
        
        if (index >= n_led) n_led = index + 1;
    }


    void addJoystick(int index, uint32_t pinX, uint32_t pinY) {
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


    void addCNY70(int pin, int threshold, int max_sensor, float alpha, int dead_zone, int output_id) {
     
        start_adc();
        adc_gpio_init(pin);

        auto &s = cny70[0];
        s.adc_ch = pin - 26;
        s.threshold = threshold;
        s.max_sensor = max_sensor;
        s.alpha = alpha;
        s.dead_zone = dead_zone;
        s.output_id = output_id;
        s.last_val = -1.0f; // Flag for first-run init
        s.smooth_value = 0.0f;

        n_cny70++;
    }


    void update(uint32_t now) {
        uint32_t all_pins = gpio_get_all(); 
            
        // --- Buttons ---
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
                    } else if ((now - btns[i].last_time) > 20) {  // debounce 20 ms
                        btns[i].state.store(r, std::memory_order_relaxed);
                    }
                }
        }

        // --- Encoders ---
        for (int i = 0; i < n_encoder; i++) {
            bool clk = (all_pins & (1u << encoder[i].pinA)) != 0;
            bool dt  = (all_pins & (1u << encoder[i].pinB)) != 0;
    
            if (clk != encoder[i].last_clk) {
                if (clk) { 
                    if (clk != dt) {
                        encoder[i].value.fetch_sub(1, std::memory_order_relaxed);
                    } else {
                        encoder[i].value.fetch_add(1, std::memory_order_relaxed);
                    }
                }
                encoder[i].last_clk = clk;
            }
        }
    
        // --- Knobs ---
        for (int i = 0; i < n_knob; i++) {
            adc_select_input(knobs[i].adc_ch);
            float raw = (float)adc_read() / 4095.0f;
            float prev = knobs[i].value.load(std::memory_order_relaxed);
            
            if (fabsf(raw - prev) > 0.001f) {
                float next_val = prev + (raw - prev) * knobs[i].coeff;
                knobs[i].value.store(next_val, std::memory_order_relaxed);
            }
        }

        // --- Joystic ---

        for (int i = 0; i < n_joystick; i++) {
            adc_select_input(joystick[i].adcX);
            float rawX = (float)adc_read();
            adc_select_input(joystick[i].adcY);
            float rawY = (float)adc_read();

            if (fabsf(rawX - joystick[i].smoothX) > 1.0f) {
                joystick[i].smoothX += (rawX - joystick[i].smoothX) * 0.1f; 
            }
            if (fabsf(rawY - joystick[i].smoothY) > 1.0f) {
                joystick[i].smoothY += (rawY - joystick[i].smoothY) * 0.1f;
            }

            int16_t dx = (int16_t)joystick[i].centerX - (int16_t)joystick[i].smoothX;
            int16_t dy = (int16_t)joystick[i].centerY - (int16_t)joystick[i].smoothY;

            joystick[i].x.store((abs(dx) > 60) ? dx : 0, std::memory_order_relaxed);
            joystick[i].y.store((abs(dy) > 60) ? dy : 0, std::memory_order_relaxed);
        }
    }   


    void updateGate(int index, float val) {
        if (index < 12 && btns[index].mode == GATE_OUT) {
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


    bool processEnc(int index, float &val) {
        int current_count = encoder[index].value.load(std::memory_order_relaxed);
        int diff = current_count - encoder[index].last_sent_count;

        if (abs(diff) >= 1) {
            val = (diff > 0) ? 1.0f : -1.0f;
            encoder[index].last_sent_count = current_count;
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


    bool processKnob(int i, float& outVal) {
        float v = knobs[i].value.load(std::memory_order_relaxed);
        if (std::abs(v - knobs[i].last_val) > 0.005f) {
            knobs[i].last_val = v;
            outVal = v;
            return true;
        }
        return false;
    }


   bool processJoystick(int id, float &outX, float &outY, bool &cX, bool &cY, bool midi_range) {
        if (id >= n_joystick) return false;

        float newX = (float)joystick[id].x.load() / 2048.0f;
        float newY = (float)joystick[id].y.load() / 2048.0f;

        if (newX > 1.0f) newX = 1.0f; if (newX < -1.0f) newX = -1.0f;
        if (newY > 1.0f) newY = 1.0f; if (newY < -1.0f) newY = -1.0f;

        if (midi_range) {
            float midiNormX = (newX + 1.0f) * 0.5f;
            float midiNormY = (newY + 1.0f) * 0.5f;
            newX = (float)((int)(midiNormX * 126.0f) + 1);
            newY = (float)((int)(midiNormY * 126.0f) + 1);
        }

        float threshold = midi_range ? 2.0f : 0.05f; 

        cX = (std::abs(newX - joystick[id].lastSentX) > threshold);
        cY = (std::abs(newY - joystick[id].lastSentY) > threshold);

        if (cX) { 
            outX = newX; 
            joystick[id].lastSentX = newX; 
        }
        if (cY) { 
            outY = newY; 
            joystick[id].lastSentY = newY; 
        }

        return (cX || cY);
    }


#ifdef PICO_ZERO

    void init_neopixel() {
        static bool initialized = false;
        if (initialized) return;

        PIO pio = pio1;
        int sm = 0;
        if (!pio_can_add_program(pio, &ws2812_program)) return;
            
        uint offset = pio_add_program(pio, &ws2812_program);
        ws2812_program_init(pio, sm, offset, 16, 800000, false); 

        pio_sm_set_enabled(pio, sm, true);
            
        initialized = true;
    }


    void addRgbLed(int index, uint32_t pin, uint8_t r, uint8_t g, uint8_t b) {
        init_neopixel(); 
        leds[index].pin = pin;
        leds[index].is_rgb = true;
        leds[index].r = r;
        leds[index].g = g;
        leds[index].b = b;

        if (index >= n_led) n_led = index + 1;
    }


    void updateRGB(int index, float hue, float intensity) {
        if (index < 0 || index >= 12) return;
        float diff = hue - smooth_hue[index];
        if (diff > 0.5f) diff -= 1.0f;
        if (diff < -0.5f) diff += 1.0f;
        smooth_hue[index] += diff * 0.15f; 

        // Keep hue in 0..1 range
        if (smooth_hue[index] >= 1.0f) smooth_hue[index] -= 1.0f;
        if (smooth_hue[index] < 0.0f) smooth_hue[index] += 1.0f;

        // 2. HSV to RGB Math
        float r = 0, g = 0, b = 0;
        float h = smooth_hue[index] * 6.0f;
        int i = (int)h;
        float f = h - i;
        float q = 1.0f - f;

        switch (i % 6) {
            case 0: r = 1.0f; g = f;    b = 0.0f; break;
            case 1: r = q;    g = 1.0f; b = 0.0f; break;
            case 2: r = 0.0f; g = 1.0f; b = f;    break;
            case 3: r = 0.0f; g = q;    b = 1.0f; break;
            case 4: r = f;    g = 0.0f; b = 1.0f; break;
            case 5: r = 1.0f; g = 0.0f; b = q;    break;
        }

        // 3. Gamma & Intensity
        float gamma = intensity * intensity;
        uint8_t uR = (uint8_t)(r * gamma * 255.0f);
        uint8_t uG = (uint8_t)(g * gamma * 255.0f);
        uint8_t uB = (uint8_t)(b * gamma * 255.0f);

        led_framebuffer[index] = ((uint32_t)(uG) << 16) | ((uint32_t)(uR) << 8) | ((uint32_t)(uB));
    }


   void showRGB() {
        if (pio_sm_get_tx_fifo_level(pio1, 0) > 4) return;
        pio1->txf[0] = led_framebuffer[0];
        pio1->txf[0] = 0;
        pio1->txf[0] = 0;
        pio1->txf[0] = 0;
    }

#endif

    void __not_in_flash_func(setLedHardware)(int index, float value) {
        if (index >= 12) return;
        float gamma = value * value;
        if (leds[index].is_rgb) return; 
        uint16_t level = (uint16_t)(gamma * 255.0f);
        pwm_set_chan_level(leds[index].slice, leds[index].chan, level);
    }


    void updateLed(int index, float val) {
        if (index < 12) {
            led_vals[index].store(val, std::memory_order_relaxed);
            if (!leds[index].is_rgb) {
                setLedHardware(index, val);
            }
        }
        }


    bool processCNY70(int i, float &outVal, float &rawOut) {
        if (i < 0 || i >= n_cny70) return false;
        auto &s = cny70[i];

        adc_select_input(s.adc_ch);
        
        uint32_t sum = 0;
        for(int j = 0; j < 16; j++) {
            sum += adc_read();
        }
        
        float raw10 = (float)sum / 64.0f; 
        rawOut = raw10; 

        float current_norm = 0.0f;
        
        // SAFETY: If sensor is below threshold (e.g. 400.3 vs 450), 
        // force it to exactly 0.0 to prevent crashing PD.
        if (raw10 > (float)s.threshold) {
            float range = (float)s.max_sensor - (float)s.threshold;
            if (range < 1.0f) range = 1.0f;
            current_norm = (raw10 - (float)s.threshold) / range;
        } else {
            current_norm = 0.0f; 
        }
        
        if (current_norm > 1.0f) current_norm = 1.0f;

        // Smoothing
        if (s.last_val < -0.5f) {
            s.smooth_value = current_norm;
            s.last_val = 0.0f;
        } else {
            s.smooth_value += (current_norm - s.smooth_value) * s.alpha;
        }

        // Deadzone check
        float dz = (float)s.dead_zone * 0.001f;
        if (fabsf(s.smooth_value - s.last_val) > dz || s.smooth_value == 0.0f || s.smooth_value == 1.0f) {
            s.last_val = s.smooth_value;
            outVal = s.smooth_value;
            return true;
        }

        return false;
    }


}

