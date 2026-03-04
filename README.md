# PD → HVCC → Raspberry Pi Pico UF2 Generator 

This project automates building **PD patches** (`.pd`) into a **Raspberry Pi Pico UF2** firmware using **hvcc**, **pico-sdk**, and **picotool**. It is currently a **Proof of Concept**. Core logic is established, but there is a significant amount of coding and testing ahead. Future plan is to add also automated arduino project builder option using arduino-cli.

## Features

- Converts Pure Data (`.pd`) patches to C code via **hvcc**
- Uses main.cpp as a template
- Set in `settings.json`:
  
    - board (pico, pico_w, pico2)
    - core frequency
    - sample rate
    - i2s pins
    - led pins
    - adc pins (knob, cv_in)
    - encoder pins
    - gate in/out pins (gate or trigger mode)
    - button pins and type (bang, toggle, switch)
      
- Copies hardware config files into project folder from `/src`
- Builds firmware using **CMake** in a `build/` folder  
- Check for device in BOOTSEL mode
- Flashes UF2 firmware to PICO board and restarts device

## Notes

- The `[send]` and `[receive]` object names in the Pure Data patch **must exactly match** (case-sensitive) the **name** and **category** defined in `settings.json`.  
- You can rename them as needed; currently, there is no enforced naming convention.  
- Make sure to verify the correct pin configuration in `settings.json` (e.g., **pin 1 corresponds to GPIO1**) according to the **category** of the object (button, etc.).  
- If you change the board from pico to pico2 in `settings.json`, remove the project folder or rename it in the command to rebuild files.  
- Tested on **macOS**.  
- If something does not work as expected on your system, please open a [GitHub issue](https://github.com/ledlaux/pikoPD/issues).

  
## Default Patch

PD patch **heavy.pd** is a simple synthesizer that uses the `[notein]` object and USB MIDI input. 

### LED Control
- Sending MIDI CC1 on channel 1 controls LED brightness.  

### Serial Output
The patch includes three `[print]` objects that send normalized values (`0.0–1.0`) from PD to the serial console:

1. MIDI CC Input – Receives MIDI CC1 and prints the value.  
2. ADC Knob Input (GPIO26) – Reads the analog knob and prints its normalized value.  
3. Encoder – Reads a rotary encoder and prints its incremental position.

> Use the `-s` flag to enable the serial console loading in the terminal after flashing (currently works only on mac).


## Patch 2  

**Monosynth.pd** is a monophonic synthesizer with simple envelope and delay effect. Send CC1 and CC2 to control delay lines.


## Project Updates

- [x] serial console 
- [x] led
- [x] usb midi
- [x] button
- [x] adc
- [x] cv in
- [x] gate in
- [x] gate out
- [x] encoder
- [ ] joystic
- [ ] sensors
- [ ] midi clock
- [ ] uart midi
- [ ] screen
- [ ] pwm audio
- [ ] bluetooth midi
- [ ] usb audio

### Working state

1. HVCC supported vanilla pd objects should work.
2. Added heavylib object support (hv.osc, hv.lfo and other).
3. Getting pico serial console to work together with the usb midi in pico-sdk was tricky, but now it works with [print] objects in PD.

### Sample loading

After some tests sample array loading works with arduino and pico-sdk for pico boards. Pico stores float values into the ram, 
to overcome limitation and load data to the flash we need to manually set tables to const in Heavy_patchname.cpp:

_float table -> const float table_

### What doesn't work ###
- Raspberry PICO boards doesn't have adc to read audio input so [adc] object will not work.

## Requirements

- Python 3.10+
  - jinja2
- arm-none-eabi toolchain
- Heavy compiler (hvcc)
- Raspberry Pi Pico SDK
- pico-extras
- picotool

## Setting up toolchain

[Read manual]( https://github.com/ledlaux/pikoPD/blob/main/docs/manual.md)

```
pico-sdk
└── pico-extras
picotool
pikoPD
├── docs
├── lib
│ └── heavylib
├── patches
└── src
```

## Usage

Enter bootloader mode by holding device boot button

```
python3 pikopd.py patches/heavy.pd project_name 

optional arguments:
  -h, --help           Show help message and exit
  -x, --skip-hvcc      Disable hvcc file regeneration for manual editing
  -f, --flash          Flash UF2 to Pico (BOOTSEL mode required)
  -s, --serial         Open serial console after reboot
  -v, --verbose        Enable verbose debug output
```

## Useful links

- About HVCC compiler  
  https://wasted-audio.github.io/hvcc/
- Supported vanilla objects  
  https://github.com/Wasted-Audio/hvcc/blob/develop/docs/reference/objects/supported.md
- Tutorial of how to load samples into the pd patch for the HVCC compiler  
  https://www.youtube.com/watch?v=0qgkYWsYdTo




