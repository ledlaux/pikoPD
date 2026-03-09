#include "PicoControl.h"
#include "hardware/dma.h"
#include "hardware/adc.h"
#include "hardware/pwm.h"
#include "hardware/gpio.h"
#include "pico/audio_i2s.h"
#include "pico/multicore.h"
#include "pico/audio_pwm.h"
#include "pico/time.h"
#include "hardware/pio.h"
#include <cmath>

#ifdef PICO_ZERO
#include "ws2812.pio.h" 
#endif

namespace Pico {

    std::atomic<float> led_vals[12];

    Button btns[12];
    Led leds[12];
    Knob knobs[4];
    Encoder encoders[4];
    Joystick joystick[2];

    int n_btn = 0;
    int n_knob = 0;
    int n_led = 0;
    int n_encoder = 0; 
    int n_joystick = 0;

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
        static bool adc_initialized = false;
        if (!adc_initialized) {
            adc_init();
            adc_initialized = true;
        }

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

        encoders[index].pinA = pinA;
        encoders[index].pinB = pinB;
        
        encoders[index].last_clk = gpio_get(pinA);
        encoders[index].last_dt  = gpio_get(pinB);
        
        encoders[index].value.store(0, std::memory_order_relaxed);
        encoders[index].last_sent_count = 0;

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
        if (n_knob == 0 && n_joystick == 0) adc_init();
        
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
            bool clk = (all_pins & (1u << encoders[i].pinA)) != 0;
            bool dt  = (all_pins & (1u << encoders[i].pinB)) != 0;
    
            if (clk != encoders[i].last_clk) {
                if (clk) { 
                    if (clk != dt) {
                        encoders[i].value.fetch_sub(1, std::memory_order_relaxed);
                    } else {
                        encoders[i].value.fetch_add(1, std::memory_order_relaxed);
                    }
                }
                encoders[i].last_clk = clk;
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

#ifdef PICO_ZERO
    void init_neopixel() {
        static bool initialized = false;
        if (initialized) return;

        PIO pio = pio1;
        int sm = 0;
        if (!pio_can_add_program(pio, &ws2812_program)) return;
            
        uint offset = pio_add_program(pio, &ws2812_program);
        ws2812_program_init(pio, sm, offset, 16, 800000, false); 
            
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


    void set_rgb_color(uint32_t pixel_grb) {
        pio_sm_put_blocking(pio1, 0, pixel_grb); 
    }


    void updateRGB(int index, float hue, float intensity) {
        if (index >= 12) return;

        float r = 0, g = 0, b = 0;
        float h = hue * 6.0f;
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

        float gamma = intensity * intensity;
        uint8_t uR = (uint8_t)(r * gamma * 255.0f);
        uint8_t uG = (uint8_t)(g * gamma * 255.0f);
        uint8_t uB = (uint8_t)(b * gamma * 255.0f);

        uint32_t color = ((uint32_t)(uG) << 16) | 
                        ((uint32_t)(uR) << 8)  | 
                        ((uint32_t)(uB));

        static uint32_t last_sent_color = 0;
        if (color != last_sent_color) {
            pio_sm_put_blocking(pio1, 0, color);
            last_sent_color = color;
        }
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


    bool processEnc(int index, float &val) {
        int current_count = encoders[index].value.load(std::memory_order_relaxed);
        int diff = current_count - encoders[index].last_sent_count;

        if (abs(diff) >= 1) {
            val = (diff > 0) ? 1.0f : -1.0f;
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


    static AudioMode _mode;
    static AudioProcessCallback _cb;
    static int _srate, _bpin, _dpin, _bsize; 

    void setupAudio(AudioMode mode, AudioProcessCallback callback, 
                    int sample_rate, uint data_pin, uint bclk_pin, int buffer_size) {
        _mode = mode;
        _cb = callback;
        _srate = sample_rate;
        _dpin = data_pin;
        _bpin = bclk_pin;
        _bsize = buffer_size;
    }


    void __not_in_flash_func(core1_audio_entry)() {
    audio_format_t audio_format = {
        .sample_freq   = (uint32_t)_srate,
        .format        = AUDIO_BUFFER_FORMAT_PCM_S16,
        .channel_count = (uint16_t)((_mode == I2S) ? 2 : 1)  // I2S stereo, PWM mono
    };

    audio_buffer_format_t producer_format = {
        .format        = &audio_format,
        .sample_stride = (uint16_t)(audio_format.channel_count * sizeof(int16_t))
    };

    float* heavy_buffer = new float[_bsize * audio_format.channel_count];
    assert(heavy_buffer);

    if (_mode == I2S) {
        struct audio_i2s_config i2s_config = {
            .data_pin       = (uint8_t)_dpin,
            .clock_pin_base = (uint8_t)_bpin,
            .dma_channel    = 0,
            .pio_sm         = 0
        };

        struct audio_buffer_pool* ap = audio_new_producer_pool(&producer_format, 3, _bsize);
        audio_i2s_setup(&audio_format, &i2s_config);
        audio_i2s_connect(ap);
        audio_i2s_set_enabled(true);

        while (true) {
            struct audio_buffer* buffer = take_audio_buffer(ap, true);
            if (!buffer) continue;

            if (_cb) {
                int frames = buffer->max_sample_count;
                _cb(heavy_buffer, frames);

                int16_t* out = (int16_t*)buffer->buffer->bytes;
                for (int i = 0; i < frames * 2; i++) {
                    float v = heavy_buffer[i];
                    if (v > 1.f) v = 1.f;
                    if (v < -1.f) v = -1.f;
                    out[i] = (int16_t)(v * 32767.f);
                }
            }

            buffer->sample_count = buffer->max_sample_count;
            give_audio_buffer(ap, buffer);
        }
    } 

    else {

        const uint pwm_pin = _dpin;
        gpio_set_function(pwm_pin, GPIO_FUNC_PWM);
        uint slice   = pwm_gpio_to_slice_num(pwm_pin);
        uint channel = pwm_gpio_to_channel(pwm_pin);

        const uint16_t wrap = 255;  
        pwm_set_wrap(slice, wrap);
        pwm_set_clkdiv(slice, 1.0f);
        pwm_set_enabled(slice, true);

        uint16_t* pwm_buffer = new uint16_t[_bsize];
        assert(pwm_buffer);

        int dma_chan = dma_claim_unused_channel(true);
        dma_channel_config cfg = dma_channel_get_default_config(dma_chan);
        channel_config_set_transfer_data_size(&cfg, DMA_SIZE_16);
        channel_config_set_read_increment(&cfg, true);
        channel_config_set_write_increment(&cfg, false);

        volatile uint16_t* pwm_cc_ptr = ((volatile uint16_t*)&pwm_hw->slice[slice].cc) + channel;

        dma_channel_configure(
        dma_chan,          
        &cfg,              
        pwm_cc_ptr,        
        pwm_buffer,        
        _bsize,           
        true              
    );

        while (true) {
            if (_cb) {
                _cb(heavy_buffer, _bsize);

                for (int i = 0; i < _bsize; i++) {
                    float v = heavy_buffer[i];
                    if (v > 1.f) v = 1.f;
                    if (v < -1.f) v = -1.f;
                    pwm_buffer[i] = (uint16_t)((v * 0.5f + 0.5f) * wrap);
                }

                dma_channel_set_read_addr(dma_chan, pwm_buffer, true);

                while (dma_channel_is_busy(dma_chan)) {
                tight_loop_contents();  
               }
            }
        }
    }       
}

}