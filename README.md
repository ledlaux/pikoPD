# PD → HVCC → Raspberry Pi Pico UF2 Generator 

This project automates building **PD patches** (`.pd`) into a **Raspberry Pi Pico UF2** firmware using **hvcc**, **pico-sdk**, and **picotool**. It is currently a **Proof of Concept**. Core logic is established, but there is a significant amount of coding and testing ahead. Future plan is to add automated arduino project builder option using arduino-cli.

Check compiled binaries for RP2040 in the release section.

## Features

- Converts Pure Data (`.pd`) patches to C code via **hvcc**
- Uses main.cpp as a template
- Set in `board.json`:
  
    - board (pico, pico_w, zero, pico2)
    - core frequency
    - sample rate
    - i2s pins
    - led (pwm, rgb and mode)
    - adc pins (knob, cv_in)
    - rotary encoder 
    - gate in/out (gate or trigger)
    - button (bang, toggle, switch)
    - joystic and range (regular or midi 1-127)
    - midi (uart, usb, host)
      - uart (pins tx 0, rx 1 )
      
- Copies hardware config files into project folder from `/src`
- Builds firmware using **CMake** in a `build/` folder  
- Checks for device in BOOTSEL mode
- Flashes UF2 firmware to PICO board and restarts device

## Project Updates

- [x] serial console
- [x] button
- [x] led
- [x] adc
- [x] uart midi
- [x] usb midi
- [x] usb midi host
- [x] encoder
- [x] joystick
- [x] gate in/out
- [ ] pwm audio
- [ ] screen
- [ ] sensors
- [ ] midi clock
- [ ] usb audio
- [ ] bluetooth midi


### Working state

1. HVCC supported vanilla pd objects should work.
2. Added heavylib object support (hv.osc, hv.lfo ...). 
3. Getting serial console to work with USB MIDI in pico-sdk was tricky, but it now works with [print] objects from  PD. It can get flooded with messages and crash the device, so use it moderately for debugging only. [print] objects are parsed automatically. You can also use the regular console if needed—just uncomment the print lines in the code.
4. Raspberry Pico can't sample audio so [adc] object will not work without an external adc.


## Notes

- The `[send]` and `[receive]` object names in the Pure Data patch **must exactly match** (case-sensitive) the **name** and **category** defined in `board.json`.  Also check for the correct `@hv_param` argument. 
- You can rename sends and receives as you wish. Currently, there is no enforced naming convention.
- To save resources remove unused send and receive objects from the patch.
- You don't need to remove objects from `board.json`, script adds objects which are present in the patch automatically. 
- Make sure to verify the correct pin configuration (e.g., **pin 1 corresponds to GPIO1**) according to the **category** of the object (button, etc.). 
- If you change the board in `board.json`, remove the project folder or rename it in the command to rebuild files.  
- Tested on **macOS**.  
- If something does not work as expected on your system, please open a [GitHub issue](https://github.com/ledlaux/pikoPD/issues).

### Sample loading

Sample loading works despite the limitations (link to tutorial is in the last section). Pico stores sample data into the ram, so to load it to the flash we need to manually set tables to *const* in Heavy_patchname.cpp:

_float table -> const float table_

Then run command with -x flag to skip file rebuilding. 


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
│   └── heavylib
├── patches
├── src
└── templates          
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




