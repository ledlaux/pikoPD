#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <atomic>
#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "pico/multicore.h"
#include "PicoControl.h"
#include "Heavy_{{ name }}.hpp"

{%- if board.pico_board == 'pico_w' %}
#include "pico/cyw43_arch.h"
{%- endif %}

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


#ifndef MAX_VOICES
#define MAX_VOICES 1
#endif



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


{%- set active_keypad = [] -%}
{%- if board.inputs.rgb_keypad is defined -%}
    {%- for kp in board.inputs.rgb_keypad if kp.name in sends -%}
        {%- set _ = active_keypad.append({
            'sda': kp.sda,
            'scl': kp.scl,
            'brightness': kp.brightness | default(0.5),
            'name': kp.name
        }) -%}
    {%- endfor -%}
{%- endif -%}


Heavy_{{ name }} pd_prog( {{ board.sample_rate }} );

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

// --- Generated Sequencer Hashes ---
{%- set step_hashes = [] -%}
{%- set trig_hashes = [] -%}

{# 1. Collect SEND hashes for the playhead (step0, step1...) #}
{%- for s in hv_manifest.sends -%}
    {%- if s.name.lower().startswith("step") -%}
        {%- set _ = step_hashes.append(s.hash) -%}
    {%- endif -%}
{%- endfor -%}

{# 2. Collect RECEIVE hashes for the triggers (trigger0, trigger1...) #}
{%- for r in hv_manifest.receives -%}
    {%- if r.name.lower().startswith("trigger") -%}
        {%- set _ = trig_hashes.append(r.hash) -%}
    {%- endif -%}
{%- endfor -%}

// --- Dynamic Sequencer Hashes ---
{% if step_hashes | length > 0 -%}
constexpr uint32_t STEP_HASHES[] = {
{%- for hash in step_hashes %} {{ hash }}U{{ "," if not loop.last }}{% endfor %}
};
{%- else -%}
constexpr uint32_t STEP_HASHES[0] = {}; 
{%- endif %}

{% if trig_hashes | length > 0 -%}
constexpr uint32_t TRIG_HASHES[] = {
{%- for hash in trig_hashes %} {{ hash }}U{{ "," if not loop.last }}{% endfor %}
};
{%- else -%}
constexpr uint32_t TRIG_HASHES[0] = {}; 
{%- endif %}

void handle_midi_message(uint8_t status, uint8_t data1, uint8_t data2) {
    uint32_t now = to_ms_since_boot(get_absolute_time());

    // 1. Real-time MIDI (Clock, Start, Stop)
    if (status >= 0xF8) {
        hv_sendMessageToReceiverV(&pd_prog, HV_MIDIREALTIMEIN_HASH, 0.0f, "f", (float)status);
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

    switch(type) {
        // --- CASE 1: NOTE ON (0x90) ---
        case 0x90:
            if (data2 > 0) {
                last_midi_velocity = data2;
                current_midi_note = data1;
                midi_activity_timer = now + FLASH_DURATION_MS;
                
                // 1. Always send to the standard [notein] receiver
                hv_sendMessageToReceiverV(&pd_prog, HV_NOTEIN_HASH, 0.0f, "fff", (float)data1, (float)data2, (float)chan + 1.0f);

                // 2. Polyphonic Voice Tracking (Only exists if note_receives were found)
                {%- if note_receives|length > 1 %}
                {
                    int v_idx = -1;
                    for (int i = 0; i < MAX_VOICES; i++) {
                        if (!voices[i].active) { v_idx = i; break; }
                    }

                    if (v_idx == -1) { // Steal Oldest
                        uint32_t oldest = UINT32_MAX;
                        for (int i = 0; i < MAX_VOICES; i++) {
                            if (voices[i].age < oldest) { oldest = voices[i].age; v_idx = i; }
                        }
                        hv_sendMessageToReceiverV(&pd_prog, VOICE_HASHES[v_idx], 0.0f, "fff", (float)voices[v_idx].note, 0.0f, (float)(chan+1));
                    }

                    voices[v_idx].note = data1;
                    voices[v_idx].velocity = data2;
                    voices[v_idx].active = true;
                    voices[v_idx].age = voiceCounter++;
                    
                    hv_sendMessageToReceiverV(&pd_prog, VOICE_HASHES[v_idx], 0.0f, "fff", (float)data1, (float)data2, (float)(chan+1));
                }
                {%- endif %}
                break; 
            }
            [[fallthrough]];

        // --- CASE 2: NOTE OFF (0x80) ---
        case 0x80:
            // 1. Always send to the standard [notein] receiver
            hv_sendMessageToReceiverV(&pd_prog, HV_NOTEIN_HASH, 0.0f, "fff", (float)data1, 0.0f, (float)chan + 1.0f);

            // 2. Polyphonic Voice Tracking (Only exists if note_receives were found)
            {%- if note_receives|length > 1 %}
            for (int i = 0; i < MAX_VOICES; i++) {
                if (voices[i].active && voices[i].note == data1) {
                    voices[i].active = false;
                    hv_sendMessageToReceiverV(&pd_prog, VOICE_HASHES[i], 0.0f, "fff", (float)data1, 0.0f, (float)(chan+1));
                    break;
                }
            }
            {%- endif %}
            break;
        
        case 0xB0: { // Control Change
            float val = (float)data2 / 127.0f;
            bool is_on = (data2 > 64);
            switch(data1){
                case 7:   Pico::master_volume = val; break;
                case 8:   Pico::limiter_bypass = is_on; break;
                case 90:  Pico::target_delay_samples = 500.0f + val * 23000.0f; break;
                case 91:  Pico::delay_level = val * 0.8f; break;
                case 92:  Pico::delay_feedback = val * 0.95f; break;
                case 93:  Pico::delay_bypass = is_on; break;
                case 120: debug_enabled = is_on; break;
            }
            hv_sendMessageToReceiverV(&pd_prog, HV_CTLIN_HASH, 0.0f, "fff", (float)data2, (float)data1, (float)chan);
            break;
        }

        case 0xE0: { // Pitch Bend
            int bend = (data2 << 7) | data1;
            hv_sendMessageToReceiverV(&pd_prog, HV_BENDIN_HASH, 0.0f, "ff", (float)bend, (float)chan);
            break;
        }
    }
}


void heavyMidiOutHook(HeavyContextInterface *c, const char *receiverName, hv_uint32_t receiverHash, const HvMessage *m) {
    uint8_t midiMsg[3] = {0, 0, 0};
    int humanChannel = 1;
    if(hv_msg_getNumElements(m) >= 3) humanChannel = (int)hv_msg_getFloat(m, 2);

    int zeroBasedCh = (humanChannel < 1) ? 0 : (humanChannel > 16) ? 15 : humanChannel - 1;

    switch(receiverHash) {
        case HV_NOTEOUT_HASH: {
            int note = (int)hv_msg_getFloat(m, 0);
            int vel  = (int)hv_msg_getFloat(m, 1);
            midiMsg[0] = vel > 0 ? (0x90 | zeroBasedCh) : (0x80 | zeroBasedCh);
            midiMsg[1] = note & 0x7F;
            midiMsg[2] = vel & 0x7F;
            break;
        }
        case HV_CTL_OUT_HASH: {
            int val = (int)hv_msg_getFloat(m, 0);
            int cc  = (int)hv_msg_getFloat(m, 1);
            midiMsg[0] = 0xB0 | zeroBasedCh;
            midiMsg[1] = cc & 0x7F;
            midiMsg[2] = val & 0x7F;
            break;
        }
        default: return;
    }

    {%- if board.midi_mode == 'uart' %}
    // --- UART MIDI ---
    uart_write_blocking(uart0, midiMsg, 3);
    {%- endif %}

    {%- if board.midi_mode == 'host' %}
    // --- USB HOST ---
    tuh_midi_stream_write(1, 0, midiMsg, 3);
    {%- endif %}

    // --- USB MIDI ---
    {%- if board.midi_mode == 'usb' %}
    uint8_t status = midiMsg[0];
    uint8_t type   = status & 0xF0;
    uint8_t cin    = status >> 4; 
    int len = 3; 
    if (type == 0xC0 || type == 0xD0) len = 2; 
    uint32_t midi_packed = ((uint32_t)len << 24) | 
                           ((uint32_t)status << 16) | 
                           ((uint32_t)midiMsg[1] << 8) | 
                           (uint32_t)midiMsg[2];

    uint32_t h = midi_out_rb.head.load(std::memory_order_relaxed);
    uint32_t t = midi_out_rb.tail.load(std::memory_order_acquire);

    if ((h - t) < MIDI_OUT_BUF) {
        midi_out_rb.data[h & (MIDI_OUT_BUF - 1)] = midi_packed;
        midi_out_rb.head.store(h + 1, std::memory_order_release);
    }
{%- endif %}
}


void sendHookHandler(HeavyContextInterface *vc, const char *name, uint32_t hash, const HvMessage *m) {
    int numElem = hv_msg_getNumElements(m);
    float val0 = hv_msg_getFloat(m, 0);

    {% if active_keypad -%}
    // Handle Dynamic Sequencer Steps (Playhead)
    // Using the STEP_HASHES array generated at the top of the file
    for (int i = 0; i < (sizeof(STEP_HASHES)/sizeof(STEP_HASHES[0])); i++) {
        if (hash == STEP_HASHES[i]) {
            Pico::keypad.steps[i] = (int)val0;
            return;
        }
    }
    {%- endif %}
    switch (hash) {
    // LEDs 
    {% for l in board.leds -%}
        {%- set led_index = loop.index0 -%}
        {%- for s in hv_manifest.sends if s.name == l.name -%}
        case {{ s.hash }}U: // {{ l.name }}
            {% if l.is_rgb -%}
            if (numElem >= 2) {
                Pico::led_hue[{{ led_index }}].store(val0, std::memory_order_relaxed);
                Pico::led_intensity[{{ led_index }}].store(hv_msg_getFloat(m, 1), std::memory_order_relaxed);
            } else {
                Pico::led_intensity[{{ led_index }}].store(val0, std::memory_order_relaxed);
            }
            {%- else -%}
            Pico::led_vals[{{ led_index }}].store(val0, std::memory_order_relaxed);
            {%- endif %}
            return;
            {%- endfor -%}
        {%- endfor %}
             
        {% if active_keypad -%}
        {%- for s in hv_manifest.sends if s.name == "keypad" -%}
            case {{ s.hash }}U: // Dynamic hash for "keypad" object
                if (val0 > 0.5f) {
                    Pico::clearKeypad();
                }
                return;
            {%- endfor -%}
            {%- endif %}

        default:
            heavyMidiOutHook(vc, name, hash, m);
            break;
    }
}



{% if active_keypad -%}
void handle_keypad_sequencer(HeavyContextInterface *vc) {
    uint32_t now = to_ms_since_boot(get_absolute_time());
    
    // 1. Hardware Update (LEDs and Buttons)
    Pico::processKeypad(now);

    static int prev_steps[4] = {-1, -1, -1, -1};
    
    size_t num_trigs = sizeof(TRIG_HASHES) / sizeof(uint32_t);
    if (num_trigs == 0) return;

    for (int row = 0; row < (int)num_trigs; row++) {
        int current_step = Pico::keypad.steps[row];
        
        // --- DEBUG: Print only when the step changes ---
        if (current_step != prev_steps[row]) {
            printf("[SEQ] Row %d -> Step %d\n", row, current_step);
            
            prev_steps[row] = current_step;
            
            if (current_step >= 0 && current_step < 4) {
                int pad_idx = (row * 4) + current_step;
                
                // If pad is active in the sequencer, send bang to PD
                if (Pico::keypad.selected[pad_idx]) {
                    // --- DEBUG: Print when a trigger actually fires ---
                    printf("  [HIT] Pad %d Triggered!\n", pad_idx);
                    
                    hv_sendFloatToReceiver(vc, TRIG_HASHES[row], 1.0f);
                }
            }
        }
    }
}
{%- endif %}

{%- if board.console %}
#define NUM_PRINT_NAMES {{ hv_manifest.prints|length }}

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
    {%- if board.masterfx.delay %}
    Pico::applyStereoDelay(buffer, frames);
    {%- endif %}
    {%- if board.masterfx.limiter %}
    Pico::applyLimiter(buffer, frames);
    {%- endif %}
}






int main() {
    set_sys_clock_khz({{ board.core_freq }}, true);
    stdio_init_all(); 

    {%- if board.console %}
    #ifndef MIDI_HOST
    cdc_stdio_lib_init();
    #endif
    {%- endif %}

    {%- if board.pico_board == 'zero' %}
    Pico::init_neopixel();
    {%- endif %}

    {% if board.pico_board == 'pico_w' %}
    cyw43_arch_init();
    {% endif %}
    {% if board.midi_mode == 'uart' %}
    uart_midi_init();
    {% elif board.midi_mode in ['usb', 'host'] %}
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

    
    
  // -------------- Keypad Setup -------------
    {% if active_keypad -%}
    {% for kp in active_keypad -%}
    Pico::addKeypad({{ kp.sda }}, {{ kp.scl }}, {{ kp.brightness }}f);
    {% endfor -%}
    {% endif %}

    // -----------------------------------


    multicore_launch_core1(Pico::core1_audio_entry);

    float val, v; 
    bool send;
    float target_val; 
    int led_idx;
 
    while (true) {

        uint32_t now = to_ms_since_boot(get_absolute_time()); 
        
        midi_task();
        {% if board.midi_mode == 'usb' %}
        process_usb_queue();
        {%- endif %}

        {%- if board.console %}
        print_queue(printNames, NUM_PRINT_NAMES, debug_enabled);
        {% else %}
        sleep_us(50); 
        {%- endif %}
        

        if (now - last_led_tick >= 20) {
            last_led_tick = now;

           Pico::update(now); 

            bool is_active = (now < midi_activity_timer);
            float midi_val = is_active ? ((float)last_midi_velocity / 127.0f) : 0.0f;
            float clock_val = (now < midi_clock_timer) ? 1.0f : 0.0f;

            
             // --- LED ---

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
            // --- Buttons & Gates ---
            Pico::processPin({{ loop.index0 }}, val, send); 
            if (send) hv_sendFloatToReceiver(&pd_prog, {{ btn.hash }}, val);
            {%- endfor %}

            {%- for gate in active_gates %}
            Pico::processPin({{ active_btns|length + loop.index0 }}, val, send);
            if (send) hv_sendFloatToReceiver(&pd_prog, {{ gate.hash }}, val);
            {%- endfor %}
    
            {%- for knob in active_knobs %}
            // --- Knobs  ---
            if (Pico::processKnob({{ loop.index0 }}, v)) hv_sendFloatToReceiver(&pd_prog, {{ knob.hash }}, v);
            {%- endfor %}

            {%- for enc in active_encoders %}
            // --- Encoder ---
            if (Pico::processEnc({{ loop.index0 }}, v)) hv_sendFloatToReceiver(&pd_prog, {{ enc.hash }}, v);
            {%- endfor %}
            
            {%- for joy in active_joystick %}
            // --- Joysticks ---
            {
                float vx = 0.0f, vy = 0.0f;
                bool cX = false, cY = false;
                if (Pico::processJoystick({{ joy.id }}, vx, vy, cX, cY, {{ 'true' if joy.midi_range else 'false' }})) {
                    if (cX) hv_sendFloatToReceiver(&pd_prog, {{ joy.hash_x }}, vx);
                    if (cY) hv_sendFloatToReceiver(&pd_prog, {{ joy.hash_y }}, vy);
                }
            }
            {%- endfor %}

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

            {% if active_keypad %}
            handle_keypad_sequencer(&pd_prog);
            {% endif %}
        } 
        tight_loop_contents();
    } 

    return 0;
}
