#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <atomic>
#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "pico/multicore.h"
#include "PicoControl.h"
#include "PicoAudio.h"
#include "PicoMIDI.h"
#include "PicoWEB.h"

{%- if board.masterfx %}
MasterFX masterFX;
{%- endif %}

{% if board.pico_board == 'pico_w' -%}
#include "pico/cyw43_arch.h"
{%- endif %}

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

Heavy_{{ name }} pd_prog( {{ board.sample_rate }} );

{% set receives = {} %}
{%- for r in hv_manifest.receives -%}{%- set _ = receives.update({r.name: r.hash}) -%}
{%- endfor -%}

{%- set sends = {} -%}
{%- for s in hv_manifest.sends -%}{%- set _ = sends.update({s.name: s.hash}) -%}
{%- endfor -%}

{%- set active_btns = [] -%}
{%- for b in board.inputs.buttons if b.name in receives -%}
    {%- set _ = active_btns.append({'pin': b.pin, 'mode': b.mode, 'hash': receives[b.name]}) -%}
{%- endfor -%}

{%- set active_gates = [] -%}
{%- for g in board.inputs.gate_in if g.name in receives -%}
    {%- set _ = active_gates.append({'pin': g.pin, 'mode': 'gate_in', 'hash': receives[g.name]}) -%}
{%- endfor -%}

{%- set active_gate_outs = [] -%}
{%- set gate_base_index = active_btns|length + active_gates|length -%}
{%- for go in board.outputs.gate_out if go.name in sends -%}
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
{%- for k in board.inputs.adc_pins if k.name in receives -%}
    {%- set _ = active_knobs.append({'hash': receives[k.name], 'pin': k.pin, 'type': k.type}) -%}
{%- endfor -%}

{%- set active_leds = [] -%}
{%- for l in board.leds -%}
    {%- set led_hash = 0 -%}
    
    {%- if l.mode == 'pd' -%}
        {%- for s in hv_manifest.sends -%}
            {%- if s.name == l.name -%}
                {%- set led_hash = s.hash -%}
            {%- endif -%}
        {%- endfor -%}
    {%- endif -%}

    {%- set _ = active_leds.append({
        'name': l.name,
        'hash': led_hash, 
        'pin': l.pin, 
        'is_rgb': l.is_rgb | default(false),
        'mode': l.mode | default('status')
    }) -%}
{%- endfor -%}

{%- set active_encoders = [] -%}
{%- for e in board.inputs.encoders if e.name in receives -%}
    {%- set _ = active_encoders.append({'a': e.pin_a, 'b': e.pin_b, 'hash': receives[e.name]}) -%}
{%- endfor -%}

{%- set active_joystick = [] -%}
{%- for j in board.inputs.joystick -%}
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

{%- set active_cny70 = [] -%}
{%- for cny in board.inputs.sensors.cny70 if cny.name in receives -%}
    {%- set _ = active_cny70.append({'adc_pin': cny.adc_pin, 'hash': receives[cny.name]}) -%}
{%- endfor -%}

{%- set active_dist = [] -%}
{%- if board.inputs.sensors['hc-sr04'] -%}
    {%- for d in board.inputs.sensors['hc-sr04'] if d.name in receives -%}
        {%- set _ = active_dist.append({
            'trig': d.trigger, 
            'echo': d.echo, 
            'hash': receives[d.name]
        }) -%}
    {%- endfor -%}
{%- endif -%}

#define FLASH_DURATION_MS 40
#define CLOCK_FLASH_MS 30 
static uint32_t midi_activity_timer = 0;
static uint8_t last_midi_velocity = 0;
static uint8_t current_midi_note = 60; 
static uint32_t last_led_tick = 0;
static uint32_t midi_clock_timer = 0;
static uint8_t clock_count = 0;
static bool clock_running = false; 
static bool debug_enabled = true;
static uint32_t last_print_tick = 0;

// ---- NOTE receives ----

#ifndef MAX_VOICES
#define MAX_VOICES 1
#endif

{% set note_receives = [] -%}
{% for r in hv_manifest.receives if r.name.lower().startswith("note") -%}
    {% set _ = note_receives.append(r.hash) -%}
{% endfor -%}
{% set note_list = note_receives[:board.voice_count] -%}
{% if note_list | length > 1 %}
constexpr uint32_t VOICE_HASHES[MAX_VOICES] = {
{%- for hash in note_list %}
    {{ hash }}{{ "," if not loop.last }}
{%- endfor %}
};
{% endif %}

