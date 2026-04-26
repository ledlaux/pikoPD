#pragma once

#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "pico/audio_i2s.h" 
#include "hardware/pwm.h"
#include "hardware/dma.h"
#include <stdint.h>
#include <string.h>
#include <cmath>
#include <algorithm>

//#define USE_DELAY
#define USE_REVERB
#define USE_LIMITER

#ifdef USE_REVERB
#include "masterfx/freeverb.h"
#endif

#define USE_PWM_AUDIO

#define MAX_BLOCK_SIZE 1024

enum AudioMode { I2S, PWM };


class StereoDelay {
private:
    int16_t echoL[24000]{0}, echoR[24000]{0};
    uint32_t write_ptr = 23999;
    float current_samples = 12000.0f;
    float smooth_level = 0.0f;
    float smooth_fb = 0.1f;

public:
    volatile float target_samples = 12000.0f;
    volatile float target_level = 0.0f;
    volatile float target_fb = 0.1f;
    volatile bool bypass = false;

    void set_time(float t) { 
        float raw = 10.0f + (t * t * 44990.0f);
        target_samples = std::min(raw, 23000.0f); 
    }

    void inline process(float inL, float inR, float& wetL, float& wetR) {
    //  parameter smoothing 
    current_samples += 0.01f * (target_samples - current_samples);
    smooth_level    += 0.01f * (target_level - smooth_level);
    smooth_fb       += 0.01f * (target_fb - smooth_fb);

    if (bypass || smooth_level < 0.001f) {
        wetL = 0; wetR = 0;
        return;
    }

    float delay_val = current_samples;
    uint32_t iPart = (uint32_t)delay_val;
    float fPart = delay_val - iPart;
    uint32_t frac = (uint32_t)(fPart * 65536.0f);

    auto read_buf = [&](int16_t* buf, uint32_t offset) {
        uint32_t p1 = (write_ptr + iPart + offset) % 24000;
        uint32_t p2 = (p1 + 1) % 24000;
        
        int32_t sample1 = buf[p1];
        int32_t sample2 = buf[p2];
        int32_t interpolated = (sample1 * (65536 - frac) + sample2 * frac) >> 16;
        
        return (float)interpolated * 0.000030518f; // (1/32768)
    };

    wetL = read_buf(echoL, 0) * smooth_level;
    wetR = read_buf(echoR, 500) * smooth_level;

    float fbL = inL + (wetR * smooth_fb);
    float fbR = inR + (wetL * smooth_fb);
    
    fbL = (fbL > 1.0f) ? 1.0f : (fbL < -1.0f) ? -1.0f : fbL;
    fbR = (fbR > 1.0f) ? 1.0f : (fbR < -1.0f) ? -1.0f : fbR;

    echoL[write_ptr] = (int16_t)(fbL * 32767.0f);
    echoR[write_ptr] = (int16_t)(fbR * 32767.0f);

    if (write_ptr == 0) write_ptr = 23999; else write_ptr--;
}
};


class SoftLimiter {
private:
    float gain_reducer = 1.0f;
    const float release_coeff = 0.0005f;
    const float threshold = 0.98f;

public:
    volatile bool bypass = false;

    void inline process(float& l, float& r) {
        if (bypass) return;

        float peak = std::max(std::fabsf(l), std::fabsf(r));
        
        if (peak > threshold) {
            float target = threshold / peak;
            if (target < gain_reducer) gain_reducer = target;
        } else {
            gain_reducer += (1.0f - gain_reducer) * release_coeff;
        }

        l *= gain_reducer;
        r *= gain_reducer;
    }
};

class MasterFX {
public:
    #ifdef USE_DELAY
        StereoDelay delay;
    #endif
    #ifdef USE_REVERB
        FreeverbStereo reverb;
    #endif
    #ifdef USE_LIMITER
        SoftLimiter limiter;
    #endif

    volatile float master_volume = 0.8f;
    volatile float reverb_mix = 0.0f;
    volatile bool reverb_bypass = false;

    MasterFX() : initialized(false) {}

    void init() {
        #ifdef USE_REVERB
            reverb.init();
        #endif
        initialized = true;
    }

    void process_inplace(float* buffer, int frames) {
        if (!initialized) return;

        for (int i = 0; i < frames; i++) {
            float& l = buffer[i * 2];
            float& r = buffer[i * 2 + 1];

            const float originalL = l;
            const float originalR = r;

            float dWetL = 0, dWetR = 0;
            
            #ifdef USE_DELAY
                delay.process(originalL, originalR, dWetL, dWetR);
            #endif

            #ifdef USE_REVERB
                float rWetL = 0, rWetR = 0;
                float mix = reverb_mix;
                
                if (!reverb_bypass && mix > 0.001f) {
                    reverb.Process(originalL + (dWetL * 0.1f), originalR + (dWetR * 0.1f), rWetL, rWetR);
                    
                    float dryGain = 1.0f - mix;
                    l = (originalL * dryGain) + (rWetL * mix);
                    r = (originalR * dryGain) + (rWetR * mix);
                } else {
                    l = originalL;
                    r = originalR;
                }
            #else
                l = originalL;
                r = originalR;
            #endif

            #ifdef USE_DELAY
                l += dWetL;
                r += dWetR;
            #endif

            l *= master_volume;
            r *= master_volume;
            
            #ifdef USE_LIMITER
                limiter.process(l, r);
            #endif
        }
    }

private:
    bool initialized;
};

