#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "hardware/adc.h"
#include "hardware/pwm.h"
#include "pico/audio_i2s.h"
#include "pico/binary_info.h"
#include "tusb.h"
#include "cdc_stdio_lib.h"
#include "Heavy_{{ name }}.hpp"

// --- Heavy hashes (inputs) ---
#define HV_NOTEIN_HASH       0x67E37CA3
#define HV_CTLIN_HASH        0x41BE0F9C
#define HV_POLYTOUCHIN_HASH  0xBC530F59
#define HV_PGMCHANGEIN_HASH  0x2E1EA03D
#define HV_TOUCHIN_HASH      0x553925BD
#define HV_BENDIN_HASH       0x3083F0F7
#define HV_MIDIIN_HASH       0x149631BE
#define HV_MIDIREALTIMEIN_HASH 0x6FFF0BCF

// --- Heavy hashes (outputs) ---
#define HV_NOTEOUT_HASH      0xD1D4AC2
#define HV_CTL_OUT_HASH      0xE5E2A040
#define HV_POLYTOUCHOUT_HASH 0xD5ACA9D1
#define HV_PGMCHANGEOUT_HASH 0x8753E39E
#define HV_TOUCHOUT_HASH     0x476D4387
#define HV_BENDOUT_HASH      0xE8458013
#define HV_MIDIOUT_HASH      0x6511DE55
#define HV_MIDIOUTPORT_HASH  0x165707E4

// --- Hardware & Config ---
#define I2S_DATA_PIN {{ settings.i2s_data_pin }}
#define I2S_BCLK_PIN {{ settings.i2s_bclk_pin }}
#define SAMPLE_RATE  {{ settings.sample_rate }}
#define MAX_VOICES   {{ settings.max_voices }}
#define I2S_BUFFER   {{ settings.buffer_size }}

#define LED_PIN {{ settings.led_builtin_pin }}  

// --- Global Objects & State ---
Heavy_{{ name }} pd_prog(SAMPLE_RATE);
float heavy_buffer[I2S_BUFFER * 2]; 
float volume = 1.0f; 

std::atomic<float> led_value{0.0f};
const hv_uint32_t LED_HASH = {{ led_hash }}   ;  


#if defined(ARDUINO_ARCH_RP2040) || defined(PICO_PLATFORM)

extern "C" {

bool __atomic_test_and_set(volatile void* ptr, int memorder) {
    (void)memorder;
    bool old = *(volatile bool*)ptr;
    *(volatile bool*)ptr = true;
    return old;
}

void __atomic_clear(volatile void* ptr, int memorder) {
    (void)memorder;
    *(volatile bool*)ptr = false;
}

} // extern "C"

#endif


struct Voice {
    uint8_t note = 0;
    bool active = false;
    hv_uint32_t hash = 0;
};

constexpr hv_uint32_t VOICE_HASHES[MAX_VOICES] = {
{% for recv in voice_hashes %}
    {{ recv.hash }}{% if not loop.last %},{% endif %} // {{ recv.name }}
{% endfor %}
};

Voice voices[MAX_VOICES];

int allocateVoice(uint8_t note) {
    for (int i = 0; i < MAX_VOICES; i++) {
        if (!voices[i].active) {
            voices[i].note = note;
            voices[i].active = true;
            voices[i].hash = VOICE_HASHES[i];
            return i;
        }
    }
    return -1; 
}

int findVoiceByNote(uint8_t note) {
    for (int i = 0; i < MAX_VOICES; i++) {
        if (voices[i].active && voices[i].note == note) return i;
    }
    return -1;
}

void init_led_pwm() {
    gpio_set_function(LED_PIN, GPIO_FUNC_PWM);
    uint slice_num = pwm_gpio_to_slice_num(LED_PIN);
    pwm_set_wrap(slice_num, 255);       // 8-bit resolution
    pwm_set_chan_level(slice_num, PWM_CHAN_A, 0);  // start off
    pwm_set_enabled(slice_num, true);
}

void update_led_pwm() {
    uint slice_num = pwm_gpio_to_slice_num(LED_PIN);
    uint chan = pwm_gpio_to_channel(LED_PIN);

    float lv = led_value.load();       // 0..1 from Pd patch
    lv = lv * 3.0f;                    // scale up to 3×
    if(lv > 1.0f) lv = 1.0f;           // clamp max

    pwm_set_chan_level(slice_num, chan, (uint16_t)(lv * 255));
}