// ---- MPR121 pad objects ----

{% if board.inputs.sensors.mpr121 -%}
struct MprPad { const char* sensor_name; int sensor_idx; int pad_idx; const char* pad_name; uint32_t hash; };

{% set active_count = namespace(value=0) -%}
{% set mpr_count = board.inputs.sensors.mpr121|length -%}

MprPad active_mpr_pads[] = {
{#- Range covers up to 8 sensors -#}
{%- for i in range(1, 97) %}
    {%- set p_name = "pad" ~ i %}
    {%- for p in hv_manifest.receives if p.name == p_name %}
        {%- set s_idx = (i - 1) // 12 -%}
        {%- set p_idx = (i - 1) % 12 -%}
        {%- if s_idx < mpr_count %}
    { "{{ board.inputs.sensors.mpr121[s_idx].name }}", {{ s_idx }}, {{ p_idx }}, "{{ p.name }}", {{ p.hash }} },
            {%- set active_count.value = active_count.value + 1 -%}
        {%- endif %}
    {%- endfor %}
{%- endfor %}
};

constexpr int NUM_ACTIVE_MPR_PADS = {{ active_count.value }};
{%- endif %}

// ---------------------------

// https://github.com/Wasted-Audio/hvcc/issues/175
#define HV_HEAVY_SPINLOCK 5

inline void hv_send_msg3_lock(uint32_t hash, float a, float b, float c) {
    uint32_t irq = spin_lock_blocking(spin_lock_instance(HV_HEAVY_SPINLOCK));
    hv_sendMessageToReceiverV(&pd_prog, hash, 0.0f, "fff", a, b, c);
    spin_unlock(spin_lock_instance(HV_HEAVY_SPINLOCK), irq);
}

inline void hv_send_msg2_lock(uint32_t hash, float a, float b) {
    uint32_t irq = spin_lock_blocking(spin_lock_instance(HV_HEAVY_SPINLOCK));
    hv_sendMessageToReceiverV(&pd_prog, hash, 0.0f, "ff", a, b);
    spin_unlock(spin_lock_instance(HV_HEAVY_SPINLOCK), irq);
}

inline void hv_send_msg1_lock(uint32_t hash, float a) {
    uint32_t irq = spin_lock_blocking(spin_lock_instance(HV_HEAVY_SPINLOCK));
    hv_sendMessageToReceiverV(&pd_prog, hash, 0.0f, "f", a);
    spin_unlock(spin_lock_instance(HV_HEAVY_SPINLOCK), irq);
}


void handle_midi_message(uint8_t status, uint8_t data1, uint8_t data2) {

    uint32_t now = to_ms_since_boot(get_absolute_time());

    if (status >= 0xF8) {
        hv_send_msg1_lock(HV_MIDIREALTIMEIN_HASH, (float)status);
        switch(status) {
            case MIDI_RT_START:    clock_count = 23; clock_running = true; break;
            case MIDI_RT_CONTINUE: clock_running = true; break;
            case MIDI_RT_STOP:     clock_running = false; break;
            case MIDI_RT_CLOCK:    
                if(clock_running && ++clock_count >= 24) {
                    clock_count = 0;
                    midi_clock_timer = now + CLOCK_FLASH_MS;
                }
                break;
        }
        return;
    }

    uint8_t type = status & 0xF0;
    uint8_t chan = status & 0x0F;
    float f_chan = (float)chan + 1.0f; 

    switch(type) {
        case 0x90: { 
            if (data2 > 0) {
                last_midi_velocity = data2;
                current_midi_note = data1;
                midi_activity_timer = now + FLASH_DURATION_MS;
                
                hv_send_msg3_lock(HV_NOTEIN_HASH, (float)data1, (float)data2, f_chan);

                {%- if board.voice_count > 1 %}
                {
                    int v_idx = allocateVoice(data1); 
                    hv_send_msg3_lock(VOICE_HASHES[v_idx], (float)data1, (float)data2, f_chan);
                }
                {%- endif %}
                break; 
            }
            [[fallthrough]];
        }

        case 0x80: { 
             hv_send_msg3_lock(HV_NOTEIN_HASH, (float)data1, 0.0f, f_chan);

            {%- if board.voice_count > 1 %}
            {
                int i = findVoiceByNote(data1);
                if (i >= 0) {
                    sendVoiceNoteOff(i, data1);
                    hv_send_msg3_lock(VOICE_HASHES[i], (float)data1, 0.0f, f_chan);
                }
            }
            {%- endif %}
            break;
        }
        
        case 0xB0: { 
            float val = (float)data2 / 127.0f;
            bool is_on = (data2 > 64);
            
            switch(data1) {
                {%- if board.masterfx.limiter %}
                case 7: masterFX.master_volume = val; break; 
                case 8: masterFX.limiter.bypass = !is_on; break;
                {%- endif %}
                {%- if board.masterfx.delay %}
                case 90: masterFX.delay.set_time(val); break;
                case 91: masterFX.delay.target_level = val * 0.8f; break;
                case 92: masterFX.delay.target_fb = val * 0.95f; break;
                case 93: masterFX.delay.bypass = !is_on; break;
                {%- endif %}
                {%- if board.masterfx.reverb %}
                case 2: masterFX.reverb_mix = val; break;      
                case 95: masterFX.reverb.roomsize(val * 0.98f); break; 
                case 96: masterFX.reverb.damping(val); break;      
                case 97: masterFX.reverb.reverb_width = val; break;       
                case 98: masterFX.reverb.reverb_predelay = val; break;    
                case 99: masterFX.reverb_bypass = !is_on; break;
                {%- endif %}
            }
            hv_send_msg3_lock(HV_CTLIN_HASH, (float)data2, (float)data1, (float)chan);
            break;
        }

        case 0xE0: { 
            int bend = (data2 << 7) | data1;
            hv_send_msg2_lock(HV_BENDIN_HASH, (float)bend, (float)chan);
            break;
        }
    }
}


// void handle_midi_message(uint8_t status, uint8_t data1, uint8_t data2) {

//     uint32_t now = to_ms_since_boot(get_absolute_time());

//     if (status >= 0xF8) {
//         hv_sendMessageToReceiverV(&pd_prog, HV_MIDIREALTIMEIN_HASH, 0.0f, "f", (float)status);
//         switch(status) {
//             case MIDI_RT_START:    clock_count = 23; clock_running = true; break;
//             case MIDI_RT_CONTINUE: clock_running = true; break;
//             case MIDI_RT_STOP:     clock_running = false; break;
//             case MIDI_RT_CLOCK:    
//                 if(clock_running && ++clock_count >= 24) {
//                     clock_count = 0;
//                     midi_clock_timer = now + CLOCK_FLASH_MS;
//                 }
//                 break;
//         }
//         return;
//     }

//     uint8_t type = status & 0xF0;
//     uint8_t chan = status & 0x0F;
//     float f_chan = (float)chan + 1.0f; 

//     switch(type) {
//         case 0x90: { 
//             if (data2 > 0) {
//                 last_midi_velocity = data2;
//                 current_midi_note = data1;
//                 midi_activity_timer = now + FLASH_DURATION_MS;
                
//                 hv_sendMessageToReceiverV(&pd_prog, HV_NOTEIN_HASH, 0.0f, "fff", (float)data1, (float)data2, f_chan);

//                 {%- if board.voice_count > 1 %}
//                 {
//                     int v_idx = allocateVoice(data1); 
//                     hv_sendMessageToReceiverV(&pd_prog, VOICE_HASHES[v_idx], 0.0f, "fff", (float)data1, (float)data2, f_chan);
//                 }
//                 {%- endif %}
//                 break; 
//             }
//             [[fallthrough]];
//         }

//         case 0x80: { 
//             hv_sendMessageToReceiverV(&pd_prog, HV_NOTEIN_HASH, 0.0f, "fff", (float)data1, 0.0f, f_chan);

//             {%- if board.voice_count > 1 %}
//             {
//                 int i = findVoiceByNote(data1);
//                 if (i >= 0) {
//                     sendVoiceNoteOff(i, data1);
//                     hv_sendMessageToReceiverV(&pd_prog, VOICE_HASHES[i], 0.0f, "fff", (float)data1, 0.0f, f_chan);
//                 }
//             }
//             {%- endif %}
//             break;
//         }
        
//         case 0xB0: { 
//             float val = (float)data2 / 127.0f;
//             bool is_on = (data2 > 64);
            
//             switch(data1) {
//                 {%- if board.masterfx.limiter %}
//                 case 7: masterFX.master_volume = val; break; 
//                 case 8: masterFX.limiter.bypass = !is_on; break;
//                 {%- endif %}
//                 {%- if board.masterfx.delay %}
//                 case 90: masterFX.delay.set_time(val); break;
//                 case 91: masterFX.delay.target_level = val * 0.8f; break;
//                 case 92: masterFX.delay.target_fb = val * 0.95f; break;
//                 case 93: masterFX.delay.bypass = !is_on; break;
//                 {%- endif %}
//                 {%- if board.masterfx.reverb %}
//                 case 2: masterFX.reverb_mix = val; break;      
//                 case 95: masterFX.reverb.roomsize(val * 0.98f); break; 
//                 case 96: masterFX.reverb.damping(val); break;      
//                 case 97: masterFX.reverb.reverb_width = val; break;       
//                 case 98: masterFX.reverb.reverb_predelay = val; break;    
//                 case 99: masterFX.reverb_bypass = !is_on; break;
//                 {%- endif %}
//             }
//             hv_sendMessageToReceiverV(&pd_prog, HV_CTLIN_HASH, 0.0f, "fff", (float)data2, (float)data1, (float)chan);
//             break;
//         }

//         case 0xE0: { 
//             int bend = (data2 << 7) | data1;
//             hv_sendMessageToReceiverV(&pd_prog, HV_BENDIN_HASH, 0.0f, "ff", (float)bend, (float)chan);
//             break;
//         }
//     }
// }


void heavyMidiOutHook(HeavyContextInterface *c, const char *receiverName, hv_uint32_t receiverHash, const HvMessage *m) {
    uint8_t midiMsg[3] = {0, 0, 0};
    int humanChannel = (hv_msg_getNumElements(m) >= 3) ? (int)hv_msg_getFloat(m, 2) : 1;
    int zeroBasedCh = (humanChannel < 1) ? 0 : (humanChannel > 16) ? 15 : humanChannel - 1;

    if (receiverHash == HV_NOTEOUT_HASH) {
        int note = (int)hv_msg_getFloat(m, 0);
        int vel  = (int)hv_msg_getFloat(m, 1);
        midiMsg[0] = (vel > 0 ? 0x90 : 0x80) | zeroBasedCh;
        midiMsg[1] = note & 0x7F;
        midiMsg[2] = vel & 0x7F;
    } 
    else if (receiverHash == HV_CTL_OUT_HASH) {
        int val = (int)hv_msg_getFloat(m, 0);
        int cc  = (int)hv_msg_getFloat(m, 1);
        midiMsg[0] = 0xB0 | zeroBasedCh;
        midiMsg[1] = cc & 0x7F;
        midiMsg[2] = val & 0x7F;
    } else return;

    {%- if board.midi_mode == 'uart' %}
    uart_write_blocking(uart0, midiMsg, 3);
    {%- endif %}

    {%- if board.midi_mode == 'usb' or board.midi_mode == 'host' %}
    uint32_t status = midiMsg[0];

    uint32_t midi_packed = (status << 16) | (midiMsg[1] << 8) | midiMsg[2];

    uint32_t h = midi_out_rb.head.load(std::memory_order_relaxed);
    uint32_t t = midi_out_rb.tail.load(std::memory_order_acquire);

    if ((h - t) < MIDI_OUT_BUF) {
        midi_out_rb.data[h & (MIDI_OUT_BUF - 1)] = midi_packed;
        midi_out_rb.head.store(h + 1, std::memory_order_release);
    }
    {%- endif %}
}


// void sendHookHandler(HeavyContextInterface *vc, const char *name, uint32_t hash, const HvMessage *m) {
//     int numElem = hv_msg_getNumElements(m);
//     float val0 = hv_msg_getFloat(m, 0);

//     switch (hash) {
//     {% for l in board.leds -%}
//         {%- set led_index = loop.index0 -%}
//         {%- for s in hv_manifest.sends if s.name == l.name -%}
//             case {{ s.hash }}U: // {{ l.name }}
//                 {% if l.is_rgb -%}
//                 if (numElem >= 2) {
//                     Pico::led_hue[{{ led_index }}].store(val0, std::memory_order_relaxed);
//                     Pico::led_intensity[{{ led_index }}].store(hv_msg_getFloat(m, 1), std::memory_order_relaxed);
//                 } else {
//                     Pico::led_intensity[{{ led_index }}].store(val0, std::memory_order_relaxed);
//                 }
//                 {%- else -%}
//                 Pico::led_vals[{{ led_index }}].store(val0, std::memory_order_relaxed);
//                 {%- endif %}
//                 return;
//         {%- endfor -%}
//     {%- endfor %}
//             default:
//                 heavyMidiOutHook(vc, name, hash, m);
//                 break;
//         } 
// }

void sendHookHandler(HeavyContextInterface *vc, const char *name, uint32_t hash, const HvMessage *m) {
    if (hv_msg_getNumElements(m) < 1) return;
    float val0 = hv_msg_getFloat(m, 0);

    switch (hash) {
    /* --- Hardware LEDs --- */
    {% for l in board.leds -%}
        {%- for s in hv_manifest.sends if s.name == l.name -%}
        case {{ s.hash }}U: 
            {% if l.is_rgb -%}
            Pico::led_hue[{{ loop.index0 }}].store(val0, std::memory_order_relaxed);
            if (hv_msg_getNumElements(m) >= 2) 
                Pico::led_intensity[{{ loop.index0 }}].store(hv_msg_getFloat(m, 1), std::memory_order_relaxed);
            {%- else -%}
            Pico::led_vals[{{ loop.index0 }}].store(val0, std::memory_order_relaxed);
            {%- endif %}
            return;
        {%- endfor -%}
    {%- endfor %}

    /* --- Web & OSC Routing --- */
    {% if board.web.enabled -%}
    {% for s in hv_manifest.sends -%}
        {# Check if this specific send was already used by an LED #}
        {%- set is_led = false -%}
        {%- for l in board.leds if l.name == s.name %}{% set is_led = true %}{% endfor -%}
        
        {%- if not is_led -%}
        case {{ s.hash }}U:
            {% if s.name.startswith('web') -%}
            if (web_float_handler) web_float_handler("{{ s.name }}", val0);
            {%- elif s.name.startswith('osc') -%}
            osc_send_float("{{ s.name }}", val0);
            {%- endif %}
            return;
        {%- endif %}
    {% endfor %}
    {%- endif %}

    default:
        heavyMidiOutHook(vc, name, hash, m);
        break;
    } 
}


{% if board.web.enabled -%}
// Global handler pointers defined in PicoWEB.h
osc_hv_float_handler_t osc_hv_handler = nullptr;
web_float_handler_t web_float_handler = nullptr;

static void web_router(const char *p, float v) {
    if (!p) return;
    {% for r in hv_manifest.receives -%}
    {% if r.name.startswith('web') -%}
    if (!strcmp(p, "{{ r.name }}")) {
        hv_sendFloatToReceiver(&pd_prog, {{ r.hash }}, v);
        return;
    }
    {%- endif %}
    {% endfor %}
}

static void hv_osc_router(const char *a, float v) {
    if (!a) return;
    {% for r in hv_manifest.receives -%}
    {% if r.name.startswith('osc') -%}
    if (!strcmp(a, "/{{ r.name }}")) hv_sendFloatToReceiver(&pd_prog, {{ r.hash }}, v);
    {%- endif %}
    {% endfor %}
}
{%- endif %}


{%- if board.console %}
#define NUM_PRINT_NAMES {{ hv_manifest.prints|length }}
PrintMsg print_pool[PRINT_POOL_SIZE];

static const char* printNames[NUM_PRINT_NAMES] = {
{% for p in hv_manifest.prints -%}
    "{{ p.name }}"{% if not loop.last %}, {% endif %}
{% endfor %}
};


static int16_t get_print_id(const char *name) {
    if (!name || name[0] == '\0') return -1; 
    for (uint16_t i = 0; i < NUM_PRINT_NAMES; i++) {
        if (strcmp(name, printNames[i]) == 0) return (int16_t)i;
    }
    return -1;
}


void hv_print_handler(HeavyContextInterface* context,
                      const char* printName,
                      const char* str,
                      const HvMessage* msg) 
{
    uint32_t now = to_ms_since_boot(get_absolute_time());
    static uint32_t last_print = 0;

    if (now - last_print < 50) return;

    for (int i = 0; i < PRINT_POOL_SIZE; ++i) {
        bool expected = false;
        if (print_pool[i].busy.compare_exchange_strong(
                expected, true, std::memory_order_acquire)) 
        {
            print_pool[i].id = get_print_id(printName);
            print_pool[i].val = msg ? hv_msg_getFloat(msg, 0) : 0.f;
            print_pool[i].is_float = (msg != nullptr);

            __dmb();

            if (!multicore_fifo_push_timeout_us(i, 10)) {
                print_pool[i].busy.store(false, std::memory_order_release);
                __dmb();
            } else {
                last_print = now;
            }

            return; 
        }
    }
}

{%- endif %}


void audioFunc(float* buffer, int frames) {
    pd_prog.processInlineInterleaved(buffer, buffer, frames);
}

int main() {
    set_sys_clock_khz({{ board.core_freq }}, true);
    stdio_init_all(); 

 {%- if board.console %}
    #if !defined(WEB_ENABLED) || (WEB_ENABLED == 0)
        #ifndef MIDI_HOST
        cdc_stdio_lib_init();
        #endif
    #endif
    {%- endif %}

 // --- I2C Initialization ---

    {% if board.inputs.sensors.mpr121 -%}
    i2c_init(i2c0, 400 * 1000);
    {% if board.inputs.sensors.mpr121 | selectattr("i2c_bus", "equalto", "i2c1") | list %}
    i2c_init(i2c1, 400 * 1000);
    {% endif %}
    {%- endif %}

 // --------------------------

    {%- if board.pico_board == 'zero' %}
    Pico::init_neopixel();
    {%- endif %}

   {% if board.pico_board == 'pico_w' %}
        {% if board.web.enabled %}
    // Web Mode: Initialize full WiFi stack
    init_wifi();
        {% else %}
    // Standard Mode: Basic wireless initialization (for LED/system)
    cyw43_arch_init();
        {% endif %}
    {% endif %}

   {% if board.midi_mode == 'uart' %}
    uart_midi_init();
    {% elif board.midi_mode in ['usb', 'host'] and not board.web.enabled %}
    usb_init(); 
    {% endif %}

    {% if board.console %}
    pd_prog.setPrintHook(&hv_print_handler);
    {% endif %}
    pd_prog.setSendHook(&sendHookHandler);
    {% if board.audio_mode == "I2S" %}
    Pico::setupAudio(I2S, audioFunc, 
        {{ board.sample_rate }}, 
        {{ board.i2s_data_pin }}, 
        {{ board.i2s_bclk_pin }}, 
        {{ board.buffer_size }});
    {% else %}
    Pico::setupAudio(PWM, audioFunc, 
        {{ board.sample_rate }}, 
        {{ board.pwm_pin }},
        0,  // second pin is unused
        {{ board.buffer_size }});
    {%- endif %}

     masterFX.init();

// ---- MPR121 init ----

{% if board.inputs.sensors.mpr121 -%}
#define NUM_SENSORS {{ board.inputs.sensors.mpr121 | length }}

    Pico::MPR121Config cfg[NUM_SENSORS] = {
    {%- for sensor in board.inputs.sensors.mpr121 %}
        { 
            {{ sensor.i2c_bus }}, 
            {{ sensor.sda }}, 
            {{ sensor.scl }}, 
            {{ sensor.irq }}, 
            {{ sensor.addr_index }} 
        }{{ "," if not loop.last }}
    {%- endfor %}
    };

    Pico::MPR121 sensor_array[NUM_SENSORS] = {
        {%- for i in range(board.inputs.sensors.mpr121|length) %}
        Pico::MPR121(cfg[{{ i }}])
        {%- if not loop.last %},{% endif %}
        {%- endfor %}
    };

    Pico::MPR121::_num_sensors = NUM_SENSORS;
    for (int i = 0; i < NUM_SENSORS; ++i) {
        Pico::MPR121::sensors[i] = &sensor_array[i];
    }

    static uint16_t last_touched_state[NUM_SENSORS] = { 0 };
{%- endif %}

// -----------------------------------

    {%- for btn in active_btns %}
    Pico::addPin({{ loop.index0 }}, {{ btn.pin }}, Pico::{{ btn.mode | upper }});
    {%- endfor %}

    {%- for gate in active_gates %}
    Pico::addPin({{ active_btns|length + loop.index0 }}, {{ gate.pin }}, GATE_IN);
    {%- endfor %}

    {%- for gate_out in active_gate_outs %}
    Pico::addPin({{ active_btns|length + active_gates|length + loop.index0 }}, {{ gate_out.pin }}, GATE_OUT, {{ gate_out.duration }});
    {%- endfor %}

    {%- for knob in active_knobs %}
        {% if knob.type == 'cv_in' %}
    Pico::addCV({{ loop.index0 }}, {{ knob.pin }});
        {% else %}
    Pico::addKnob({{ loop.index0 }}, {{ knob.pin }});
        {% endif %}
    {%- endfor %}

   {%- for led in active_leds %}
        {% if board.pico_board == 'zero' and led.is_rgb %}
    #ifdef PICO_ZERO
    Pico::addRgbLed({{ loop.index0 }}, {{ led.pin }}, 255, 255, 255);
    #endif
    {% else %}
    Pico::addLed({{ loop.index0 }}, {{ led.pin }});
    {% endif %}
    {%- endfor %}
   
    {%- for enc in active_encoders -%}
    Pico::addEncoder({{ loop.index0 }}, {{ enc.a }}, {{ enc.b }});
    {%- endfor %}

    {%- for joy in active_joystick -%}
    Pico::addJoystick({{ joy.id }}, {{ joy.x }}, {{ joy.y }});
    {%- endfor %}

    {%- for cny in active_cny70 %}
    Pico::addCNY70({{ cny.adc_pin }}, 380, 750, 0.6f, 2, {{ loop.index0 }});
    {% endfor %}

    {%- for d in active_dist %}
    Pico::addDistanceSensor({{ d.trig }}, {{ d.echo }});
    {%- endfor %}

    multicore_launch_core1(Pico::core1_audio_entry);

    float val, v; 
    bool send;
    float target_val; 
    int led_idx;

    {% if board.web.enabled -%}
    osc_hv_handler = hv_osc_router;
    web_float_handler = web_router;
    {%- endif %}

    while (true) {

        {% if board.web.enabled -%}
        web_poll(); 
        {%- endif %}

        uint32_t now = to_ms_since_boot(get_absolute_time()); 
        
        midi_task();
        
        #if !defined(MIDI_HOST) && ENABLE_DEBUG
        print_queue(printNames, NUM_PRINT_NAMES, debug_enabled);
    #else
        best_effort_wfe_or_timeout(make_timeout_time_ms(1));
    #endif

        if (now - last_led_tick >= 20) {
            last_led_tick = now;

           Pico::update(now); 

            bool is_active = (now < midi_activity_timer);
            float midi_val = is_active ? ((float)last_midi_velocity / 127.0f) : 0.0f;
            float clock_val = (now < midi_clock_timer) ? 1.0f : 0.0f;

// ---- LED ----

                {% for led in active_leds -%}
                    {%- set idx = loop.index0 -%}
                    
                    {%- if led.is_rgb -%}
                        #ifdef PICO_ZERO
                        {
                            float h = Pico::led_hue[{{ idx }}].load(std::memory_order_relaxed);
                            float i = Pico::led_intensity[{{ idx }}].load(std::memory_order_relaxed);

                            {% if led.mode == 'midi' %}
                                i = midi_val;
                                h = 0.66f; 
                            {% elif led.mode == 'clock' %}
                                i = clock_val;
                                h = 0.66f; 
                            {% elif led.mode == 'status' %}
                                i = 1.0f;
                            {% endif %}

                            Pico::updateRGB({{ idx }}, h, i);
                        }
                        #endif
                    {%- else -%}
                        val = Pico::led_vals[{{ idx }}].load(std::memory_order_relaxed);
                        
                        {%- if led.mode == 'status' %} val = 1.0f; {% endif %}
                        {%- if led.mode == 'midi' %} val = midi_val; {% endif %}
                        {%- if led.mode == 'clock' %} val = clock_val; {% endif %}

                        {%- if board.pico_board == 'pico_w' and (led.pin == 25) -%}
                            cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, val > 0.01f);
                        {%- else -%}
                            Pico::updateLed({{ idx }}, val);
                        {%- endif %}
                    {%- endif %}
                {% endfor %}

                #ifdef PICO_ZERO
                Pico::showRGB(); 
                #endif
            
            {% for btn in active_btns %}

// ---- Buttons & Gates ----

            Pico::processPin({{ loop.index0 }}, val, send); 
            if (send) hv_sendFloatToReceiver(&pd_prog, {{ btn.hash }}, val);
            {%- endfor %}

            {%- for gate in active_gates %}
            Pico::processPin({{ active_btns|length + loop.index0 }}, val, send);
            if (send) hv_sendFloatToReceiver(&pd_prog, {{ gate.hash }}, val);
            {%- endfor %}
    
            {%- for knob in active_knobs %}

// ---- Knobs  ----

            if (Pico::processKnob({{ loop.index0 }}, v)) hv_sendFloatToReceiver(&pd_prog, {{ knob.hash }}, v);
            {%- endfor %}
            {%- for enc in active_encoders %}

// ---- Encoder ----

            if (Pico::processEnc({{ loop.index0 }}, v)) hv_sendFloatToReceiver(&pd_prog, {{ enc.hash }}, v);
            {%- endfor %}
            {%- for joy in active_joystick %}

// ---- Joysticks ----

            {
                float vx = 0.0f, vy = 0.0f;
                bool cX = false, cY = false;
                if (Pico::processJoystick({{ joy.id }}, vx, vy, cX, cY, {{ 'true' if joy.midi_range else 'false' }})) {
                    if (cX) hv_sendFloatToReceiver(&pd_prog, {{ joy.hash_x }}, vx);
                    if (cY) hv_sendFloatToReceiver(&pd_prog, {{ joy.hash_y }}, vy);
                }
            }
            {%- endfor %}

// ---- CNY70 ----

            {%- for cny in active_cny70 %}
            {
                float sensor_out = 0.0f; 
                float raw_val = 0.0f; 
                if (Pico::processCNY70({{ loop.index0 }}, sensor_out, raw_val)) {
                    hv_sendFloatToReceiver(&pd_prog, {{ cny.hash }}, sensor_out);
                //  printf("Sensor %d | Raw: %.1f | Norm: %.3f\n", {{ loop.index0 }}, raw_val, sensor_out);
                }
            }
            {%- endfor %}

// ---- HC-SR04 ----

           {%- for d in active_dist %}
            if (Pico::dist_sensor.changed()) {
                float dist_cm = Pico::dist_sensor.getDistance(); 
                hv_sendFloatToReceiver(&pd_prog, {{ d.hash }}, dist_cm);
             // printf("[Distance] %s | Val: %.2f cm\n", "distance", dist_cm);
            }
            {%- endfor %}


// ---- MPR121 ----

        {% if board.inputs.sensors.mpr121 -%}

        for (int i = 0; i < NUM_SENSORS; ++i) {
            if (!sensor_array[i].initialized()) {
             //   if (sensor_array[i].tryInit()) printf("Sensor #%d initialized!\n", i);
                continue;
            }

            sensor_array[i].processMPR121();
            uint16_t touched = sensor_array[i].getTouched(); 

            if (touched != last_touched_state[i]) {
                for (int p = 0; p < NUM_ACTIVE_MPR_PADS; ++p) {
                    MprPad& pad = active_mpr_pads[p]; 
                    if (pad.sensor_idx == i) {
                        bool isTouched = (touched >> pad.pad_idx) & 0x01;
                        bool wasTouched = (last_touched_state[i] >> pad.pad_idx) & 0x01;
                        if (isTouched != wasTouched) {
                            hv_sendFloatToReceiver(&pd_prog, pad.hash, isTouched ? 1.0f : 0.0f);
                        }
                    }
                }
                last_touched_state[i] = touched;
            }
        }
        {%- endif %}


        } 
        tight_loop_contents();
    } 

    return 0;
}
