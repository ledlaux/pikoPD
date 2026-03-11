# PD → HVCC → Raspberry Pi Pico UF2 Generator 

This project automates building **PD patches** (`.pd`) into a **Raspberry Pi Pico UF2** firmware using **hvcc** compiler and **pico-sdk**. 

The goal of this project is to develop an interface between the Raspberry Pi Pico, its peripherals (such as knobs, buttons, and sensors), and Pure Data, providing an interactive workflow for creating embedded audio and MIDI tools.

It is currently a **Proof of Concept**. The core logic of the system has been implemented, although considerable amount of coding and testing remains. Development is ongoing to expand features and hardware support. Future plans include adding an automated Arduino project builder based on arduino-cli.

Check compiled binaries for RP2040 in the release section.

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
- [x] midi clock
- [ ] pwm audio
- [ ] sd card
- [ ] screen
- [ ] sensors
- [ ] usb audio
- [ ] bluetooth midi
      

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
    - debug console 
      
- Copies hardware config files into project folder from `/src`
- Builds firmware using **CMake** in a `build/` folder  
- Checks for device in BOOTSEL mode
- Flashes UF2 firmware to PICO board and restarts device
  

### Working state

1. HVCC supported vanilla pd objects.
2. Heavylib object support (hv.osc, hv.lfo ...) except hv.reverb.
3. Midi input/output implemented in usb, usb host and uart config. Also midi clock works with [midirealtimein] object.
4. Raspberry Pico can't sample audio so [adc] object will not work without an external adc.


## Notes

- The `[send]` and `[receive]` object names in the Pure Data patch **must exactly match** (case-sensitive) the **name** and **category** defined in `board.json`.  Also check for the correct `@hv_param` argument. 
- You can rename sends and receives as you wish. Currently, there is no enforced naming convention.
- To save resources remove unused send and receive objects from the patch.
- You don't need to remove objects from `board.json`, script adds objects which are present in the patch.
- Make sure to verify the correct pin configuration (e.g., **pin 1 corresponds to GPIO1**).
- [print] objects are parsed by the scipt automatically, they will output to the serial console. Use moderately or it will crash device, also remove unused prints from the patch. Regular console also is available in the code. Use midi cc120 to enable/disable debug console while pikoPD runs.
- **If you change the board or change midi mode in `board.json` remove the project folder or rename it in the command to rebuild files.**  
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




