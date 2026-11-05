#pragma once

#include "pico/stdlib.h"

#include <cstdio>
#include <cstdint>
#include <cstddef>
#include <atomic>
#include <cmath>
#include <cstring>

#include "hardware/uart.h"
#include "hardware/gpio.h"
#include "hardware/irq.h"

#include "hvcc/HvMessage.h"

// ============================================================
// TINYUSB CONFIGURATION
// ============================================================

#if !defined(WEB) || (WEB == 0)

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

        #include "tusb_config.h"
        #include "tusb.h"
        #include "host/usbh.h"
        #include "class/midi/midi_host.h"

    #else

        #include "tusb_config.h"
        #include "tusb.h"

    #endif

#endif


// ============================================================
// CONFIGURATION
// ============================================================

#ifndef ENABLE_DEBUG
    #define ENABLE_DEBUG 0
#endif

#ifndef MAX_VOICES
    #define MAX_VOICES 1
#endif

#define MIDI_IN_BUF  256
#define MIDI_OUT_BUF 256

// MIDI input and output buffers must be powers of two.
static_assert((MIDI_IN_BUF & (MIDI_IN_BUF - 1)) == 0,
              "MIDI_IN_BUF must be a power of two");

static_assert((MIDI_OUT_BUF & (MIDI_OUT_BUF - 1)) == 0,
              "MIDI_OUT_BUF must be a power of two");

// Print queue must have a power-of-two capacity.
#define PRINT_QUEUE_SIZE 256
#define PRINT_QUEUE_MASK (PRINT_QUEUE_SIZE - 1)
#define PRINT_NAME_SIZE  128
#define PRINT_TEXT_SIZE  128
#define PRINT_INTERVAL_MS 40

static_assert((PRINT_QUEUE_SIZE & PRINT_QUEUE_MASK) == 0,
              "PRINT_QUEUE_SIZE must be a power of two");


// ============================================================
// HEAVY INPUT SPSC QUEUE
// ============================================================
//
// Producer: Core 0 only.
// Consumer: Core 1 only.
//
// Core 0:
//   - Parses raw MIDI input.
//   - Enqueues MIDI events.
//   - Enqueues hardware/web/OSC float inputs.
//
// Core 1:
//   - Dequeues events.
//   - Calls handle_midi_message().
//   - Calls pd_prog.sendFloatToReceiver().
//
// Do not push into this queue from an IRQ or Core 1.
// ============================================================

#define HEAVY_INPUT_QUEUE_SIZE 256
#define HEAVY_INPUT_QUEUE_MASK (HEAVY_INPUT_QUEUE_SIZE - 1)

static_assert(
    (HEAVY_INPUT_QUEUE_SIZE & HEAVY_INPUT_QUEUE_MASK) == 0,
    "HEAVY_INPUT_QUEUE_SIZE must be a power of two"
);

enum HeavyInputType : uint8_t {
    HEAVY_INPUT_MIDI,
    HEAVY_INPUT_FLOAT
};

struct HeavyInputEvent {
    HeavyInputType type;

    uint8_t status;
    uint8_t data1;
    uint8_t data2;

    uint32_t hash;
    float value;
};

struct HeavyInputQueue {
    HeavyInputEvent data[HEAVY_INPUT_QUEUE_SIZE];

    // Written only by Core 0.
    std::atomic<uint32_t> head{0};

    // Written only by Core 1.
    std::atomic<uint32_t> tail{0};

    std::atomic<uint32_t> dropped{0};
};

inline HeavyInputQueue heavy_input_rb;


// ------------------------------------------------------------
// Core 0: enqueue MIDI event
// ------------------------------------------------------------

inline bool heavy_input_push_midi(
    uint8_t status,
    uint8_t data1,
    uint8_t data2)
{
    uint32_t head =
        heavy_input_rb.head.load(std::memory_order_relaxed);

    uint32_t tail =
        heavy_input_rb.tail.load(std::memory_order_acquire);

    if (head - tail >= HEAVY_INPUT_QUEUE_SIZE) {
        heavy_input_rb.dropped.fetch_add(
            1,
            std::memory_order_relaxed
        );

        return false;
    }

    HeavyInputEvent& event =
        heavy_input_rb.data[head & HEAVY_INPUT_QUEUE_MASK];

    event.type = HEAVY_INPUT_MIDI;

    event.status = status;
    event.data1 = data1;
    event.data2 = data2;

    event.hash = 0;
    event.value = 0.0f;

    // Publish the completed event.
    heavy_input_rb.head.store(
        head + 1,
        std::memory_order_release
    );

    return true;
}


