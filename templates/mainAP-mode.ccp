#include <stdio.h>

#include <string.h>
#include <stdint.h>
#include <atomic>
#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "pico/multicore.h"
#include "picoOSC.h"
#include "PicoControl.h"
#include "Heavy_{{ name }}.hpp"

{%- if board.pico_board == 'pico_w' %}
#include "pico/cyw43_arch.h"
{%- endif %}

#ifdef __cplusplus
extern "C" {
#endif

#include "dhcpserver.h"

#ifdef __cplusplus
}

#endif
#include "lwip/apps/httpd.h"
#include "lwip/apps/mdns.h"
#include "lwip/netif.h"
#include "lwip/apps/fs.h"
#include <stddef.h> 
#include "ssi.h"


// Peer Discovery Variables
static ip_addr_t computer_ip;
static uint16_t computer_port = 9000; 
static bool computer_discovered = false;
static struct udp_pcb* osc_out_pcb = nullptr;


// Define your credentials
#define WIFI_SSID ""
#define WIFI_PASSWORD ""

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

// ---- NOTE receives ----

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

// // ---- UART MIDI ----

//     {%- if board.midi_mode == 'uart' %}
//     uart_write_blocking(uart0, midiMsg, 3);
//     {%- endif %}
//     {%- if board.midi_mode == 'host' %}

// // ---- USB HOST ----

//     tuh_midi_stream_write(1, 0, midiMsg, 3);
//     {%- endif %}

// // ---- USB MIDI ----

//     {%- if board.midi_mode == 'usb' %}
//     uint8_t status = midiMsg[0];
//     uint8_t type   = status & 0xF0;
//     uint8_t cin    = status >> 4; 
//     int len = 3; 
//     if (type == 0xC0 || type == 0xD0) len = 2; 
//     uint32_t midi_packed = ((uint32_t)len << 24) | 
//                            ((uint32_t)status << 16) | 
//                            ((uint32_t)midiMsg[1] << 8) | 
//                            (uint32_t)midiMsg[2];

//     uint32_t h = midi_out_rb.head.load(std::memory_order_relaxed);
//     uint32_t t = midi_out_rb.tail.load(std::memory_order_acquire);

//     if ((h - t) < MIDI_OUT_BUF) {
//         midi_out_rb.data[h & (MIDI_OUT_BUF - 1)] = midi_packed;
//         midi_out_rb.head.store(h + 1, std::memory_order_release);
//     }
// {%- endif %}
}


void sendHookHandler(HeavyContextInterface *vc, const char *name, uint32_t hash, const HvMessage *m) {
    // 1. Determine the value to send
    float valToSend = 0.0f;
    if (hv_msg_getNumElements(m) > 0 && hv_msg_isFloat(m, 0)) {
        valToSend = hv_msg_getFloat(m, 0);
    } 
    // If it's a bang, we'll just send 1.0 to indicate activity
    else if (hv_msg_isBang(m, 0)) {
        valToSend = 1.0f;
    }

    // 2. OSC SENDER
    if (computer_discovered && osc_out_pcb != nullptr) {
        picoosc::OSCMessage msg;
        char address[64];
        
        // Ensure the address starts with / and uses the PD object name
        snprintf(address, sizeof(address), "/%s", name);
        
        msg.addAddress(address);
        msg.add(valToSend);

        struct pbuf* pb = pbuf_alloc(PBUF_TRANSPORT, msg.size(), PBUF_RAM);
        if (pb) {
            std::memcpy(pb->payload, msg.data(), msg.size());
            udp_sendto(osc_out_pcb, pb, &computer_ip, computer_port);
            pbuf_free(pb);
        }
    }

    switch (hash) {
        // Your existing LED logic here
        default:
            heavyMidiOutHook(vc, name, hash, m);
            break;
    }
}


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


