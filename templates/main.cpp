#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "pico/multicore.h"
#include "PicoControl.h"
#include "Heavy_{{ name }}.hpp"

{% if board.pico_board == 'pico_w' %}
#include "pico/cyw43_arch.h"
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


void handle_midi_message(uint8_t status, uint8_t data1, uint8_t data2) {
    // --- 1. Real-time Messages (Clock, Start, Stop) ---
    if (status >= 0xF8) {
        hv_sendMessageToReceiverV(&pd_prog, HV_MIDIREALTIMEIN_HASH, 0.0f, "f", (float)status);

        if (status == 0xFA) { // START
            clock_count = 23; 
            clock_running = true;
            midi_clock_timer = to_ms_since_boot(get_absolute_time()) + CLOCK_FLASH_MS;
        } 
        else if (status == 0xFB) { // CONTINUE
            clock_running = true;
        } 
        else if (status == 0xFC) { // STOP
            clock_running = false;
            midi_clock_timer = 0; 
        } 
        else if (status == 0xF8) { // CLOCK
            if (clock_running) {
                clock_count++;
                if (clock_count >= 24) {
                    clock_count = 0;
                    midi_clock_timer = to_ms_since_boot(get_absolute_time()) + CLOCK_FLASH_MS;
                }
            }
        }
        return; 
    }

    uint8_t type = status & 0xF0;
    uint8_t chan = status & 0x0F;

    // --- 2. Activity Monitoring (LED Flashing) ---
    if (type == 0x90 && data2 > 0) {
        last_midi_velocity = data2;
        current_midi_note = data1;
        midi_activity_timer = to_ms_since_boot(get_absolute_time()) + FLASH_DURATION_MS;
    }

    // --- 3. Message Routing ---
    switch (type) {
        case 0x90: // Note On
            hv_sendMessageToReceiverV(&pd_prog, HV_NOTEIN_HASH, 0.0f, "fff", (float)data1, (float)data2, (float)chan + 1.0f);
            break;

        case 0x80: // Note Off
            hv_sendMessageToReceiverV(&pd_prog, HV_NOTEIN_HASH, 0.0f, "fff", (float)data1, 0.0f, (float)chan + 1.0f);
            break;

        case 0xB0: { // Control Change
            float val = (float)data2 / 127.0f;
            bool is_on = (data2 > 64);
            
            // DSP Parameter Mapping
            switch(data1) {
                case 7:                                          // Master volume
                    Pico::midi_master_volume = val;
                    break;
                case 8: Pico::limiter_bypass = is_on; break;     // Toggle Limiter
                case 90:                                         // Delay Time
                    Pico::target_delay_samples = 500.0f + (val * 23000.0f); 
                    break;
                case 91:                                        // Delay Send Level
                    Pico::delay_level = val * 0.8f; 
                    break;
                case 92:                                        // Feedback Amount
                    Pico::delay_feedback = val * 0.95f; 
                    break;
                case 93: Pico::delay_bypass = is_on; break;     // Toggle Delay
                case 120:                                       // Debug Toggle
                    debug_enabled = (data2 > 64);
                    break;
            }

            hv_sendMessageToReceiverV(&pd_prog, HV_CTLIN_HASH, 0.0f, "fff", (float)data2, (float)data1, (float)chan);
            break;
        }

        case 0xE0: 
            hv_sendMessageToReceiverV(&pd_prog, HV_BENDIN_HASH, 0.0f, "ff", (float)((data2 << 7) | data1), (float)chan);
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

            raw[0] = isNoteOn ? 0x90 : 0x80;
            raw[1] = (uint8_t)note;
            raw[2] = (uint8_t)vel;
            raw_len = 3;

            packet[0] = isNoteOn ? 0x09 : 0x08; 
            packet[1] = raw[0];
            packet[2] = raw[1];
            packet[3] = raw[2];
            break;
        }

        case HV_CTL_OUT_HASH: {
            uint8_t ccNum = (uint8_t)msg_getFloat(m, 0);
            uint8_t ccVal = (uint8_t)msg_getFloat(m, 1);

            raw[0] = 0xB0;
            raw[1] = ccNum;
            raw[2] = ccVal;
            raw_len = 3;

            packet[0] = 0x0B; 
            packet[1] = raw[0];
            packet[2] = raw[1];
            packet[3] = raw[2];
            break;
        }

        default:
            return; 
    }

    {% if board.midi_mode == 'usb' %}
    if (tud_midi_mounted() && packet[0] != 0) {
        tud_midi_packet_write(packet);
    }
    {% endif %}

    {% if board.midi_mode == 'uart' %}
    if (raw_len > 0) {
    uart_write_blocking(uart0, raw, raw_len);
    }
    {% endif %}

    {% if board.midi_mode == 'host' %}
    if (raw_len > 0) {
        tuh_midi_stream_write(1, 0, raw, raw_len);
    }
    {% endif %}
}