// ------------------------------------------------------------
// Core 0: enqueue float receiver event
// ------------------------------------------------------------

inline bool heavy_input_push_float(
    uint32_t hash,
    float value)
{
    uint32_t head =
        heavy_input_rb.head.load(std::memory_order_relaxed);

    uint32_t tail =
        heavy_input_rb.tail.load(std::memory_order_acquire);

    if (head - tail >= HEAVY_INPUT_QUEUE_SIZE) {
        heavy_input_rb.dropped.fetch_add(
            1,
            std::memory_order_relaxed
        );

        return false;
    }

    HeavyInputEvent& event =
        heavy_input_rb.data[head & HEAVY_INPUT_QUEUE_MASK];

    event.type = HEAVY_INPUT_FLOAT;

    event.status = 0;
    event.data1 = 0;
    event.data2 = 0;

    event.hash = hash;
    event.value = value;

    // Publish the completed event.
    heavy_input_rb.head.store(
        head + 1,
        std::memory_order_release
    );

    return true;
}


// ------------------------------------------------------------
// Core 1: dequeue Heavy input event
// ------------------------------------------------------------

inline bool heavy_input_pop(HeavyInputEvent& event)
{
    uint32_t tail =
        heavy_input_rb.tail.load(std::memory_order_relaxed);

    uint32_t head =
        heavy_input_rb.head.load(std::memory_order_acquire);

    if (tail == head) {
        return false;
    }

    event = heavy_input_rb.data[
        tail & HEAVY_INPUT_QUEUE_MASK
    ];

    // Release the consumed slot.
    heavy_input_rb.tail.store(
        tail + 1,
        std::memory_order_release
    );

    return true;
}


// ============================================================
// VOICE MANAGEMENT
// ============================================================

struct Voice {
    uint8_t note = 0;
    uint8_t velocity = 0;
    bool active = false;
    uint32_t age = 0;
};

inline Voice voices[MAX_VOICES];
inline uint32_t voiceCounter = 0;


inline int findVoiceByNote(uint8_t note)
{
    for (int i = 0; i < MAX_VOICES; i++) {
        if (voices[i].active && voices[i].note == note) {
            return i;
        }
    }

    return -1;
}


inline void sendVoiceNoteOff(int voiceIndex, uint8_t note)
{
    (void)note;

    if (voiceIndex < 0 || voiceIndex >= MAX_VOICES) {
        return;
    }

    voices[voiceIndex].active = false;
}


inline void sendVoiceNoteOn(
    int voiceIndex,
    uint8_t note,
    uint8_t velocity)
{
    if (voiceIndex < 0 || voiceIndex >= MAX_VOICES) {
        return;
    }

    voices[voiceIndex].note = note;
    voices[voiceIndex].velocity = velocity;
    voices[voiceIndex].active = true;
    voices[voiceIndex].age = voiceCounter++;
}


inline int allocateVoice(uint8_t note)
{
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

    sendVoiceNoteOff(oldestIndex, voices[oldestIndex].note);

    voices[oldestIndex].note = note;
    voices[oldestIndex].active = true;
    voices[oldestIndex].age = voiceCounter++;

    return oldestIndex;
}


// ============================================================
// MIDI INPUT / OUTPUT BUFFERS
// ============================================================

struct MidiInputBuffer {
    uint8_t data[MIDI_IN_BUF];

    // Producer: one active MIDI input source.
    // Consumer: Core 0 MIDI parser.
    std::atomic<uint32_t> head{0};
    std::atomic<uint32_t> tail{0};

    uint32_t available() const
    {
        return head.load(std::memory_order_acquire) -
               tail.load(std::memory_order_acquire);
    }

    bool is_full() const
    {
        return available() >= MIDI_IN_BUF;
    }
};


struct MidiOutputBuffer {
    uint32_t data[MIDI_OUT_BUF];