const char * cgi_pd_handler(int iIndex, int iNumParams, char *pcParam[], char *pcValue[]) {
    for (int i = 0; i < iNumParams; i++) {
        float raw_val = atof(pcValue[i]);
        
        if (strcmp(pcParam[i], "v") == 0) {
            hv_sendFloatToReceiver(&pd_prog, 0x65400F82, raw_val);
        // } else if (strcmp(pcParam[i], "t") == 0) {
        //     hv_sendFloatToReceiver(&pd_prog, 0x99AABBCC, raw_val);
        // } else if (strcmp(pcParam[i], "s") == 0) {
        //     hv_sendFloatToReceiver(&pd_prog, 0x22FF3344, raw_val);
        // }
    }
    
    return NULL; 
}
}
static const tCGI cgi_handlers[] = {
    { "/update_pd.cgi", cgi_pd_handler },
};
static void osc_internal_callback(void *arg, struct udp_pcb *pcb, struct pbuf *p, const ip_addr_t *addr, u16_t port) {
    // 1. Safety Guard
    if (p == NULL) return;

    // 2. PEER DISCOVERY
    // Using ip_addr_set to safely copy the address structure
    if (!computer_discovered || !ip_addr_cmp(&computer_ip, addr)) {
        ip_addr_set(&computer_ip, addr);
        computer_discovered = true;
        // We force the port to 9000 for the Mac Listener (oscdump)
        computer_port = 9000; 
        printf(">>> OSC Peer Discovered: %s\n", ipaddr_ntoa(addr));
    }

    // 3. RECEIVER LOGIC
    char* payload = (char*)p->payload;
    
    // Check if it's a valid OSC path (starts with /)
    if (p->len > 4 && payload[0] == '/') {
        // Find Type Tags
        int tag_idx = (int)std::strlen(payload) + 1;
        while (tag_idx % 4 != 0 && tag_idx < p->len) tag_idx++; 
        
        if (tag_idx < p->len) {
            char* tags = payload + tag_idx;

            // Only process if it's a float (",f")
            if (tags[0] == ',' && tags[1] == 'f') {
                int data_idx = tag_idx + (int)std::strlen(tags) + 1;
                while (data_idx % 4 != 0 && data_idx < p->len) data_idx++;

                if (data_idx + 4 <= p->len) {
                    uint32_t raw;
                    std::memcpy(&raw, payload + data_idx, 4);
                    
                    // OSC is Big-Endian, Pico is Little-Endian
                    union { uint32_t i; float f; } u;
                    u.i = __builtin_bswap32(raw);
                    float val = u.f;

                    // Route to Heavy/PD (web1 receiver)
                    if (strcmp(payload, "/v") == 0) {
                        hv_sendFloatToReceiver(&pd_prog, 0x65400F82, val);
                    }
                }
            }
        }
    }
    
    // 4. THE ONLY FREE POINT
    // Explicitly free the pbuf provided by the callback
    pbuf_free(p); 
}

