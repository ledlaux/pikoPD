#pragma once

#include "pico/stdlib.h"
#include <cstdio>
#include <atomic>
#include <cmath>

#if defined(WEB) && (WEB == 1)
#else
    #ifdef MIDI_HOST
        #ifndef CFG_TUH_ENABLED
            #define CFG_TUH_ENABLED 1
        #endif
        #ifndef CFG_TUH_MIDI
            #define CFG_TUH_MIDI 1
        #endif
        #ifndef BOARD_TUH_RHPORT
            #define BOARD_TUH_RHPORT 0
        #endif
        
        #include "usb/tusb_config.h"
        #include "tusb.h"
        #include "host/usbh.h"
        #include "class/midi/midi_host.h"
    #else
        #include "usb/tusb_config.h"
        #include "tusb.h"
        #include "usb/cdc_stdio_lib.h" 
    #endif
#endif

#ifndef ENABLE_DEBUG
    #define ENABLE_DEBUG 0 
#endif

#ifndef MAX_VOICES
    #define MAX_VOICES 1  
#endif
    
#define MIDI_IN_BUF 256
#define MIDI_OUT_BUF 256
#define PRINT_POOL_SIZE 64


struct Voice {
    uint8_t note;
    uint8_t velocity;   
    bool active;
    uint32_t age;
};


struct MidiInputBuffer {
    uint8_t data[MIDI_IN_BUF];
    std::atomic<uint32_t> head{0};
    std::atomic<uint32_t> tail{0};

    uint32_t available() const {
        return head.load() - tail.load();
    }

    bool is_full() const {
        return available() >= MIDI_IN_BUF;
    }
};

    
struct MidiOutputBuffer {
    uint32_t data[MIDI_OUT_BUF]; 
    std::atomic<uint32_t> head{0};
    std::atomic<uint32_t> tail{0};

    uint32_t occupied() const {
        return head.load(std::memory_order_acquire) - tail.load(std::memory_order_relaxed);
        }

    bool is_full() const {
        return occupied() >= MIDI_OUT_BUF;
        }
};


struct PrintMsg {
    std::atomic<bool> busy{false}; 
    int16_t id;
    float val;      
    bool is_float;  
};


void handle_midi_message(uint8_t status, uint8_t data1, uint8_t data2);
void print_queue(const char** names, int num_names, bool debug);
   
inline Voice voices[MAX_VOICES];
inline uint32_t voiceCounter = 0;
extern PrintMsg print_pool[PRINT_POOL_SIZE];


inline int findVoiceByNote(uint8_t note) {
    for (int i = 0; i < MAX_VOICES; i++) {
        if (voices[i].active && voices[i].note == note) {
            return i;
        }
    }
    return -1;
}


inline void sendVoiceNoteOff(int voiceIndex, uint8_t note) {
    if (voiceIndex < 0 || voiceIndex >= MAX_VOICES) return;
    voices[voiceIndex].active = false;
}


inline void sendVoiceNoteOn(int voiceIndex, uint8_t note, uint8_t velocity) {
    if (voiceIndex < 0 || voiceIndex >= MAX_VOICES) return;

    voices[voiceIndex].note = note;
    voices[voiceIndex].velocity = velocity;
    voices[voiceIndex].active = true;
    voices[voiceIndex].age = voiceCounter++;
}


inline int allocateVoice(uint8_t note) {

        int existing = findVoiceByNote(note);
        if (existing >= 0) {
            voices[existing].age = voiceCounter++;
            return existing;
        }

        int oldestIndex = 0;
        uint32_t oldestAge = UINT32_MAX;

        for (int i = 0; i < MAX_VOICES; i++) {

            if (!voices[i].active) {
                voices[i].note = note;
                voices[i].active = true;
                voices[i].age = voiceCounter++;
                return i;
            }

            if (voices[i].age < oldestAge) {
                oldestAge = voices[i].age;
                oldestIndex = i;
            }
        }

        int v = oldestIndex;

        sendVoiceNoteOff(v, voices[v].note);

        voices[v].note = note;
        voices[v].active = true;
        voices[v].age = voiceCounter++;

        return v;
    }


// ----------- MIDI ------------
    MidiOutputBuffer midi_out_rb;
    MidiInputBuffer  midi_in_rb;

    void midi_push(uint8_t byte) {
        uint32_t h = midi_in_rb.head.load(std::memory_order_relaxed);
        uint32_t t = midi_in_rb.tail.load(std::memory_order_acquire);
        if ((h - t) < MIDI_IN_BUF) {
            midi_in_rb.data[h & (MIDI_IN_BUF - 1)] = byte;
            midi_in_rb.head.store(h + 1, std::memory_order_release);
        }
    }


    bool midi_pop(uint8_t &byte) {
        uint32_t t = midi_in_rb.tail.load(std::memory_order_relaxed);
        uint32_t h = midi_in_rb.head.load(std::memory_order_acquire);
        if (t == h) return false; 
        byte = midi_in_rb.data[t & (MIDI_IN_BUF - 1)];
        midi_in_rb.tail.store(t + 1, std::memory_order_release);
        return true;
    }


   struct MidiParser {
    uint8_t msg[3];
    int idx = 0;
    int expected = 0;
};

static MidiParser core0_parser;

