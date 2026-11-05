https://github.com/bk40-home/Jteensy4000/blob/main/AudioScopeTap.h

#pragma once
#include <stdint.h>
#include "pico/platform.h" // for __dmb() memory barrier

class AudioScopeTap {
public:
    static constexpr uint16_t RING_LEN = 1024;

    AudioScopeTap() : _writeIdx(0), _rawPeak(0) {}

    // --- CORE 1: Audio Thread ---
    void write(const int16_t* samples, uint16_t count) {
        uint16_t w = _writeIdx;
        int16_t localPeak = _rawPeak;

        for (uint16_t i = 0; i < count; ++i) {
            const int16_t s = samples[i];
            _ring[w] = s;
            w = (w + 1) & (RING_LEN - 1);

            const int16_t absS = (s < 0) ? -s : s;
            if (absS > localPeak) localPeak = absS;
        }

        _rawPeak = localPeak;

        __dmb(); // Ensure ring data is written to RAM before updating index
        _writeIdx = w;
    }

    // --- CORE 0: Screen/UI Thread ---
    void snapshot(int16_t* dst, uint16_t count) {
        if (count > RING_LEN) count = RING_LEN;
        
        // Grab current write index safely
        uint16_t w = _writeIdx;
        __dmb(); 

        uint16_t readIdx = (w + RING_LEN - count) & (RING_LEN - 1);

        for (uint16_t i = 0; i < count; ++i) {
            dst[i] = _ring[readIdx];
            readIdx = (readIdx + 1) & (RING_LEN - 1);
        }
    }

    // --- CORE 0: Peak Meter ---
    int16_t readPeakAndClear() {
        int16_t p = _rawPeak;
        _rawPeak = 0;
        return p; // Returns raw height (0 to 32767)
    }

private:
    volatile uint16_t _writeIdx;
    volatile int16_t  _rawPeak;
    int16_t           _ring[RING_LEN];
};