    // Producer: Core 1 Heavy MIDI output hook.
    // Consumer: Core 0 USB MIDI output task.
    std::atomic<uint32_t> head{0};
    std::atomic<uint32_t> tail{0};

    uint32_t occupied() const
    {
        return head.load(std::memory_order_acquire) -
               tail.load(std::memory_order_relaxed);
    }

    bool is_full() const
    {
        return occupied() >= MIDI_OUT_BUF;
    }
};


inline MidiOutputBuffer midi_out_rb;
inline MidiInputBuffer midi_in_rb;


// ============================================================
// MIDI INPUT RING BUFFER
// ============================================================
//
// Producer: UART IRQ OR USB MIDI callback.
// Consumer: Core 0 MIDI parser.
//
// Important:
// Only one input source should produce into this queue at a
// time. If UART and USB can produce concurrently, serialize
// their calls to midi_push().
// ============================================================

inline void midi_push(uint8_t byte)
{
    uint32_t head =
        midi_in_rb.head.load(std::memory_order_relaxed);

    uint32_t tail =
        midi_in_rb.tail.load(std::memory_order_acquire);

    if ((head - tail) < MIDI_IN_BUF) {

        midi_in_rb.data[head & (MIDI_IN_BUF - 1)] = byte;

        midi_in_rb.head.store(
            head + 1,
            std::memory_order_release
        );
    }
}


inline bool midi_pop(uint8_t& byte)
{
    uint32_t tail =
        midi_in_rb.tail.load(std::memory_order_relaxed);

    uint32_t head =
        midi_in_rb.head.load(std::memory_order_acquire);

    if (tail == head) {
        return false;
    }

    byte = midi_in_rb.data[tail & (MIDI_IN_BUF - 1)];

    midi_in_rb.tail.store(
        tail + 1,
        std::memory_order_release
    );

    return true;
}


// ============================================================
// HEAVY PRINT SPSC QUEUE
// Producer: Core 1
// Consumer: Core 0
// ============================================================

struct PrintMsg {
    char name[PRINT_NAME_SIZE];
    char text[PRINT_TEXT_SIZE];
};

struct PrintQueue {
    PrintMsg data[PRINT_QUEUE_SIZE];

    std::atomic<uint32_t> head{0};
    std::atomic<uint32_t> tail{0};
    std::atomic<uint32_t> dropped{0};
};

inline PrintQueue print_queue_rb;


// ------------------------------------------------------------
// Producer: Core 1
// ------------------------------------------------------------

inline bool print_push(const char* name, const HvMessage* hvMsg)
{
#if ENABLE_DEBUG

    if (!hvMsg) {
        return false;
    }

    uint32_t head =
        print_queue_rb.head.load(std::memory_order_relaxed);

    uint32_t tail =
        print_queue_rb.tail.load(std::memory_order_acquire);

    if (head - tail >= PRINT_QUEUE_SIZE) {
        print_queue_rb.dropped.fetch_add(
            1,
            std::memory_order_relaxed
        );

        return false;
    }

    PrintMsg& msg =
        print_queue_rb.data[head & PRINT_QUEUE_MASK];

    auto copy_string = [](
        char* dst,
        size_t capacity,
        const char* src)
    {
        if (!src) {
            src = "";
        }

        size_t i = 0;

        while (i < capacity - 1 && src[i]) {
            dst[i] = src[i];
            ++i;
        }

        dst[i] = '\0';
    };

    copy_string(msg.name, sizeof(msg.name), name);

    size_t used = 0;
    const int count = msg_getNumElements(hvMsg);

    for (int i = 0;
         i < count && used < sizeof(msg.text) - 1;
         ++i)
    {
        char atom[32];
        int len = 0;

        switch (msg_getType(hvMsg, i)) {

            case HV_MSG_FLOAT:
                len = snprintf(
                    atom,
                    sizeof(atom),
                    "%.3f",
                    (double)msg_getFloat(hvMsg, i)
                );
                break;

            case HV_MSG_SYMBOL:
                len = snprintf(
                    atom,
                    sizeof(atom),
                    "%s",
                    msg_getSymbol(hvMsg, i)
                );
                break;

            case HV_MSG_BANG:
                len = snprintf(
                    atom,
                    sizeof(atom),
                    "bang"
                );
                break;

            case HV_MSG_HASH:
                len = snprintf(
                    atom,
                    sizeof(atom),
                    "0x%08lx",
                    (unsigned long)msg_getHash(hvMsg, i)
                );
                break;

            default:
                continue;
        }

        if (len < 0) {
            continue;
        }

        if (i > 0 && used < sizeof(msg.text) - 1) {
            msg.text[used++] = ' ';
        }

        size_t available = sizeof(msg.text) - 1 - used;

        size_t copyLen =
            (size_t)len < available ? (size_t)len : available;

        memcpy(msg.text + used, atom, copyLen);
        used += copyLen;
    }

    msg.text[used] = '\0';

    print_queue_rb.head.store(
        head + 1,
        std::memory_order_release
    );

    return true;

#else

    (void)name;
    (void)hvMsg;

    return false;

#endif
}


