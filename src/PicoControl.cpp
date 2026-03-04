#include "PicoControl.h"
#include "hardware/dma.h"
#include "hardware/adc.h"
#include "hardware/pwm.h"
#include "hardware/gpio.h"
#include "pico/audio_i2s.h"
#include "pico/multicore.h"
#include "pico/audio_pwm.h"
#include "pico/time.h"
#include <cmath>

namespace Pico {

    std::atomic<float> led_vals[12];

    Button btns[12];
    Led leds[12];
    Knob knobs[4];
    Encoder encoders[4];

    int n_btn = 0;
    int n_knob = 0;
    int n_led = 0;
    int n_encoder = 0; 

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
        if (index >= n_led) n_led = index + 1;
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
                knobs[i].value.store(prev + (raw - prev) * knobs[i].coeff, std::memory_order_relaxed);  // smoothening
            }
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
