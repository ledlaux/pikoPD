#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "pico/audio_i2s.h"
#include "pico/multicore.h"
#include "cdc_stdio_lib.h"
#include "PicoControl.h"
#include "Heavy_{{ name }}.hpp"


{% if settings.midi_mode == 'host' %}
// #ifndef CFG_TUH_ENABLED
// #define CFG_TUH_ENABLED 1
// #endif
// #ifndef CFG_TUH_MIDI
// #define CFG_TUH_MIDI 1
// #endif
// #include "tusb.h"
// #include "host/usbh.h"
// #include "class/midi/midi_host.h"
{% else %}
#include "tusb.h"
{% endif %}

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
#define MIDI_RT_ACTIVESENSE     0xFE
#define MIDI_RT_RESET           0xFF

{% set receives = {} %}
{%- for r in hv_manifest.receives -%}{%- set _ = receives.update({r.name: r.hash}) -%}
{%- endfor -%}

{%- set sends = {} -%}
{%- for s in hv_manifest.sends -%}{%- set _ = sends.update({s.name: s.hash}) -%}
{%- endfor -%}

{%- set active_btns = [] -%}
{%- for b in settings.buttons if b.name in receives -%}
    {%- set _ = active_btns.append({'pin': b.pin, 'mode': b.mode, 'hash': receives[b.name]}) -%}
{%- endfor -%}

{%- set active_gates = [] -%}
{%- for g in settings.gate_in if g.name in receives -%}
    {%- set _ = active_gates.append({'pin': g.pin, 'mode': 'gate_in', 'hash': receives[g.name]}) -%}
{%- endfor -%}

{%- set active_gate_outs = [] -%}
{%- set gate_base_index = active_btns|length + active_gates|length -%}
{%- for go in settings.gate_out if go.name in sends -%}
    {%- set dur = 15 if go.mode == "trigger" else 0 -%}
    {%- set index = gate_base_index + loop.index0 -%}
    {%- set _ = active_gate_outs.append({
        'pin': go.pin,
        'hash': sends[go.name],
        'duration': dur,
        'index': index   
    }) -%}
{%- endfor -%}

{%- set active_knobs = [] -%}
{%- for k in settings.adc_pins if k.name in receives -%}
    {%- set _ = active_knobs.append({'hash': receives[k.name], 'pin': k.pin, 'type': k.type}) -%}
{%- endfor -%}

{%- set active_leds = [] -%}
{%- for l in settings.leds if l.name in sends -%}
    {%- set _ = active_leds.append({'hash': sends[l.name], 'pin': l.pin}) -%}
{%- endfor -%}

{%- set active_encoders = [] -%}
{%- for e in settings.encoders if e.name in receives -%}
    {%- set _ = active_encoders.append({'a': e.pin_a, 'b': e.pin_b, 'hash': receives[e.name]}) -%}
{%- endfor -%}

{%- set active_joystick = [] -%}
{%- for j in settings.joystick -%}
    {%- set hash_x = receives[j.name + "_x"] -%}
    {%- set hash_y = receives[j.name + "_y"] -%}
    
    {%- if hash_x and hash_y -%}
        {%- set _ = active_joystick.append({
            'id': loop.index0,
            'x': j.joy_x,
            'y': j.joy_y,
            'hash_x': hash_x,
            'hash_y': hash_y,
            'midi_range': j.midi_range if j.midi_range is defined else false
        }) -%}
    {%- endif -%}
{%- endfor -%}

Heavy_{{ name }} pd_prog( {{ settings.sample_rate }} );

void handle_midi_message(uint8_t status, uint8_t data1, uint8_t data2);


{% if settings.midi_mode in ['uart', 'host'] %}
#define MIDI_RB_SIZE 512
typedef struct {
    volatile uint16_t head, tail;
    uint8_t data[MIDI_RB_SIZE];
} midi_ring_t;
static midi_ring_t midi_rb = {0};

static inline void rb_push(midi_ring_t *rb, uint8_t b) {
    uint16_t next = (rb->head + 1) % MIDI_RB_SIZE;
    if (next != rb->tail) { rb->data[rb->head] = b; rb->head = next; }
}

static inline int rb_pop(midi_ring_t *rb, uint8_t *b) {
    if (rb->head == rb->tail) return 0;
    *b = rb->data[rb->tail]; rb->tail = (rb->tail + 1) % MIDI_RB_SIZE;
    return 1;
}

void parse_raw_midi_byte(uint8_t byte) {
    static uint8_t msg[3];
    static int idx = 0;
    static int expected = 0;
    
    if (byte & 0x80) { 
        msg[0] = byte; 
        idx = 1;
        uint8_t type = byte & 0xF0;
        
        expected = (type == 0xC0 || type == 0xD0) ? 2 : 3;
    } 
    else if (idx > 0 && idx < 3) { 
        msg[idx++] = byte;
    }

    if (idx != 0 && idx == expected) {
        handle_midi_message(msg[0], msg[1], (expected == 3) ? msg[2] : 0);
        idx = 1; 
    }
}
{% endif %}