// ------------------------------------------------------------
// Consumer: Core 0
// ------------------------------------------------------------

inline void print_queue(bool debug)
{
#if ENABLE_DEBUG && !defined(MIDI_HOST) && !WEB

    if (!debug || !tud_cdc_connected()) {
        return;
    }

    constexpr uint32_t MAX_MESSAGES = 4;

    for (uint32_t i = 0; i < MAX_MESSAGES; ++i) {

        uint32_t tail =
            print_queue_rb.tail.load(std::memory_order_relaxed);

        uint32_t head =
            print_queue_rb.head.load(std::memory_order_acquire);

        if (tail == head) {
            break;
        }

        const PrintMsg& msg =
            print_queue_rb.data[tail & PRINT_QUEUE_MASK];

        char output[
            PRINT_NAME_SIZE + PRINT_TEXT_SIZE + 4
        ];

        int len = snprintf(
            output,
            sizeof(output),
            "%s: %s\r\n",
            msg.name,
            msg.text
        );

        if (len <= 0 || len >= (int)sizeof(output)) {

            print_queue_rb.tail.store(
                tail + 1,
                std::memory_order_release
            );

            continue;
        }

        uint32_t available = tud_cdc_write_available();

        if (available < (uint32_t)len) {
            break;
        }

        uint32_t written =
            tud_cdc_write(output, (uint32_t)len);

        if (written != (uint32_t)len) {
            break;
        }

        print_queue_rb.tail.store(
            tail + 1,
            std::memory_order_release
        );
    }

    tud_cdc_write_flush();

#else

    (void)debug;

#endif
}


// ============================================================
// MIDI PARSER
// ============================================================

struct MidiParser {
    uint8_t msg[3] = {0, 0, 0};
    int idx = 0;
    int expected = 0;
};

inline MidiParser core0_parser;


inline void parse_raw_midi_byte(
    uint8_t byte,
    MidiParser& p,
    void (*handler)(uint8_t, uint8_t, uint8_t))
{
    // Realtime messages can occur anywhere.
    if (byte >= 0xF8) {
        handler(byte, 0, 0);
        return;
    }

    if (byte & 0x80) {

        p.msg[0] = byte;
        p.idx = 1;

        uint8_t type = byte & 0xF0;

        if (type == 0xC0 || type == 0xD0) {
            p.expected = 2;
        }
        else if (byte == 0xF2) {
            p.expected = 3;
        }
        else if (byte == 0xF1 || byte == 0xF3) {
            p.expected = 2;
        }
        else if (byte < 0xF0) {
            p.expected = 3;
        }
        else {
            p.idx = 0;
            p.expected = 0;
        }
    }
    else if (p.idx > 0 && p.idx < 3) {

        p.msg[p.idx++] = byte;
    }

    if (p.idx != 0 && p.idx == p.expected) {

        handler(
            p.msg[0],
            p.msg[1],
            (p.expected == 3) ? p.msg[2] : 0
        );

        if (p.msg[0] < 0xF0) {

            // Running status.
            p.idx = 1;
        }
        else {

            p.idx = 0;
            p.expected = 0;
        }
    }
}


// ============================================================
// MIDI -> HEAVY QUEUE HANDLER
// ============================================================
//
// Called by the Core 0 MIDI parser.
// It does not call Heavy directly.
// ============================================================

