#include <stdio.h>
#include "tusb.h"
#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "pico/audio_i2s.h"
#include "pico/multicore.h"
#include "cdc_stdio_lib.h"
#include "PicoControl.h"
#include "Heavy_{{ name }}.hpp"

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
{%- for go in settings.gate_out if go.name in sends -%}
    {%- set dur = 15 if go.mode == "trigger" else 0 -%}
    {%- set _ = active_gate_outs.append({'pin': go.pin, 'hash': sends[go.name], 'duration': dur}) -%}
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
    {%- set hash_x = receives[j.name + "_X"] -%}
    {%- set hash_y = receives[j.name + "_Y"] -%}
    
    {%- if hash_x and hash_y -%}
        {%- set _ = active_joystick.append({
            'x': j.joy_x, 
            'y': j.joy_y, 
            'hash_x': hash_x,
            'hash_y': hash_y,
            'midi_range': j.midi_range | default(false)
        }) -%}
    {%- endif -%}
{%- endfor -%}

Heavy_{{ name }} pd_prog( {{ settings.sample_rate }} );


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
    
    switch (receiverHash) {
        case HV_NOTEOUT_HASH: {
            int note = (int)msg_getFloat(m, 0);
            int vel = (int)msg_getFloat(m, 1);

            if (vel > 0) {
             // printf("[MIDI OUT] Note On: %d | Vel: %d\n", note, vel);
                packet[0] = 0x09; 
                packet[1] = 0x90; 
            } else {
            //  printf("[MIDI OUT] Note Off: %d\n", note);
                packet[0] = 0x08; 
                packet[1] = 0x80; 
            }
            packet[2] = (uint8_t)note;
            packet[3] = (uint8_t)vel;
            break;
        }

        case HV_CTL_OUT_HASH: {
            int ccNum = (int)msg_getFloat(m, 0);
            int ccVal = (int)msg_getFloat(m, 1);
        //  printf("[MIDI OUT] CC: %d | Val: %d\n", ccNum, ccVal);
            packet[0] = 0x0B; 
            packet[1] = 0xB0; 
            packet[2] = (uint8_t)ccNum;
            packet[3] = (uint8_t)ccVal;
            break;
        }
        default:
            return; 
    }
    if (tud_midi_mounted() && packet[0] != 0) {
        tud_midi_packet_write(packet);
    }
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
    float val = hv_msg_getFloat(m, 0);

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
    tusb_init(); 
    cdc_stdio_lib_init();

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

    {% for joy in active_joystick %}
    Pico::addJoystick({{ loop.index0 }}, {{ joy.x }}, {{ joy.y }});
    {% endfor %}

   {% if settings.audio_mode == "I2S" %}
    // --- I2S Configuration ---
    Pico::setupAudio(
        Pico::I2S, 
        audioFunc, 
        {{ settings.sample_rate }}, 
        {{ settings.i2s_data_pin }}, 
        {{ settings.i2s_bclk_pin }}, 
        {{ settings.buffer_size }}
    );
    {% else %}

    // --- PWM Configuration ---
    Pico::setupAudio(
        Pico::PWM, 
        audioFunc, 
        {{ settings.sample_rate }}, 
        {{ settings.pwm_pin_l }}, 
        {{ settings.pwm_pin_r }}, 
        {{ settings.buffer_size }}
    );
    {% endif %}

    multicore_launch_core1(Pico::core1_audio_entry);

    uint32_t last_hw_tick = 0;
    uint32_t last_print_time = 0;

    float val, v, vx, vy; 
    bool send;

    while (true) {
        tud_task(); 
        midi_task(); 

        uint32_t now = to_ms_since_boot(get_absolute_time()); 

        if (now != last_hw_tick) {
            last_hw_tick = now;
            Pico::update(now); 

            float val; 
            bool send;
            float v;

            {% for btn in active_btns %}
            Pico::processPin({{ loop.index0 }}, val, send); 
            if (send) { 
                hv_sendFloatToReceiver(&pd_prog, {{ btn.hash }}, val);
            }
            {% endfor %}

            {% for gate in active_gates %}
            Pico::processPin({{ active_btns|length + loop.index0 }}, val, send);
            if (send) {
                hv_sendFloatToReceiver(&pd_prog, {{ gate.hash }}, val);
            }
            {% endfor %}

            {% for knob in active_knobs -%}
            if (Pico::processKnob({{ loop.index0 }}, v)) {
                hv_sendFloatToReceiver(&pd_prog, {{ knob.hash }}, v);
            }
            {% endfor %}

            {% for enc in active_encoders -%}
            if (Pico::processEnc({{ loop.index0 }}, v)) {
                hv_sendFloatToReceiver(&pd_prog, {{ enc.hash }}, v);
            }
            {% endfor %}

        {% for joy in active_joystick -%}
            bool cX = false;
            bool cY = false;

            if (Pico::processJoystick({{ loop.index0 }}, vx, vy, cX, cY, {{ 'true' if joy.midi_range else 'false' }})) {
                
                if (cX) {
                    hv_sendFloatToReceiver(&pd_prog, {{ joy.hash_x }}, vx);
                }

                if (cY) {
                    hv_sendFloatToReceiver(&pd_prog, {{ joy.hash_y }}, vy);
                }
            }
            {% endfor %}
        }
    } 
    return 0;
}