{% if settings.midi_mode == 'host' %}
void tuh_midi_rx_cb(uint8_t dev_idx, uint32_t xferred_bytes) {
    uint8_t buf[64]; uint8_t cable; uint32_t n;
    while ((n = tuh_midi_stream_read(dev_idx, &cable, buf, sizeof(buf))) > 0) {
        for (uint32_t i = 0; i < n; i++) rb_push(&midi_rb, buf[i]);
    }
}
{% endif %}


void handle_midi_message(uint8_t status, uint8_t data1, uint8_t data2) {
    
    uint8_t type = status & 0xF0;
    uint8_t chan = status & 0x0F;

    switch (type) {
        case 0x90: 
            if (data2 > 0) {
            //  printf("[MIDI IN] Note On: %d | Vel: %d | Chan: %d\n", data1, data2, chan);
                hv_sendMessageToReceiverV(&pd_prog, HV_NOTEIN_HASH, 0.0f, "fff", (float)data1, (float)data2, (float)chan);
                break;
            }
            [[fallthrough]]; 

        case 0x80: 
        //  printf("[MIDI IN] Note Off: %d | Chan: %d\n", data1, chan);
            hv_sendMessageToReceiverV(&pd_prog, HV_NOTEIN_HASH, 0.0f, "fff", (float)data1, 0.0f, (float)chan);
            break;

        case 0xB0: 
        //  printf("[MIDI IN] CC: %d | Val: %d | Chan: %d\n", data1, data2, chan);
            hv_sendMessageToReceiverV(&pd_prog, HV_CTLIN_HASH, 0.0f, "fff", (float)data2, (float)data1, (float)chan);
            break;

        case 0xE0: 
            {
                int bend = (data2 << 7) | data1;
        //      printf("[MIDI IN] Bend: %d | Chan: %d\n", bend, chan);
                hv_sendMessageToReceiverV(&pd_prog, HV_BENDIN_HASH, 0.0f, "ff", (float)bend, (float)chan);
            }
            break;

        default:
        //    printf("[MIDI IN] Other: Type 0x%02X | D1: %d | D2: %d\n", type, data1, data2);
            break;
    }
}


void heavyMidiOutHook(HeavyContextInterface *c, const char *receiverName, hv_uint32_t receiverHash, const HvMessage *m) {
    uint8_t packet[4] = {0}; 
    uint8_t raw[3] = {0};    
    int raw_len = 0;
    
    switch (receiverHash) {
        case HV_NOTEOUT_HASH: {
            int note = (int)msg_getFloat(m, 0);
            int vel = (int)msg_getFloat(m, 1);
            bool isNoteOn = (vel > 0);

            // Raw bytes
            raw[0] = isNoteOn ? 0x90 : 0x80;
            raw[1] = (uint8_t)note;
            raw[2] = (uint8_t)vel;
            raw_len = 3;

            // USB Packet
            packet[0] = isNoteOn ? 0x09 : 0x08; 
            packet[1] = raw[0];
            packet[2] = raw[1];
            packet[3] = raw[2];
            break;
        }

        case HV_CTL_OUT_HASH: {
            uint8_t ccNum = (uint8_t)msg_getFloat(m, 0);
            uint8_t ccVal = (uint8_t)msg_getFloat(m, 1);

            // Raw bytes
            raw[0] = 0xB0;
            raw[1] = ccNum;
            raw[2] = ccVal;
            raw_len = 3;

            // USB Packet
            packet[0] = 0x0B; 
            packet[1] = raw[0];
            packet[2] = raw[1];
            packet[3] = raw[2];
            break;
        }

        default:
            return; 
    }

    {% if settings.midi_usb_device %}
    if (tud_midi_mounted() && packet[0] != 0) {
        tud_midi_packet_write(packet);
    }
    {% endif %}

    {% if settings.midi_uart %}
    if (raw_len > 0) {
        for (int i = 0; i < raw_len; i++) {
            uart_putc(uart0, raw[i]);
        }
    }
    {% endif %}

    {% if settings.midi_host %}
    if (raw_len > 0) {
        tuh_midi_stream_write(1, 0, raw, raw_len);
    }
    {% endif %}
}

void midi_task() {
    // 1. USB Device 
    {% if settings.midi_mode == 'usb' %}
    if (tud_midi_available()) {
        uint8_t packet[4];
        while (tud_midi_packet_read(packet)) {
            handle_midi_message(packet[1], packet[2], packet[3]);
        }
    }
    {% endif %}

    // 2. USB Host 
    {% if settings.midi_mode == 'host' %}
    uint8_t b;
    while (rb_pop(&midi_rb, &b)) parse_raw_midi_byte(b);
    {% endif %}

    // 3. Hardware UART 
    {% if settings.midi_mode == 'uart' %}
    while (uart_is_readable(uart0)) {
        uint8_t raw_byte = uart_getc(uart0);
        parse_raw_midi_byte(raw_byte);
    }
    {% endif %}
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
    float val = hv_msg_getFloat(m, 0);

  //  printf("PD Message -> Name: %s | Hash: 0x%08X | Val: %f\n", name, hash, val);

    switch (hash) {
        {% for led in active_leds %}
        case {{ led.hash }}: 
            Pico::updateLed({{ loop.index0 }}, val);
            return;
        {% endfor %}

        {% for gate_out in active_gate_outs %}
        case {{ gate_out.hash }}: 
            Pico::updateGate({{ active_btns|length + active_gates|length + loop.index0 }}, val);
            return;
        {% endfor %}

        default:
            heavyMidiOutHook(vc, name, hash, m);
            break;
    }
}