int main() {
    set_sys_clock_khz({{ board.core_freq }}, true);
    stdio_init_all(); 

    while (!stdio_usb_connected()) {
        sleep_ms(100);
    }

    // 1. Audio and Pure Data Setup
    pd_prog.setSendHook(&sendHookHandler);
    {% if board.audio_mode == "I2S" %}
    Pico::setupAudio(I2S, audioFunc, {{ board.sample_rate }}, {{ board.i2s_data_pin }}, {{ board.i2s_bclk_pin }}, {{ board.buffer_size }});
    {% else %}
    Pico::setupAudio(PWM, audioFunc, {{ board.sample_rate }}, {{ board.pwm_pin }}, 0, {{ board.buffer_size }});
    {% endif %}

    multicore_launch_core1(Pico::core1_audio_entry);
    
    // // 2. Initialize Wi-Fi Hardware
    // if (cyw43_arch_init_with_country(CYW43_COUNTRY_USA)) {
    //     return -1;
    // }

    // cyw43_arch_enable_sta_mode();

    // // 3. Attempt Connection
    // int connect_result = cyw43_arch_wifi_connect_blocking(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK);
    
    // if (connect_result == 0) {
    //     // --- SUCCESSFUL CONNECTION ---
        
    //     // A. Disable Power Save immediately for stable web traffic
    //     cyw43_wifi_pm(&cyw43_state, cyw43_pm_value(CYW43_NO_POWERSAVE_MODE, 20, 1, 1, 1));

    //     // B. Initialize Web Server Services
    //     // We do this BEFORE mDNS so the HTTP port is ready
    //     httpd_init();
    //     http_set_cgi_handlers(cgi_handlers, 1); 
    //     ssi_init();

    //     // C. Initialize mDNS Responder
    //     // This allows access via http://pikopd.local
    //     mdns_resp_init();
    //     mdns_resp_add_netif(netif_default, "pikopd");
    //     mdns_resp_add_service(netif_default, "piko-control", "_http", DNSSD_PROTO_UDP, 80, NULL, NULL);

    //     osc_out_pcb = udp_new();
    //     static picoosc::OSCServer osc_receiver(8000, osc_internal_callback);
        
    //     // Print status to USB Serial
    //     printf("\n========================================\n");
    //     printf("Wi-Fi Connected!\n");
    //     printf("IP Address: %s\n", ip4addr_ntoa(netif_ip4_addr(netif_default)));
    //     printf("OSC Server listening on port 8000\n");
    //     printf("========================================\n\n");
    

    //     // Optional: Print IP to console for manual fallback
    //     // printf("Web Server active at: %s\n", ip4addr_ntoa(netif_ip4_addr(netif_default)));
    // } else {
    //     // Failed to connect - handle blinky error or retry logic here
    // }


// 2. Initialize Wi-Fi Hardware
if (cyw43_arch_init_with_country(CYW43_COUNTRY_USA)) {
    return -1;
}

// 3. Define the AP Network Parameters
const char *ap_name = "pikoPD_Setup";
const char *password = "password123";
ip4_addr_t gw, mask;
IP4_ADDR(&gw, 192, 168, 4, 1);     // The Pico's address
IP4_ADDR(&mask, 255, 255, 255, 0);

// 4. Enable Access Point Mode
// This starts the hardware radio
cyw43_arch_enable_ap_mode(ap_name, password, CYW43_AUTH_WPA2_AES_PSK);

// 5. Start the DHCP Server
// This is what allows your phone to get an IP and see the website
dhcp_server_t dhcp_server;
dhcp_server_init(&dhcp_server, &gw, &mask);

// --- SUCCESSFUL AP START ---

// A. Power Management (Disable for maximum web responsiveness)
cyw43_wifi_pm(&cyw43_state, cyw43_pm_value(CYW43_NO_POWERSAVE_MODE, 20, 1, 1, 1));

// B. Initialize Web Server Services
httpd_init();
http_set_cgi_handlers(cgi_handlers, 1); 
ssi_init();

// C. Initialize mDNS
// Note: In AP mode, use netif_default (which is now the AP interface)
mdns_resp_init();
mdns_resp_add_netif(netif_default, "pikopd");
mdns_resp_add_service(netif_default, "piko-control", "_http", DNSSD_PROTO_UDP, 80, NULL, NULL);

// D. Initialize OSC
osc_out_pcb = udp_new();
static picoosc::OSCServer osc_receiver(8000, osc_internal_callback);

// Print status
printf("\n========================================\n");
printf("Access Point Active!\n");
printf("SSID: %s\n", ap_name);
printf("Webserver IP: %s\n", ip4addr_ntoa(netif_ip4_addr(netif_default)));
printf("Connect and go to http://192.168.4.1\n");
printf("========================================\n\n");


    // 4. The Background Loop
    while (true) {
        // This is mandatory for the background Wi-Fi stack to process packets
        cyw43_arch_poll();
        
        // Give the processor a tiny break to let internal tasks catch up
        sleep_ms(1);
    } 

    return 0;
}
