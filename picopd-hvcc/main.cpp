#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "pico/audio_i2s.h"
#include "pico/binary_info.h"
#include "tusb.h"
#include "cdc_stdio_lib.h"
#include "Heavy_{{ name }}.hpp"
#include "PicoControl.hpp"

#define SAMPLE_RATE {{ settings.sample_rate }}
#define I2S_DATA_PIN {{ settings.i2s_data_pin }}
#define I2S_BCLK_PIN {{ settings.i2s_bclk_pin }}
#define I2S_BUFFER   {{ settings.buffer_size }}

#define HV_NOTEIN_HASH       0x67E37CA3
#define HV_CTLIN_HASH        0x41BE0F9C
#define HV_POLYTOUCHIN_HASH  0xBC530F59
#define HV_PGMCHANGEIN_HASH  0x2E1EA03D
#define HV_TOUCHIN_HASH      0x553925BD
#define HV_BENDIN_HASH       0x3083F0F7
#define HV_MIDIIN_HASH       0x149631BE
#define HV_MIDIREALTIMEIN_HASH 0x6FFF0BCF

#define HV_NOTEOUT_HASH      0xD1D4AC2
#define HV_CTL_OUT_HASH      0xE5E2A040
#define HV_POLYTOUCHOUT_HASH 0xD5ACA9D1
#define HV_PGMCHANGEOUT_HASH 0x8753E39E
#define HV_TOUCHOUT_HASH     0x476D4387
#define HV_BENDOUT_HASH      0xE8458013
#define HV_MIDIOUT_HASH      0x6511DE55
#define HV_MIDIOUTPORT_HASH  0x165707E4

#define MIDI_RT_CLOCK           0xF8
#define MIDI_RT_START           0xFA
#define MIDI_RT_CONTINUE        0xFB
#define MIDI_RT_STOP            0xFC
#define MIDI_RT_ACTIVESENSE     0xFE
#define MIDI_RT_RESET           0xFF

{%- set active_btns = [] -%}
{%- for b in settings.buttons -%}
    {%- for r in hv_manifest.receives if r.name == b.name -%}
        {%- set _ = active_btns.append(r.hash) -%}
    {%- endfor -%}
{%- endfor -%}
{%- set active_leds = [] -%}
{%- for l in settings.leds -%}
    {%- for s in hv_manifest.sends if s.name == l.name -%}
        {%- set _ = active_leds.append(s.hash) -%}
    {%- endfor -%}
{%- endfor -%}
{% if active_btns %}

static const uint32_t btn_hashes[] = { {{ active_btns | join(', ') }} };
{% endif %}
{% if active_leds %}
static const uint32_t led_hashes[] = { {{ active_leds | join(', ') }} };
{% endif %}

Heavy_{{ name }} pd_prog(SAMPLE_RATE);

float heavy_buffer[I2S_BUFFER * 2];
float volume = 1.0f;