void audioFunc(float* buffer, int frames) {
    pd_prog.processInlineInterleaved(buffer, buffer, frames);
}



int main() {
    set_sys_clock_khz({{ settings.core_freq }}, true);
    stdio_init_all(); 
    cdc_stdio_lib_init();

    {% if settings.midi_mode == 'uart' %}
    uart_init(uart0, 31250);
    uart_set_fifo_enabled(uart0, true); 
    gpio_set_function(0, GPIO_FUNC_UART); 
    gpio_set_function(1, GPIO_FUNC_UART); 
    {% endif %}

    {% if settings.midi_mode in ['usb', 'host'] %}
    tusb_init(); 
    {% endif %}

    pd_prog.setPrintHook(&hv_print_handler);
    pd_prog.setSendHook(&sendHookHandler);

    {% for btn in active_btns %}
    Pico::addPin({{ loop.index0 }}, {{ btn.pin }}, Pico::{{ btn.mode | upper }});
    {% endfor %}

    {% for gate in active_gates %}
    Pico::addPin({{ active_btns|length + loop.index0 }}, {{ gate.pin }}, Pico::GATE_IN);
    {% endfor %}

    {% for gate_out in active_gate_outs %}
    Pico::addPin({{ active_btns|length + active_gates|length + loop.index0 }}, {{ gate_out.pin }}, Pico::GATE_OUT, {{ gate_out.duration }});
    {% endfor %}

    {% for knob in active_knobs %}
        {% if knob.type == 'cv_in' %}
    Pico::addCV({{ loop.index0 }}, {{ knob.pin }});
        {% else %}
    Pico::addKnob({{ loop.index0 }}, {{ knob.pin }});
        {% endif %}
    {% endfor %}

    {% for led in active_leds %}
    Pico::addLed({{ loop.index0 }}, {{ led.pin }});
    {% endfor %}

    {% for enc in active_encoders -%}
    Pico::addEncoder({{ loop.index0 }}, {{ enc.a }}, {{ enc.b }});
    {% endfor %}

    {% for joy in active_joystick -%}
    Pico::addJoystick({{ joy.id }}, {{ joy.x }}, {{ joy.y }});
    {% endfor %}

    {% if settings.audio_mode == "I2S" %}
    Pico::setupAudio(Pico::I2S, audioFunc, {{ settings.sample_rate }}, {{ settings.i2s_data_pin }}, {{ settings.i2s_bclk_pin }}, {{ settings.buffer_size }});
    {% else %}
    Pico::setupAudio(Pico::PWM, audioFunc, {{ settings.sample_rate }}, {{ settings.pwm_pin_l }}, {{ settings.pwm_pin_r }}, {{ settings.buffer_size }});
    {% endif %}

    multicore_launch_core1(Pico::core1_audio_entry);

    uint32_t last_hw_tick = 0;
    float val, v; 
    bool send;

    while (true) {
        {% if settings.midi_mode == 'usb' %}
        tud_task(); 
        {% endif %}

        {% if settings.midi_mode == 'host' %}
        tuh_task(); 
        {% endif %}

        midi_task(); 

        uint32_t now = to_ms_since_boot(get_absolute_time()); 

        if (now != last_hw_tick) {
            last_hw_tick = now;
            Pico::update(now); 

            {% for btn in active_btns %}
            Pico::processPin({{ loop.index0 }}, val, send); 
            if (send) hv_sendFloatToReceiver(&pd_prog, {{ btn.hash }}, val);
            {% endfor %}

            {% for gate in active_gates %}
            Pico::processPin({{ active_btns|length + loop.index0 }}, val, send);
            if (send) hv_sendFloatToReceiver(&pd_prog, {{ gate.hash }}, val);
            {% endfor %}

            {% for knob in active_knobs -%}
            if (Pico::processKnob({{ loop.index0 }}, v)) hv_sendFloatToReceiver(&pd_prog, {{ knob.hash }}, v);
            {% endfor %}

            {% for enc in active_encoders -%}
            if (Pico::processEnc({{ loop.index0 }}, v)) hv_sendFloatToReceiver(&pd_prog, {{ enc.hash }}, v);
            {% endfor %}

            {% for joy in active_joystick -%}
            {
                float vx = 0.0f, vy = 0.0f;
                bool cX = false, cY = false;
                if (Pico::processJoystick({{ joy.id }}, vx, vy, cX, cY, {{ 'true' if joy.midi_range else 'false' }})) {
                    if (cX) hv_sendFloatToReceiver(&pd_prog, {{ joy.hash_x }}, vx);
                    if (cY) hv_sendFloatToReceiver(&pd_prog, {{ joy.hash_y }}, vy);
                }
            }
            {% endfor %}
        }
    } 
    return 0;
}