inline void midi_queue_handler(
    uint8_t status,
    uint8_t data1,
    uint8_t data2)
{
    heavy_input_push_midi(
        status,
        data1,
        data2
    );
}


// ============================================================
// USB MIDI DEVICE STATE
// ============================================================

inline int usb_midi_dev0 = -1;


// ============================================================
// USB INITIALIZATION
// ============================================================

#if !defined(WEB) || (WEB == 0)

inline void usb_init()
{
    usb_midi_dev0 = -1;

    #ifdef MIDI_HOST
        tusb_init(0, NULL);
    #else
        tusb_init();
    #endif
}

#endif


// ============================================================
// MIDI TASK
// ============================================================
//
// Call from Core 0.
//
// Processes:
//   1. TinyUSB tasks.
//   2. Incoming raw MIDI bytes.
//   3. Parsed MIDI events -> Heavy SPSC queue.
//   4. Outgoing MIDI events.
//
// IMPORTANT:
// handle_midi_message() is NOT called here.
// It is called by Core 1 after heavy_input_pop().
// ============================================================

void handle_midi_message(
    uint8_t status,
    uint8_t data1,
    uint8_t data2);


inline void midi_task()
{
    // --------------------------------------------------------
    // 1. TinyUSB tasks
    // --------------------------------------------------------

#if !defined(WEB) || (WEB == 0)

    #ifdef MIDI_HOST

        tuh_task();

    #else

        tud_task();

    #endif

#endif


    // --------------------------------------------------------
    // 2. Process incoming MIDI
    // --------------------------------------------------------

    uint8_t b;
    uint32_t count = 0;

    while (count++ < 64 && midi_pop(b)) {

        parse_raw_midi_byte(
            b,
            core0_parser,
            midi_queue_handler
        );
    }


    // --------------------------------------------------------
    // 3. USB MIDI output
    // --------------------------------------------------------

#if !defined(WEB) || (WEB == 0)

    #ifdef MIDI_HOST

        // ----------------------------------------------------
        // USB MIDI HOST OUTPUT
        // ----------------------------------------------------

        if (usb_midi_dev0 != -1 &&
            tuh_midi_mounted(usb_midi_dev0))
        {
            uint32_t head =
                midi_out_rb.head.load(std::memory_order_acquire);

            uint32_t tail =
                midi_out_rb.tail.load(std::memory_order_relaxed);

            uint32_t out_count = 0;

            while (tail != head && out_count++ < 64) {

                uint32_t m =
                    midi_out_rb.data[
                        tail & (MIDI_OUT_BUF - 1)
                    ];

                uint8_t status = (uint8_t)(m >> 16);

                uint8_t pkt[4] = {
                    (uint8_t)(status >> 4),
                    status,
                    (uint8_t)(m >> 8),
                    (uint8_t)m
                };

                if (tuh_midi_packet_write(usb_midi_dev0, pkt)) {

                    midi_out_rb.tail.store(
                        ++tail,
                        std::memory_order_release
                    );
                }
                else {
                    break;
                }
            }

            tuh_midi_write_flush(usb_midi_dev0);
        }

    #else

        // ----------------------------------------------------
        // USB MIDI DEVICE OUTPUT
        // ----------------------------------------------------

        if (tud_midi_mounted()) {

            uint32_t head =
                midi_out_rb.head.load(std::memory_order_acquire);

            uint32_t tail =
                midi_out_rb.tail.load(std::memory_order_relaxed);

            uint32_t out_count = 0;

            while (tail != head && out_count++ < 64) {

                uint32_t m =
                    midi_out_rb.data[
                        tail & (MIDI_OUT_BUF - 1)
                    ];

                uint8_t status = (uint8_t)(m >> 16);

                uint8_t pkt[4] = {
                    (uint8_t)(status >> 4),
                    status,
                    (uint8_t)(m >> 8),
                    (uint8_t)m
                };

                if (tud_midi_packet_write(pkt)) {

                    midi_out_rb.tail.store(
                        ++tail,
                        std::memory_order_release
                    );
                }
                else {
                    break;
                }
            }
        }

    #endif

#endif
}


// ============================================================
// UART MIDI INPUT
// ============================================================

extern "C" inline void on_uart_rx()
{
    while (uart_is_readable(uart0)) {
        midi_push(uart_getc(uart0));
    }
}