void handle_midi_message(uint8_t status, uint8_t data1, uint8_t data2) {
    uint8_t type = status & 0xF0;
    uint8_t chan = status & 0x0F;

    if (type == 0x90 && data2 > 0) { // Note On
        hv_sendMessageToReceiverV(
        &pd_prog,
        HV_NOTEIN_HASH,
        0.0f,
        "fff",
        (float)data1,
        (float)data2,
        (float)chan
        );
    }
    else if (type == 0x80 || (type == 0x90 && data2 == 0)) { // Note Off
        hv_sendMessageToReceiverV(
        &pd_prog,
        HV_NOTEIN_HASH,
        0.0f,
        "fff",
        (float)data1,
        0.0f,
        (float)chan
        );
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
    bool handled = false;

    {% for p in hv_manifest.prints -%}
    if (strcmp(printName, "{{ p.name }}") == 0) {
        printf("[%s] %s\n", printName, str);
        handled = true;
    }
    {% endfor %}
    if (!handled) {
        printf("[print] %s: %s\n", printName, str);
    }
}

void sendHookHandler(HeavyContextInterface *vc, const char *name, uint32_t hash, const HvMessage *m) {
    {% if active_leds %}
    for (int i = 0; i < {{ active_leds|length }}; i++) {
        if (hash == led_hashes[i]) {
            Pico::led_vals[i].store(hv_msg_getFloat(m, 0));
            return;
        }
    }
    {% endif %}
    hv_print_handler(vc, name, name, m); 
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
    stdio_init_all(); 
    tusb_init(); 
    cdc_stdio_lib_init();

    pd_prog.setPrintHook(&hv_print_handler);
    pd_prog.setSendHook(&sendHookHandler);

    {% set b_count = namespace(value=0) %}
    {% for b in settings.buttons %}
        {%- for r in hv_manifest.receives if r.name == b.name -%}
    Pico::addBtn({{ b_count.value }}, {{ b.pin }});
            {%- set b_count.value = b_count.value + 1 -%}
        {%- endfor -%}
    {% endfor %}
    {% set k_count = namespace(value=0) %}
    {% for p in settings.adc_pins %}
        {%- for r in hv_manifest.receives if r.name == p.name -%}
    Pico::addKnob({{ k_count.value }}, {{ p.pin }});
            {%- set k_count.value = k_count.value + 1 -%}
        {%- endfor -%}
    {% endfor %}
    {% set l_count = namespace(value=0) %}
    {% for l in settings.leds %}
        {%- for s in hv_manifest.sends if s.name == l.name -%}
    Pico::addLed({{ l_count.value }}, {{ l.pin }});
            {%- set l_count.value = l_count.value + 1 -%}
        {%- endfor -%}
    {% endfor %}

    auto *ap = init_audio();
    uint32_t last_hw_tick = 0;

    while (true) {
        tud_task(); 
        midi_task();

        struct audio_buffer *buffer = take_audio_buffer(ap, false);
        if (buffer) {
            pd_prog.processInlineInterleaved(heavy_buffer, heavy_buffer, buffer->max_sample_count);
            int16_t *smp = (int16_t *)buffer->buffer->bytes;
            for (int i = 0; i < buffer->max_sample_count * 2; i++) {
                smp[i] = (int16_t)(heavy_buffer[i] * 0.8f * 32767.0f);
            }
            buffer->sample_count = buffer->max_sample_count;
            give_audio_buffer(ap, buffer);
        }

        uint32_t now = to_ms_since_boot(get_absolute_time()); 
        if (now != last_hw_tick) {
            last_hw_tick = now;
            Pico::update();

            // --- BUTTONS ---
            {% for b in settings.buttons %}
                {% set b_idx = loop.index0 %}
                {%- for r in hv_manifest.receives if r.name == b.name -%}
            {
                auto& btn = Pico::btns[{{ b_idx }}];
                bool phys = btn.state.load();

                {% if b.mode == "bang" %}
                if (phys && !btn.last) {
                    btn.last = true;
                    hv_sendFloatToReceiver(&pd_prog, {{ r.hash }}, 1.0f);
                } 
                else if (!phys && btn.last) {
                    btn.last = false;
                    hv_sendFloatToReceiver(&pd_prog, {{ r.hash }}, 0.0f);
                }
                {% elif b.mode == "toggle" %}
                if (phys && !btn.last) {
                    btn.last = true;
                    btn.toggle_state = !btn.toggle_state;
                    hv_sendFloatToReceiver(&pd_prog, {{ r.hash }}, btn.toggle_state ? 1.0f : 0.0f);
                } 
                else if (!phys) {
                    btn.last = false;
                }
                {% endif %}
            } 
                {%- endfor -%}
            {% endfor %}

            // --- KNOBS ---
            {% for k in settings.adc_pins %}
                {% set k_idx = loop.index0 %}
                {%- for r in hv_manifest.receives if r.name == k.name -%}
            {
                float v;
                if (Pico::knobChanged({{ k_idx }}, v)) {
                    hv_sendFloatToReceiver(&pd_prog, {{ r.hash }}, v);
                }
            }
                {%- endfor -%}
            {% endfor %}

            // --- LEDS ---
            {% for l in settings.leds %}
                {% set l_idx = loop.index0 %}
                {%- for s in hv_manifest.sends if s.name == l.name -%}
            Pico::setLedHardware({{ l_idx }}, Pico::led_vals[{{ l_idx }}].load());
                {%- endfor -%}
            {% endfor %}
        } 
    } 
    return 0; 
}
