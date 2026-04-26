
/*  Updated freeverb algorhitm from the ported PJRC Teensy Audio library to Pico 2.
 *   https://github.com/ghostintranslation/pico-audio
 *
 *   This version adds:
 *   1. Pre-Delay Buffer
 *   2. High-Pass Input Filter (hpL_state)
 *   3. Post-Reverb Low-Pass Filter (lp_wetL/R) to smooth mettalic ringing
 *   4. Mathematical "Rounding":
 *      Adding a rounding bit (`n += (1 << (rshift - 1))`) during bit-shifting which 
 *      keeps the tail smooth and silent as it fades to zero.
 *   5. Softer Allpass Coefficients (feedback gain from 0.5 to 0.45) to break the resonance peaks 
 *   6. Parameter Smoothing (smoothed_predelay)
 *   7. Stereo Width Control
 *
 *
 * Audio Library for Teensy 3.X
 * Copyright (c) 2018, Paul Stoffregen, paul@pjrc.com
 *
 * Development of this audio library was funded by PJRC.COM, LLC by sales of
 * Teensy and Audio Adaptor boards.  Please support PJRC's efforts to develop
 * open source software by purchasing Teensy or other PJRC products.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice, development funding notice, and this permission
 * notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

// A fixed point implementation of Freeverb by Jezar at Dreampoint
//  http://blog.bjornroche.com/2012/06/freeverb-original-public-domain-code-by.html
//  https://music.columbia.edu/pipermail/music-dsp/2001-October/045433.html

#pragma once
#include <stdint.h>
#include <string.h>
#include <algorithm>

#define PREDELAY_MAX 4800

class FreeverbStereo {
public:
    FreeverbStereo() 
        : reverb_level(0.0f), reverb_width(0.5f), reverb_predelay(0.0f),
          smoothed_predelay(0.0f), hpL_state(0.0f), hpR_state(0.0f),
          lp_wetL(0.0f), lp_wetR(0.0f), pre_ptr(0),
          comb1indexL(0), comb2indexL(0), comb3indexL(0), comb4indexL(0),
          comb5indexL(0), comb6indexL(0), comb7indexL(0), comb8indexL(0),
          comb1indexR(0), comb2indexR(0), comb3indexR(0), comb4indexR(0),
          comb5indexR(0), comb6indexR(0), comb7indexR(0), comb8indexR(0),
          ap1idxL(0), ap2idxL(0), ap3idxL(0), ap4idxL(0),
          ap1idxR(0), ap2idxR(0), ap3idxR(0), ap4idxR(0)
    {
        memset(pre_buffer_L, 0, sizeof(pre_buffer_L));
        memset(pre_buffer_R, 0, sizeof(pre_buffer_R));
        clear_buffers();
        combdamp1 = 6553; 
        combdamp2 = 26215; 
        combfeeback = 27524;
    }

    void init() {

        clear_buffers();
        memset(pre_buffer_L, 0, sizeof(pre_buffer_L));
        memset(pre_buffer_R, 0, sizeof(pre_buffer_R));
        pre_ptr = 0;
        }

    void roomsize(float n) {
        n = std::clamp(n, 0.0f, 1.0f);
        combfeeback = (int16_t)(n * 12000.0f) + 20000;
    }

    void damping(float n) {
        n = std::clamp(n, 0.0f, 1.0f);
        int x1 = (int)(n * 16384.0f);
        combdamp1 = (int16_t)x1;
        combdamp2 = (int16_t)(32768 - x1);
    }

    void Process(float inL, float inR, float& outL, float& outR) {
        smoothed_predelay += 0.001f * (reverb_predelay - smoothed_predelay);
        
        hpL_state += 0.05f * (inL - hpL_state);
        float cleanL = inL - hpL_state;
        hpR_state += 0.05f * (inR - hpR_state);
        float cleanR = inR - hpR_state;

        pre_buffer_L[pre_ptr] = cleanL;
        pre_buffer_R[pre_ptr] = cleanR;
        int delay_samples = (int)(smoothed_predelay * (PREDELAY_MAX - 1));
        int read_ptr = (pre_ptr - delay_samples + PREDELAY_MAX) % PREDELAY_MAX;
        
        float dL = pre_buffer_L[read_ptr];
        float dR = pre_buffer_R[read_ptr];
        if (++pre_ptr >= PREDELAY_MAX) pre_ptr = 0;

        int16_t iL = (int16_t)(dL * 12000.0f);
        int16_t iR = (int16_t)(dR * 12000.0f);
        int32_t sL = 0, sR = 0;

        auto comb = [&](int16_t input, int16_t* buffer, uint16_t& index, int16_t& filter, uint16_t size, int32_t& sum) {
            int16_t output = buffer[index];
            sum += output;
            filter = (int16_t)((output * combdamp2 + filter * combdamp1) >> 15);
            buffer[index] = sat16(input + ((filter * combfeeback) >> 15), 0);
            if (++index >= size) index = 0;
        };

        comb(iL, comb1bufL, comb1indexL, comb1filterL, 1116, sL);
        comb(iL, comb2bufL, comb2indexL, comb2filterL, 1188, sL);
        comb(iL, comb3bufL, comb3indexL, comb3filterL, 1277, sL);
        comb(iL, comb4bufL, comb4indexL, comb4filterL, 1356, sL);
        comb(iL, comb5bufL, comb5indexL, comb5filterL, 1422, sL);
        comb(iL, comb6bufL, comb6indexL, comb6filterL, 1491, sL);
        comb(iL, comb7bufL, comb7indexL, comb7filterL, 1557, sL);
        comb(iL, comb8bufL, comb8indexL, comb8filterL, 1617, sL);

        comb(iR, comb1bufR, comb1indexR, comb1filterR, 1139, sR);
        comb(iR, comb2bufR, comb2indexR, comb2filterR, 1211, sR);
        comb(iR, comb3bufR, comb3indexR, comb3filterR, 1300, sR);
        comb(iR, comb4bufR, comb4indexR, comb4filterR, 1379, sR);
        comb(iR, comb5bufR, comb5indexR, comb5filterR, 1445, sR);
        comb(iR, comb6bufR, comb6indexR, comb6filterR, 1514, sR);
        comb(iR, comb7bufR, comb7indexR, comb7filterR, 1580, sR);
        comb(iR, comb8bufR, comb8indexR, comb8filterR, 1640, sR);

        int16_t rL = sat16(sL, 3);
        int16_t rR = sat16(sR, 3);

        auto allpass = [&](int16_t& v, int16_t* buffer, uint16_t& index, uint16_t size) {
            int16_t buf_out = buffer[index];
            int16_t ff = (int16_t)((v * 14745) >> 15);
            int16_t fb = (int16_t)((buf_out * 14745) >> 15);
            buffer[index] = sat16(v + fb, 0);
            v = sat16(buf_out - ff, 0);
            if (++index >= size) index = 0;
        };

        allpass(rL, ap1bufL, ap1idxL, 556); allpass(rL, ap2bufL, ap2idxL, 441);
        allpass(rL, ap3bufL, ap3idxL, 341); allpass(rL, ap4bufL, ap4idxL, 225);
        allpass(rR, ap1bufR, ap1idxR, 579); allpass(rR, ap2bufR, ap2idxR, 464);
        allpass(rR, ap3bufR, ap3idxR, 364); allpass(rR, ap4bufR, ap4idxR, 248);

        float wetL = (float)rL / 32767.0f;
        float wetR = (float)rR / 32767.0f;
        lp_wetL += 0.2f * (wetL - lp_wetL);
        lp_wetR += 0.2f * (wetR - lp_wetR);

        float mono = (lp_wetL + lp_wetR) * 0.5f;
        outL = (mono + (lp_wetL - mono) * reverb_width);
        outR = (mono + (lp_wetR - mono) * reverb_width);
    }

    volatile float reverb_level, reverb_width, reverb_predelay;

private:
    static inline int16_t sat16(int32_t n, int rshift) {
        if (rshift > 0) { n += (1 << (rshift - 1)); n >>= rshift; }
        return (n > 32767) ? 32767 : (n < -32768) ? -32768 : (int16_t)n;
    }

    void clear_buffers() {
        memset(comb1bufL, 0, sizeof(comb1bufL)); memset(comb2bufL, 0, sizeof(comb2bufL));
        memset(comb3bufL, 0, sizeof(comb3bufL)); memset(comb4bufL, 0, sizeof(comb4bufL));
        memset(comb5bufL, 0, sizeof(comb5bufL)); memset(comb6bufL, 0, sizeof(comb6bufL));
        memset(comb7bufL, 0, sizeof(comb7bufL)); memset(comb8bufL, 0, sizeof(comb8bufL));
        memset(comb1bufR, 0, sizeof(comb1bufR)); memset(comb2bufR, 0, sizeof(comb2bufR));
        memset(comb3bufR, 0, sizeof(comb3bufR)); memset(comb4bufR, 0, sizeof(comb4bufR));
        memset(comb5bufR, 0, sizeof(comb5bufR)); memset(comb6bufR, 0, sizeof(comb6bufR));
        memset(comb7bufR, 0, sizeof(comb7bufR)); memset(comb8bufR, 0, sizeof(comb8bufR));
        memset(ap1bufL, 0, sizeof(ap1bufL)); memset(ap2bufL, 0, sizeof(ap2bufL));
        memset(ap3bufL, 0, sizeof(ap3bufL)); memset(ap4bufL, 0, sizeof(ap4bufL));
        memset(ap1bufR, 0, sizeof(ap1bufR)); memset(ap2bufR, 0, sizeof(ap2bufR));
        memset(ap3bufR, 0, sizeof(ap3bufR)); memset(ap4bufR, 0, sizeof(ap4bufR));
    }

    float smoothed_predelay, hpL_state, hpR_state, lp_wetL, lp_wetR;
    float pre_buffer_L[PREDELAY_MAX], pre_buffer_R[PREDELAY_MAX];
    uint16_t pre_ptr;
    int16_t comb1bufL[1116], comb2bufL[1188], comb3bufL[1277], comb4bufL[1356], comb5bufL[1422], comb6bufL[1491], comb7bufL[1557], comb8bufL[1617];
    int16_t comb1bufR[1139], comb2bufR[1211], comb3bufR[1300], comb4bufR[1379], comb5bufR[1445], comb6bufR[1514], comb7bufR[1580], comb8bufR[1640];
    uint16_t comb1indexL, comb2indexL, comb3indexL, comb4indexL, comb5indexL, comb6indexL, comb7indexL, comb8indexL;
    uint16_t comb1indexR, comb2indexR, comb3indexR, comb4indexR, comb5indexR, comb6indexR, comb7indexR, comb8indexR;
    int16_t comb1filterL, comb2filterL, comb3filterL, comb4filterL, comb5filterL, comb6filterL, comb7filterL, comb8filterL;
    int16_t comb1filterR, comb2filterR, comb3filterR, comb4filterR, comb5filterR, comb6filterR, comb7filterR, comb8filterR;
    int16_t ap1bufL[556], ap2bufL[441], ap3bufL[341], ap4bufL[225], ap1bufR[579], ap2bufR[464], ap3bufR[364], ap4bufR[248];
    uint16_t ap1idxL, ap2idxL, ap3idxL, ap4idxL, ap1idxR, ap2idxR, ap3idxR, ap4idxR;
    int16_t combdamp1, combdamp2, combfeeback;
};