void sendHookHandler(HeavyContextInterface *vc, const char *name, uint32_t hash, const HvMessage *m) {
    int numElem = hv_msg_getNumElements(m);
    float val0 = hv_msg_getFloat(m, 0);

    switch (hash) {
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
            default:
                heavyMidiOutHook(vc, name, hash, m);
                break;
        } 
    }


void audioFunc(float* buffer, int frames) {
    pd_prog.processInlineInterleaved(buffer, buffer, frames);

    {% if board.masterfx.delay %}
    Pico::applyStereoDelay(buffer, frames);
    {% endif %}

    {% if board.masterfx.limiter %}
    Pico::applyLimiter(buffer, frames);
    {% endif %}
}


{% if board.console %}
#define PRINT_QUEUE_SIZE 64
#define PRINT_STR_LEN 8
#define PRINT_RATE_MS 150
#define NUM_PRINT_NAMES {{ hv_manifest.prints|length }}

struct PrintMsg {
    int16_t id;
    char str[PRINT_STR_LEN];
};

static PrintMsg queue[PRINT_QUEUE_SIZE];
static uint8_t head = 0;
static uint32_t lastPrintTimes[NUM_PRINT_NAMES] = {0};
static const char* printNames[NUM_PRINT_NAMES] = {
{% for p in hv_manifest.prints -%}
    "{{ p.name }}"{% if not loop.last %}, {% endif %}
{% endfor %}
};

static int16_t get_print_id(const char *name) {
    for (uint16_t i = 0; i < NUM_PRINT_NAMES; i++) {
        if (strcmp(name, printNames[i]) == 0) return (int16_t)i;
    }
    return -1;
}


void hv_print_handler(HeavyContextInterface *context,
                      const char *printName,
                      const char *str,
                      const HvMessage *msg)
{
    if (!str) return;

    int16_t id = get_print_id(printName);
    uint32_t now = to_ms_since_boot(get_absolute_time());

    if (id >= 0) {
        if (now - lastPrintTimes[id] < PRINT_RATE_MS) return;
        lastPrintTimes[id] = now;
    }

    if (!multicore_fifo_wready()) return;

    uint8_t h = head;
    queue[h].id = id;
    
    int i = 0;
    for (; i < PRINT_STR_LEN - 1 && str[i] != '\0'; i++) {
        queue[h].str[i] = str[i];
    }
    queue[h].str[i] = '\0';

    multicore_fifo_push_blocking((uint32_t)&queue[h]);
    
    head = (h + 1) & (PRINT_QUEUE_SIZE - 1);
}


void process_print_queue() {
    if (!tud_cdc_connected()) {
        while (multicore_fifo_rvalid()) {
            multicore_fifo_pop_blocking();
        }
        return;
    }

    while (multicore_fifo_rvalid()) {
        PrintMsg* m = (PrintMsg*)multicore_fifo_pop_blocking();

        if (m->id >= 0 && m->id < NUM_PRINT_NAMES) {
            printf("[%s] %s\n", printNames[m->id], m->str);
        } else {
            printf("[hv] %s\n", m->str);
        }
    }
}
{% endif %}


