

# PikoPD - RP2040 I2S Audio Processing

Real-time audio input and output using the Raspberry Pi RP2040, PIO-based I2S, DMA, and [Heavy (hvcc)](https://github.com/Wasted-Audio/hvcc) for DSP processing.  

Due to the complexity of integrating this into the main project branch, particularly because of the tricky PIO, DMA, and clock configurations, it will remain in a separate folder for now.     

> [!WARNING]
 **Highly Experimental**

⚠️ Audio Pop at Startup

The first few audio samples may produce a loud pop or noise burst during startup. Do not turn up the volume immediately after powering on the device. This issue is currently under investigation.  

It works by placing [adc] and [dac] objects inside the PD patch. Different processing can be placed between those, like delay. For the safety don't forget to limit sound volume before outputing.

Build folder contains compiled binary with test.     



## Features

- I2S audio input and output
- PIO and DMA-based audio streaming
- Heavy DSP processing generated with hvcc
- Dual-core processing (Core 0: audio I/O, Core 1: DSP)
- Buffered audio transfer using ring queues

## Hardware

- Raspberry Pi Pico / RP2040 board
- PCM1802 I2S ADC
- I2S DAC

## Configuration

| Setting | Value |
|---|---:|
| System clock | 250 MHz |
| Sample rate | 22,050 Hz |
| Heavy block size | 64 samples |
| Audio buffer | 2,048 words |
| DSP queue | 4 buffers |

## PCM1802 Solder Pads

Solder a bridge between the `FMT0` pad and the `+` pad.

On some faulty PCM1802 modules, this modification may not be sufficient. In that case, connect the `FMT0` pad directly to the module's `3.3V` pin using a wire.

Reference: [PCM1802 breakout board modification](https://www.pjrc.com/pcm1802-breakout-board-needs-hack/).

## Wiring

| PCM1802 | RPi Pico | Notes |
|---|---|---|
| +5V | VBUS | Module power |
| 3.3V | — | Leave unconnected; the module has a built-in voltage regulator |
| GND | GND | Ground |
| SCK | GPIO16 | Master clock |
| BCK | GPIO17 | Input bit clock |
| LRCK | GPIO18 | Input word clock |
| DOUT | GPIO19 | Audio data output |
| FSY | 3V3 | Can connect to Pico 3V3 or module 3.3V |
| POW | 3V3 | Can connect to Pico 3V3 or module 3.3V |

## I2S DAC Wiring

| Signal | GPIO |
|---|---:|
| BCLK | GPIO10 |
| LRCK | GPIO11 |
| DATA | GPIO9 |

## Building

This project uses HVCC compiler, Raspberry Pi Pico SDK and CMake.

To convert .pd file to C code use HVCC compiler not pikoPD script (will be integrated in future):
```bash
hvcc audio.pd -o hvcc_output -n audio
```
And copy output to the hvcc folder.

In the project folder:
```bash
mkdir build
cd build
cmake ..
make -j4
```

Flash the generated UF2 file to your RP2040 board.

## Credits

- [Pico-I2S library by VVatashi](https://github.com/VVatashi/Pico-I2S) - MIT license
- [Heavy / hvcc](https://github.com/Wasted-Audio/hvcc)
- [PCM1802 breakout board modification](https://www.pjrc.com/pcm1802-breakout-board-needs-hack/)
- Raspberry Pi Pico SDK


## MIT License

Copyright (c) 2026 ledlaux.github.com

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

