

#include <I2S.h>
#include <Adafruit_TinyUSB.h>
#include "Heavy_oscilator.hpp"

// --- Atomic stubs for RP2040 only ---
#if defined(ARDUINO_ARCH_RP2040)
extern "C" bool __atomic_test_and_set(volatile void* ptr, int memorder) {
    (void)memorder;
    bool old = *(volatile bool*)ptr;
    *(volatile bool*)ptr = true;
    return old;
}
extern "C" void __atomic_clear(volatile void* ptr, int memorder) {
    (void)memorder;
    *(volatile bool*)ptr = false;
}
#endif

#define I2S_DATA_PIN  9
#define I2S_BCLK_PIN  10
#define SAMPLE_RATE   44100
#define AUDIO_BLOCK   64
float volume = 0.5f;

float audioBlock[2*AUDIO_BLOCK];
int16_t left[AUDIO_BLOCK];
int16_t right[AUDIO_BLOCK];

// --- Heavy hashes (inputs) ---
#define HV_NOTEIN_HASH       0x67E37CA3
#define HV_CTLIN_HASH        0x41BE0F9C

// --- Heavy hashes (outputs) ---
#define HV_NOTEOUT_HASH      0xD1D4AC2
#define HV_CTL_OUT_HASH      0xE5E2A040

I2S i2s_output(OUTPUT);
Heavy_oscilator pd_prog(SAMPLE_RATE);
Adafruit_USBD_MIDI usb_midi;

void heavyMidiOutHook(HeavyContextInterface *c, const char *name, hv_uint32_t hash, const HvMessage *m) {
    if (!tud_mounted()) return;

    uint8_t ch = (msg_getNumElements(m) >= 3) ? (uint8_t)msg_getFloat(m, 2) & 0x0F : 0;
    uint8_t msg[3] = {0, 0, 0};

    if (hash == 0xD1D4AC2) { // HV_NOTEOUT_HASH
        uint8_t vel = (uint8_t)msg_getFloat(m, 1) & 0x7F;
        msg[0] = (vel > 0 ? 0x90 : 0x80) | ch;
        msg[1] = (uint8_t)msg_getFloat(m, 0) & 0x7F;
        msg[2] = vel;
    } 
    else if (hash == 0xE5E2A040) { // HV_CTL_OUT_HASH
        msg[0] = 0xB0 | ch;
        msg[1] = (uint8_t)msg_getFloat(m, 1) & 0x7F; 
        msg[2] = (uint8_t)msg_getFloat(m, 0) & 0x7F; 
    }

    if (msg[0]) usb_midi.write(msg, 3);
}

void handleUsbMidi() {
    uint8_t packet[4];
    while (tud_midi_packet_read(packet)) {
        uint8_t type = packet[1] & 0xF0;
        float d1 = (float)packet[2], d2 = (float)packet[3];
        
        if (type == 0x90 || type == 0x80) 
            hv_sendMessageToReceiverFF(&pd_prog, 0x67E37CA3, 0.0, d1, (type == 0x90) ? d2 : 0.0f);
        else if (type == 0xB0) 
            hv_sendMessageToReceiverFF(&pd_prog, 0x41BE0F9C, 0.0, d1, d2);
    }
}

void setup() {
    Serial.begin(115200);

    usb_midi.begin();
    while (!TinyUSBDevice.mounted()) delay(10);

    i2s_output.setFrequency(SAMPLE_RATE);
    i2s_output.setDATA(I2S_DATA_PIN);
    i2s_output.setBCLK(I2S_BCLK_PIN);
    i2s_output.setBitsPerSample(16);
    i2s_output.setBuffers(4, AUDIO_BLOCK);
    i2s_output.begin();

    pd_prog.setSendHook(heavyMidiOutHook);

    Serial.println("USB MIDI + Heavy ready!");
}

void loop() {
    handleUsbMidi();

    pd_prog.processInlineInterleaved(audioBlock, audioBlock, AUDIO_BLOCK);

    for(int i=0; i<AUDIO_BLOCK; i++){
        left[i]  = int16_t(audioBlock[2*i]   * 32767.0f * volume);
        right[i] = int16_t(audioBlock[2*i+1] * 32767.0f * volume);
    }

    for(int i=0; i<AUDIO_BLOCK; i++){
        i2s_output.write16(left[i], right[i]);
    }
}