void handle_midi_message(uint8_t status, uint8_t data1, uint8_t data2) {
    uint8_t type = status & 0xF0;
    uint8_t chan = status & 0x0F;
    
    if (type == 0x90 && data2 > 0) { // Note On
        int v = allocateVoice(data1);
        if (v >= 0) {
    //        printf("[MIDI] Note On:  %d | Vel: %d | Voice: %d\n", data1, data2, v);
            hv_sendMessageToReceiverV(&pd_prog, voices[v].hash, 0.0f, "fff", (float)data1, (float)data2, (float)chan);
        } else {
    //        printf("[MIDI] Note On:  %d | OUT OF VOICES\n", data1);
        }
    } 
    else if (type == 0x80 || (type == 0x90 && data2 == 0)) { // Note Off
        int v = findVoiceByNote(data1);
        if (v >= 0) {
    //        printf("[MIDI] Note Off: %d | Voice: %d\n", data1, v);
            hv_sendMessageToReceiverV(&pd_prog, voices[v].hash, 0.0f, "fff", (float)data1, 0.0f, (float)chan);
            voices[v].active = false;
        }
    }
    else if (type == 0xB0) { // CC
    //    printf("[MIDI] CC: %d | Val: %d\n", data1, data2);
        hv_sendMessageToReceiverV(&pd_prog, HV_CTLIN_HASH, 0.0f, "fff", (float)data2, (float)data1, (float)chan);
        if (data1 == 7) volume = data2 / 127.0f;
    }
    else if (type == 0xE0) { // Pitch Bend
        int bend = (data2 << 7) | data1;
    //    printf("[MIDI] Bend: %d\n", bend);
        hv_sendMessageToReceiverV(&pd_prog, HV_BENDIN_HASH, 0.0f, "ff", (float)bend, (float)chan);
    }
    else {
        // Helpful for identifying why other knobs/buttons aren't working
    //    printf("[MIDI] Other: Type 0x%02X | D1: %d | D2: %d\n", type, data1, data2);
    }
}


void heavyMidiOutHook(HeavyContextInterface *c, const char *receiverName, hv_uint32_t receiverHash, const HvMessage *m) {
    uint8_t packet[4] = {0};
    if (receiverHash == HV_NOTEOUT_HASH) {
        int note = (int)msg_getFloat(m, 0);
        int vel = (int)msg_getFloat(m, 1);
        packet[0] = (vel > 0 ? 0x09 : 0x08);
        packet[1] = (vel > 0 ? 0x90 : 0x80);
        packet[2] = (uint8_t)note;
        packet[3] = (uint8_t)vel;
    } else if (receiverHash == HV_CTL_OUT_HASH) {
        packet[0] = 0x0B;
        packet[1] = 0xB0;
        packet[2] = (uint8_t)msg_getFloat(m, 0);
        packet[3] = (uint8_t)msg_getFloat(m, 1);
    }
    if (tud_midi_mounted() && packet[0] != 0) tud_midi_packet_write(packet);
}


void midi_task() {
    if (!tud_midi_available()) return;
    uint8_t packet[4];
    while (tud_midi_packet_read(packet)) {
        handle_midi_message(packet[1], packet[2], packet[3]);
    }
}


void hv_print_handler(HeavyContextInterface *context, const char *printName, const char *str, const HvMessage *msg) {
    // This sends the PD [print] output directly to the USB Serial console
    printf("[%s] %s\n", printName, str);
}

void sendHookHandler(HeavyContextInterface *c, const char *name, hv_uint32_t hash, const HvMessage *m) {
    if(hash == LED_HASH) {
        float val = msg_getFloat(m, 0);
        led_value.store(val);
        printf("[LED] Received: %f\n", val);
    }
    heavyMidiOutHook(c, name, hash, m);
}

struct audio_buffer_pool *init_audio() {
    static audio_format_t audio_format = {
        .sample_freq = SAMPLE_RATE,
        .format = AUDIO_BUFFER_FORMAT_PCM_S16,
        .channel_count = 2
    };
    static audio_buffer_format_t producer_format = {
        .format = &audio_format,
        .sample_stride = 4 
    };

    struct audio_buffer_pool *pool = audio_new_producer_pool(&producer_format, 3, I2S_BUFFER);
    struct audio_i2s_config config = {
        .data_pin = I2S_DATA_PIN,
        .clock_pin_base = I2S_BCLK_PIN,
        .dma_channel = 0,
        .pio_sm = 0
    };
    audio_i2s_setup(&audio_format, &config);
    audio_i2s_connect(pool);
    audio_i2s_set_enabled(true);
    return pool;
}

int main() {
    set_sys_clock_khz({{ settings.core_freq }}, true); 
    tusb_init();
    cdc_stdio_lib_init();

    pd_prog.setPrintHook(&hv_print_handler);
    pd_prog.setSendHook(&sendHookHandler);

    init_led_pwm(); 

    struct audio_buffer_pool *ap = init_audio();

    while (true) {
        tud_task(); 
        midi_task(); 

        update_led_pwm();

        struct audio_buffer *buffer = take_audio_buffer(ap, false);
        if (buffer) {
            int16_t *samples = (int16_t *)buffer->buffer->bytes;
            
            // Process the Heavy DSP graph
            pd_prog.processInlineInterleaved(heavy_buffer, heavy_buffer, buffer->max_sample_count);
            
            // Convert float to int16 with global volume
            for (int i = 0; i < buffer->max_sample_count * 2; i++) {
                samples[i] = (int16_t)(heavy_buffer[i] * volume * 32767.0f);
            }
            
            buffer->sample_count = buffer->max_sample_count;
            give_audio_buffer(ap, buffer);
        }
    }
    return 0;
}