void parse_raw_midi_byte(uint8_t byte, MidiParser& p, void (*handler)(uint8_t, uint8_t, uint8_t)) {
    if (byte >= 0xF8) { 
        handler(byte, 0, 0);
        return;
    }

    if (byte & 0x80) { 
        p.msg[0] = byte; 
        p.idx = 1;
        uint8_t type = byte & 0xF0;
        if (type == 0xC0 || type == 0xD0) p.expected = 2; 
        else if (byte == 0xF2) p.expected = 3;          
        else if (byte == 0xF1 || byte == 0xF3) p.expected = 2; 
        else if (byte < 0xF0) p.expected = 3;            
        else { p.idx = 0; p.expected = 0; }
    } 
    else if (p.idx > 0 && p.idx < 3) { 
        p.msg[p.idx++] = byte;
    }

    if (p.idx != 0 && p.idx == p.expected) {
        handler(p.msg[0], p.msg[1], (p.expected == 3) ? p.msg[2] : 0);
        if (p.msg[0] < 0xF0) {
            p.idx = 1; // Running status
        } else {
            p.idx = 0;
            p.expected = 0;
        }
    }
}

    static int usb_midi_dev0 = -1;

    #if !defined(WEB) || (WEB == 0)

    void usb_init() {
   
        usb_midi_dev0 = -1;

        #ifdef MIDI_HOST
            tusb_init(0, NULL);
        #else
            tusb_init(); 
        #endif
    }
    #endif

void midi_task() {
    // 1. Process TinyUSB Tasks (Only if Web is OFF)
    #if !defined(WEB) || (WEB == 0)
        #ifdef MIDI_HOST
            tuh_task(); 
        #else
            tud_task();  
        #endif
    #endif

    // 2. Process incoming MIDI (Shared: works for UART, USB, and Web/OSC)
    uint8_t b;
    uint32_t count = 0;
    while (count++ < 64 && midi_pop(b)) {
        parse_raw_midi_byte(b, core0_parser, handle_midi_message);
    }

    // 3. USB Output Logic (Only if Web is OFF)
    #if !defined(WEB) || (WEB == 0)
        #ifdef MIDI_HOST
            // --- HOST MODE OUTPUT ---
            if (usb_midi_dev0 != -1 && tuh_midi_mounted(usb_midi_dev0)) {
                uint32_t h = midi_out_rb.head.load(std::memory_order_acquire);
                uint32_t t = midi_out_rb.tail.load(std::memory_order_relaxed);
                uint32_t out_count = 0;

                while (t != h && out_count++ < 64) {
                    uint32_t m = midi_out_rb.data[t & (MIDI_OUT_BUF - 1)];
                    uint8_t status = (uint8_t)(m >> 16);
                    uint8_t pkt[4] = { (uint8_t)(status >> 4), status, (uint8_t)(m >> 8), (uint8_t)m };

                    if (tuh_midi_packet_write(usb_midi_dev0, pkt)) {
                        midi_out_rb.tail.store(++t, std::memory_order_release);
                    } else break;
                }
                tuh_midi_write_flush(usb_midi_dev0);
            }
        #else
            // --- DEVICE MODE OUTPUT ---
            if (tud_midi_mounted()) {
                uint32_t h = midi_out_rb.head.load(std::memory_order_acquire);
                uint32_t t = midi_out_rb.tail.load(std::memory_order_relaxed);
                uint32_t out_count = 0;

                while (t != h && out_count++ < 64) {
                    uint32_t m = midi_out_rb.data[t & (MIDI_OUT_BUF - 1)];
                    uint8_t status = (uint8_t)(m >> 16);
                    uint8_t pkt[4] = { (uint8_t)(status >> 4), status, (uint8_t)(m >> 8), (uint8_t)m };

                    if (tud_midi_packet_write(pkt)) {
                        midi_out_rb.tail.store(++t, std::memory_order_release);
                    } else break;
                }
            }
        #endif
    #endif
} // Function always closes correctly now

    extern "C" void on_uart_rx() {
        while (uart_is_readable(uart0)) {
            midi_push(uart_getc(uart0));
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


    
// ----------- PRINT-----------
   
#ifndef MIDI_HOST
    void print_queue(const char** names, int num_names, bool debug) {
        constexpr int MAX_PRINTS_PER_CALL = 8;

        for (int i = 0; i < MAX_PRINTS_PER_CALL; ++i) {
            if (!multicore_fifo_rvalid()) break;

            uint32_t idx;
            if (!multicore_fifo_pop_timeout_us(0, &idx)) break;
            if (idx >= PRINT_POOL_SIZE) continue;

            PrintMsg* m = &print_pool[idx];
            #if !defined(WEB_ENABLED) || (WEB_ENABLED == 0)
            #if ENABLE_DEBUG
            if (tud_cdc_connected() && debug) {
                const char* name = (m->id >= 0 && m->id < num_names)
                                ? names[m->id]
                                : "print";

                if (m->is_float) {
                    printf("[%s] %.3f\n", name, m->val);
                }
            }
            #endif
            #endif
            m->busy.store(false, std::memory_order_release);
        } 
    } 
    
#endif

#if !defined(WEB) || (WEB == 0)

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
            default: continue; 
        }

        for (uint8_t i = 0; i < len; i++) {
            midi_push(packet[i + 1]); 
        }
    }
}

    void tuh_midi_mount_cb(uint8_t dev_addr, const tuh_midi_mount_cb_t *mount_cb_data) {
        (void)mount_cb_data;
        
        if (usb_midi_dev0 == -1) {
            usb_midi_dev0 = dev_addr;
        }
    }

    void tuh_midi_umount_cb(uint8_t dev_addr) {
        if (usb_midi_dev0 == dev_addr) {
            usb_midi_dev0 = -1;
        }
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
                    midi_push(packet[i + 1]);
                }
            }
        }
    }
#endif

}
#endif

// This is not thread safe but atleast works for RP2040

#if defined(PICO_PLATFORM) || defined(PICO_RP2040)

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

} 

#endif