inline void uart_midi_init()
{
    uart_deinit(uart0);

    uart_init(uart0, 31250);

    gpio_set_function(0, GPIO_FUNC_UART);
    gpio_set_function(1, GPIO_FUNC_UART);

    uart_set_format(
        uart0,
        8,
        1,
        UART_PARITY_NONE
    );

    uart_set_hw_flow(uart0, false, false);
    uart_set_fifo_enabled(uart0, true);

    irq_set_exclusive_handler(
        UART0_IRQ,
        on_uart_rx
    );

    uart_set_irq_enables(
        uart0,
        true,
        false
    );

    irq_set_enabled(
        UART0_IRQ,
        true
    );
}


// ============================================================
// USB MIDI CALLBACKS
// ============================================================

#if !defined(WEB) || (WEB == 0)

extern "C" {

#ifdef MIDI_HOST

    // --------------------------------------------------------
    // USB MIDI HOST RECEIVE
    // --------------------------------------------------------

    void tuh_midi_rx_cb(uint8_t dev_addr, uint32_t qt)
    {
        (void)qt;

        uint8_t packet[4];

        while (tuh_midi_packet_read(dev_addr, packet)) {

            uint8_t cin = packet[0] & 0x0F;
            uint8_t len = 0;

            switch (cin) {

                case 0x05:
                case 0x0F:
                    len = 1;
                    break;

                case 0x02:
                case 0x06:
                case 0x0C:
                case 0x0D:
                    len = 2;
                    break;

                case 0x03:
                case 0x04:
                case 0x07:
                case 0x08:
                case 0x09:
                case 0x0A:
                case 0x0B:
                case 0x0E:
                    len = 3;
                    break;

                default:
                    continue;
            }

            for (uint8_t i = 0; i < len; i++) {
                midi_push(packet[i + 1]);
            }
        }
    }


    // --------------------------------------------------------
    // USB MIDI HOST MOUNT
    // --------------------------------------------------------

    void tuh_midi_mount_cb(
        uint8_t dev_addr,
        const tuh_midi_mount_cb_t* mount_cb_data)
    {
        (void)mount_cb_data;

        if (usb_midi_dev0 == -1) {
            usb_midi_dev0 = dev_addr;
        }
    }


    // --------------------------------------------------------
    // USB MIDI HOST UNMOUNT
    // --------------------------------------------------------

    void tuh_midi_umount_cb(uint8_t dev_addr)
    {
        if (usb_midi_dev0 == dev_addr) {
            usb_midi_dev0 = -1;
        }
    }

#else

    // --------------------------------------------------------
    // USB MIDI DEVICE RECEIVE
    // --------------------------------------------------------

    void tud_midi_rx_cb(uint8_t itf)
    {
        (void)itf;

        uint8_t packet[4];

        while (tud_midi_available()) {

            if (tud_midi_packet_read(packet)) {

                uint8_t cin = packet[0] & 0x0F;
                uint8_t len = 0;

                switch (cin) {

                    case 0x05:
                    case 0x0F:
                        len = 1;
                        break;

                    case 0x02:
                    case 0x06:
                    case 0x0C:
                    case 0x0D:
                        len = 2;
                        break;

                    case 0x03:
                    case 0x04:
                    case 0x07:
                    case 0x08:
                    case 0x09:
                    case 0x0A:
                    case 0x0B:
                    case 0x0E:
                        len = 3;
                        break;

                    default:
                        break;
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


// ============================================================
// RP2040 ATOMIC STUBS
// ============================================================
// These are not thread-safe implementations. They should not
// be relied upon for synchronization between cores.
// The SPSC queues above use atomic 32-bit head/tail counters
// and do not depend on these stubs.
// ============================================================

#if defined(PICO_PLATFORM) || defined(PICO_RP2040)

extern "C" {

bool __atomic_test_and_set(volatile void* ptr, int memorder)
{
    (void)memorder;

    bool old = *(volatile bool*)ptr;
    *(volatile bool*)ptr = true;

    return old;
}

void __atomic_clear(volatile void* ptr, int memorder)
{
    (void)memorder;

    *(volatile bool*)ptr = false;
}

}

#endif