int main() {
    set_sys_clock_khz({{ board.core_freq }}, true);
    stdio_init_all(); 

    {% if board.console %}
    #ifndef MIDI_HOST
    cdc_stdio_lib_init();
    #endif
    {% endif %}

    {% if board.pico_board == 'zero' %}
    #ifdef PICO_ZERO
    Pico::init_neopixel();
    #endif
    {% endif %}

    {% if board.pico_board == 'pico_w' %}
    cyw43_arch_init();
    {% endif %}

    {% if board.midi_mode == 'uart' %}
    Pico::uart_midi_init();
    {% elif board.midi_mode in ['usb', 'host'] %}
    Pico::usb_init(); 
    {% endif %}

    {% if board.console %}
    pd_prog.setPrintHook(&hv_print_handler);
    {% endif %}
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
        {% if board.pico_board == 'zero' and led.is_rgb %}
    #ifdef PICO_ZERO
    Pico::addRgbLed({{ loop.index0 }}, {{ led.pin }}, 255, 255, 255);
    #endif
    {% else %}
    Pico::addLed({{ loop.index0 }}, {{ led.pin }});
    {% endif %}
    {% endfor %}
   
    {% for enc in active_encoders -%}
    Pico::addEncoder({{ loop.index0 }}, {{ enc.a }}, {{ enc.b }});
    {% endfor %}

    {% for joy in active_joystick -%}
    Pico::addJoystick({{ joy.id }}, {{ joy.x }}, {{ joy.y }});
    {% endfor %}

    {% if board.audio_mode == "I2S" %}
    Pico::setupAudio(Pico::I2S, audioFunc, 
        {{ board.sample_rate }}, 
        {{ board.i2s_data_pin }}, 
        {{ board.i2s_bclk_pin }}, 
        {{ board.buffer_size }});
    {% else %}
    Pico::setupAudio(Pico::PWM, audioFunc, 
        {{ board.sample_rate }}, 
        {{ board.pwm_pin }},
        0,  // second pin is unused
        {{ board.buffer_size }});
    {% endif %}

    multicore_launch_core1(Pico::core1_audio_entry);

    float val, v; 
    bool send;
    float target_val; 
    int led_idx;
 
    while (true) {
        {% if board.midi_mode == 'usb' %}
        tud_task(); 
        {% elif board.midi_mode == 'host' %}
        tuh_task(); 
        {% endif %}

        uint32_t now = to_ms_since_boot(get_absolute_time()); 

        {% if board.console %}
        if (now - last_print_tick >= 10) {
        last_print_tick = now;
        process_print_queue();
        }
        {% endif %}

        Pico::midi_task();

        if (now - last_led_tick >= 10) {
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
            

            // --- Buttons & Gates ---

            {% for btn in active_btns %}
            Pico::processPin({{ loop.index0 }}, val, send); 
            if (send) hv_sendFloatToReceiver(&pd_prog, {{ btn.hash }}, val);
            {% endfor %}

            {% for gate in active_gates %}
            Pico::processPin({{ active_btns|length + loop.index0 }}, val, send);
            if (send) hv_sendFloatToReceiver(&pd_prog, {{ gate.hash }}, val);
            {% endfor %}


            // --- Knobs & Encoders ---

            {% for knob in active_knobs -%}
            if (Pico::processKnob({{ loop.index0 }}, v)) hv_sendFloatToReceiver(&pd_prog, {{ knob.hash }}, v);
            {% endfor %}

            {% for enc in active_encoders -%}
            if (Pico::processEnc({{ loop.index0 }}, v)) hv_sendFloatToReceiver(&pd_prog, {{ enc.hash }}, v);
            {% endfor %}


            // --- Joysticks ---

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