extern MasterFX masterFX;


namespace Pico {

    typedef void (*AudioProcessCallback)(float* buffer, int frames);

    static float heavy_buffer[MAX_BLOCK_SIZE * 2] __attribute__((aligned(4)));

    #ifdef USE_PWM_AUDIO
    static uint16_t static_pwm_buffers[2][MAX_BLOCK_SIZE] __attribute__((aligned(4)));
    #endif

    static AudioMode _mode;
    static AudioProcessCallback _cb;
    static int _srate, _bpin, _dpin, _bsize; 

    void init_dsp_effects() { 
        masterFX.init(); 
    }

    void process_master_effects(float* buffer, int frames) { 
        masterFX.process_inplace(buffer, frames); 
    }


    void setupAudio(AudioMode mode, AudioProcessCallback callback, 
                            int sample_rate, uint data_pin, uint bclk_pin, int buffer_size) {
            _mode = mode; _cb = callback; _srate = sample_rate;
            _dpin = data_pin; _bpin = bclk_pin; _bsize = buffer_size;
        }

    void __not_in_flash_func(core1_audio_entry)() {
        audio_format_t audio_format = {
            .sample_freq   = (uint32_t)_srate,
            .format        = AUDIO_BUFFER_FORMAT_PCM_S16,
            .channel_count = (uint16_t)((_mode == I2S) ? 2 : 1) 
        };

        audio_buffer_format_t producer_format = {
            .format        = &audio_format,
            .sample_stride = (uint16_t)(audio_format.channel_count * sizeof(int16_t))
        };

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
                struct audio_buffer* buffer = take_audio_buffer(ap, false);
                if (buffer) {
                    if (_cb) {
                        int frames = buffer->max_sample_count;
                        _cb(heavy_buffer, frames);
                        
                        // Process entire FX chain: Delay -> Reverb -> Limiter
                        process_master_effects(heavy_buffer, frames);
                        
                        int16_t* out = (int16_t*)buffer->buffer->bytes;
                        for (int i = 0; i < frames * 2; i++) {
                        float v = std::clamp(heavy_buffer[i], -1.0f, 1.0f);
                        out[i] = (int16_t)(v * 32767.0f);
                    }
                    }
                    buffer->sample_count = buffer->max_sample_count;
                    give_audio_buffer(ap, buffer);
                }       
            }
        }
        else {
            const uint pwm_pin = _dpin;
            gpio_set_function(pwm_pin, GPIO_FUNC_PWM);
            uint slice   = pwm_gpio_to_slice_num(pwm_pin);
            uint channel = pwm_gpio_to_channel(pwm_pin);
            const uint16_t wrap = 4095;  
            pwm_set_wrap(slice, wrap);
            pwm_set_enabled(slice, true);

            uint16_t* pwm_buffers[2] = { static_pwm_buffers[0], static_pwm_buffers[1] };
            int write_idx = 0;
            int dma_chan = dma_claim_unused_channel(true);
            dma_channel_config cfg = dma_channel_get_default_config(dma_chan);
            channel_config_set_transfer_data_size(&cfg, DMA_SIZE_16);
            channel_config_set_read_increment(&cfg, true);
            channel_config_set_write_increment(&cfg, false);
            channel_config_set_dreq(&cfg, pwm_get_dreq(slice));
            volatile uint16_t* pwm_cc_ptr = ((volatile uint16_t*)&pwm_hw->slice[slice].cc) + channel;

            while (true) {
                if (_cb) {
                    _cb(heavy_buffer, _bsize);
                    
                    process_master_effects(heavy_buffer, _bsize);

                    for (int i = 0; i < _bsize; i++) {
                        float v = (heavy_buffer[i * 2] + heavy_buffer[i * 2 + 1]) * 0.5f;
                        v = std::clamp(v, -1.0f, 1.0f);
                        pwm_buffers[write_idx][i] = (uint16_t)((v * 0.5f + 0.5f) * wrap);
                    }

                    dma_channel_wait_for_finish_blocking(dma_chan);
                    dma_channel_configure(dma_chan, &cfg, pwm_cc_ptr, pwm_buffers[write_idx], _bsize, true);
                    write_idx = 1 - write_idx;
                }
            }
        }
    }
}
