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
#include "tusb.h"

#ifdef PICO_ZERO
#include "ws2812.pio.h" 
#endif


void handle_midi_message(uint8_t status, uint8_t data1, uint8_t data2);

extern "C" void on_uart_rx();


namespace Pico {

    std::atomic<float> led_vals[12];
    std::atomic<float> led_hue[12];        
    std::atomic<float> led_intensity[12];
    static uint32_t led_framebuffer[12] = {0};
    static float smooth_hue[12] = {0.0f};
// -----------Interface hardware-----------

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

// -----------Audio-----------

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


// -----------MIDI-----------

    MidiBuffer midi_rb;
    static int usb_midi_dev0 = -1;
    static int usb_midi_dev1 = -1;

    void midi_push(uint8_t byte) {
        uint32_t h = midi_rb.head.load(std::memory_order_relaxed);
        uint32_t t = midi_rb.tail.load(std::memory_order_acquire);
        if ((h - t) < MIDI_RB_SIZE) {
            midi_rb.data[h & (MIDI_RB_SIZE - 1)] = byte;
            midi_rb.head.store(h + 1, std::memory_order_release);
        }
    }

    bool midi_pop(uint8_t &byte) {
        uint32_t t = midi_rb.tail.load(std::memory_order_relaxed);
        uint32_t h = midi_rb.head.load(std::memory_order_acquire);
        if (t == h) return false; // Empty
        byte = midi_rb.data[t & (MIDI_RB_SIZE - 1)];
        midi_rb.tail.store(t + 1, std::memory_order_release);
        return true;
    }


    void usb_init() {
   
        usb_midi_dev0 = -1;
        usb_midi_dev1 = -1;

        #ifdef MIDI_HOST
            tusb_init(0, NULL);
        #else
            tusb_init(); 
        #endif
    }


    void parse_raw_midi_byte(uint8_t byte, void (*handler)(uint8_t, uint8_t, uint8_t)) {
    
        if (byte >= 0xF8) { 
            handler(byte, 0, 0);
            return;
        }
    
        static uint8_t msg[3];
        static int idx = 0;
        static int expected = 0;
        
        if (byte & 0x80) { 
            msg[0] = byte; 
            idx = 1;
            uint8_t type = byte & 0xF0;
    
            if (type == 0xC0 || type == 0xD0) expected = 2; 
            else if (byte == 0xF2) expected = 3;          
            else if (byte == 0xF1 || byte == 0xF3) expected = 2; 
            else if (byte < 0xF0) expected = 3;            
            else {
                idx = 0;
                expected = 0; 
            }
        } 
    
        else if (idx > 0 && idx < 3) { 
            msg[idx++] = byte;
        }
    
        if (idx != 0 && idx == expected) {
            handler(msg[0], msg[1], (expected == 3) ? msg[2] : 0);
            
            if (msg[0] < 0xF0) {
                idx = 1; 
            } else {
                idx = 0;
                expected = 0;
            }
        }
    }


    void midi_task() {
        #ifdef MIDI_HOST
            tuh_task();
        #else
            tud_task(); 
        #endif
            uint8_t b;
            while (midi_pop(b)) {
                parse_raw_midi_byte(b, handle_midi_message);
            }
        }


    void uart_midi_init() {
        uart_deinit(uart0);
        uart_init(uart0, 31250); 
        gpio_set_function(0, GPIO_FUNC_UART); 
        gpio_set_function(1, GPIO_FUNC_UART); 
        uart_set_format(uart0, 8, 1, UART_PARITY_NONE);
        uart_set_hw_flow(uart0, false, false);
        uart_set_fifo_enabled(uart0, true);
        irq_set_exclusive_handler(UART0_IRQ, on_uart_rx);
        uart_set_irq_enables(uart0, true, false); 
        irq_set_enabled(UART0_IRQ, true);
    }


    void midi_task_uart() {
        uint8_t byte;
        while (midi_pop(byte)) {
            parse_raw_midi_byte(byte, handle_midi_message);
        }
    }
}
   

extern "C" {

#if MIDI_HOST
    void tuh_midi_rx_cb(uint8_t dev_addr, uint32_t qt) {
        (void)qt;
        uint8_t packet[4];
        while (tuh_midi_packet_read(dev_addr, packet)) {
            uint8_t cin = packet[0] & 0x0F;
            uint8_t len = 0;

            switch (cin) {
                case 0x05: case 0x0F: len = 1; break;
                case 0x02: case 0x06: case 0x0C: case 0x0D: len = 2; break;
                case 0x03: case 0x04: case 0x07: case 0x08: 
                case 0x09: case 0x0A: case 0x0B: case 0x0E: len = 3; break;
            }

            for (uint8_t i = 0; i < len; i++) {
                Pico::midi_push(packet[i + 1]);
            }
        }
    }

    void tuh_midi_mount_cb(uint8_t dev_addr, const tuh_midi_mount_cb_t *mount_cb_data) {
        (void)mount_cb_data;
        if (Pico::usb_midi_dev0 == -1) Pico::usb_midi_dev0 = dev_addr;
        else if (Pico::usb_midi_dev1 == -1) Pico::usb_midi_dev1 = dev_addr;
    }

    void tuh_midi_umount_cb(uint8_t dev_addr) {
        if (Pico::usb_midi_dev0 == dev_addr) Pico::usb_midi_dev0 = -1;
        else if (Pico::usb_midi_dev1 == dev_addr) Pico::usb_midi_dev1 = -1;
    }
#else
    void tud_midi_rx_cb(uint8_t itf) {
        (void)itf;
        uint8_t packet[4];
        while (tud_midi_available()) {
            if (tud_midi_packet_read(packet)) {
                uint8_t cin = packet[0] & 0x0F;
                uint8_t len = 0;

                switch (cin) {
                    case 0x05: case 0x0F: len = 1; break;
                    case 0x02: case 0x06: case 0x0C: case 0x0D: len = 2; break;
                    case 0x03: case 0x04: case 0x07: case 0x08: 
                    case 0x09: case 0x0A: case 0x0B: case 0x0E: len = 3; break;
                }

                for (uint8_t i = 0; i < len; i++) {
                    Pico::midi_push(packet[i + 1]);
                }
            }
        }
    }
#endif

void on_uart_rx() {
    while (uart_is_readable(uart0)) {
        Pico::midi_push(uart_getc(uart0));
    }
  }
}


// #include "lwip/apps/httpd.h"


// extern "C" {
//     // These satisfy the linker since we aren't using an actual filesystem
//     struct fs_file {
//         const char *data;
//         int len;
//         int index;
//         void *pextension;
//     };

//     int fs_open(struct fs_file *file, const char *name) {
//         return 0; // Always fail to find a file
//     }

//     void fs_close(struct fs_file *file) {
//         // Nothing to close
//     }

//     int fs_read(struct fs_file *file, char *buffer, int count) {
//         return 0; // Nothing to read
//     }

//     int fs_bytes_left(struct fs_file *file) {
//         return 0; // No bytes left
//     }
// }



// static const char *cgi_control_handler(int iIndex, int iNumParams, char *pcParam[], char *pcValue[]) {
//     uint32_t hash = 0;
//     float val = 0.0f;

//     for (int i = 0; i < iNumParams; i++) {
//         // 'h' for hex hash, 'v' for float value
//         if (strcmp(pcParam[i], "h") == 0) hash = (uint32_t)strtoul(pcValue[i], NULL, 16);
//         if (strcmp(pcParam[i], "v") == 0) val = atof(pcValue[i]);
//     }

//     if (hash != 0) {
//         hv_sendFloatToReceiver(&pd_prog, hash, val);
//     }

//     // Since we have no filesystem, return a non-existent path. 
//     // The command is already executed!
//     return "/404.html"; 
// }

// static const tCGI cgi_handlers[] = {
//     {"/control", cgi_control_handler},
// };

// void start_wifi() {
//     printf("\n--- Wi-Fi Initialization ---\n");
    
//     if (cyw43_arch_init()) {
//         printf("FAILED: Could not initialize cyw43 chip.\n");
//         return;
//     }
    
//     cyw43_arch_enable_sta_mode();
//     printf("Searching for SSID: %s...\n", "YOUR_SSID_HERE");

//     // This will block for up to 30 seconds
//     int connect_status = cyw43_arch_wifi_connect_timeout_ms(
//         "Redmi", 
//         "12345678", 
//         CYW43_AUTH_WPA2_AES_PSK, 
//         30000
//     );

//     if (connect_status != 0) {
//         printf("FAILED: Connection error code: %d\n", connect_status);
//     } else {
//         printf("SUCCESS! Connected to Wi-Fi.\n");
//         printf("IP Address: %s\n", ip4addr_ntoa(netif_ip4_addr(netif_default)));
        
//         httpd_init();
//         http_set_cgi_handlers(cgi_handlers, 1);
//         printf("HTTP Server Started on port 80.\n");
//     }
// }
