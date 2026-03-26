# Pure Data → HVCC → Raspberry Pi Pico UF2 Generator 

This project automates building **PD patches** (`.pd`) into a **Raspberry Pi Pico UF2** firmware using **hvcc** compiler and **pico-sdk**. 

The goal of this project is to develop an interface between the Raspberry Pi Pico, its peripherals (such as knobs, buttons, and sensors), and Pure Data, providing an interactive workflow for creating embedded audio and MIDI tools.

It is currently a **Proof of Concept**. The core logic of the system has been implemented, although considerable amount of testing remains. Development is ongoing to expand features and hardware support. Future plans include adding an automated project builder using arduino-cli workflow.

Compiled binaries for RP2040 are published in the [release](https://github.com/ledlaux/pikoPD/releases/tag/test1) section. Check [Discussions](https://github.com/ledlaux/pikoPD/discussions) for the project updates. 

I also created an interactive web configuration tool. You can try it here: [Live Demo](https://ledlaux.github.io/pikoPD).

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
- [x] pwm audio
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
    - audio mode (I2S, PWM) and pins
    - voice count
    - led (pwm, rgb and mode)
    - adc pins (knob, cv_in)
    - rotary encoder 
    - gate in/out (gate or trigger)
    - button (bang, toggle, switch)
    - joystic and range (regular or midi 1-127)
    - midi mode (uart, usb, host)
      - uart (pins tx 0, rx 1 )
    - debug console
    - sensors
      - cny70
      - mpr121
    - masterfx (delay, limiter)
      
- Copies hardware config files into project folder from `/src`
- Builds firmware using **CMake** in a `build/` folder  
- Checks for device in BOOTSEL mode
- Flashes UF2 firmware to PICO board and restarts device


## Working state

1. HVCC supported vanilla pd objects.
2. Heavylib object support (hv.osc, hv.lfo ...) except hv.reverb.
3. Midi input and output implemented in usb, usb host and uart config. Midi clock and start/stop messages work with PD `[midirealtimein]` object.
4. Debug console will also output PD `[print]` objects, which are parsed automatically. This function is stable on the RP2040, but there are some issues on the RP2350, which can cause the console to crash device after a few PD `[print]` messages. Use it moderately. This issue is being discussed here [https://github.com/ledlaux/pikoPD/issues/26].
5. Raspberry Pico can't sample audio so `[adc]` object will not work without an external adc.
6. Added experimental CNY70 optical sensor support, but it still needs testing and code adjustments. Wiring diagram is in the `docs` folder and there is a patch example.

   
## Notes

[Read manual](https://github.com/ledlaux/pikoPD/blob/main/docs/manual.md)

- All hardware configuration is done by adjusting `board.json` file.
- The `[send]` and `[receive]` object names in the Pure Data patch **must exactly match** (case-sensitive) the **name** defined in `board.json`.  Also check for the correct `@hv_param` argument. 
- You can rename sends and receives as you wish. Currently, there is no enforced naming convention.
- There’s no need to remove objects from board.json. The script automatically includes only objects present in the patch and ignores unconnected.
- Make sure to verify the correct pin configuration (e.g., **pin 1 corresponds to GPIO1**).
- **If you change board or MIDI mode in `board.json` and encounter compile-time errors, remove the project build folder or rename project in the command to rebuild files.**
- Tested on **macOS**.  
- If something does not work as expected on your system, please open a [GitHub issue](https://github.com/ledlaux/pikoPD/issues).

## Polyphonic input

The Pure Data `[poly]` object works with `[notein]` on PICO, but it is resource-intensive.      

To make MIDI note processing lightweight, a custom voice allocation system with oldest voice stealing was implemented using `[r NOTE]` objects.  

To use the custom system:  
1. Set **voice count** to 2 or more in `board.json`.  
2. Add `[NOTE1, [NOTE2]...` objects for each voice.
4. Use `[unpack]` to extract **note**, **velocity**, and **channel** in the PD patch.  

Check example in the patch folder. 

## Supported MIDI CC

| CC Number | Parameter              | 
|-----------|------------------------|
| 7         | Master Volume          | 
| 8        | Limiter Bypass         | 
| 90        | Delay Time             |
| 91        | Delay Send Level       | 
| 92        | Delay Feedback Amount  |
| 93        | Delay Bypass           | 
| 120       | Debug Console Toggle   | 

To use safe volume it is recomended to keep limiter on. I added a simple delay utilising delayline from DaisySP library. 
  

## Sample loading

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

Enter bootloader mode by holding device boot button.

```
python3 pikopd.py patches/heavy.pd project_name -f

optional arguments:
  -h, --help           Show help message and exit
  -b, --board          Path to custom json configuration file
  -f, --flash          Flash UF2 to Pico (BOOTSEL mode required)
  -s, --serial         Open serial console after reboot
  -x, --skip-hvcc      Disable hvcc file regeneration for manual editing
  -v, --verbose        Enable verbose compiler console debug output
```

## Useful links

- About HVCC compiler  
  https://wasted-audio.github.io/hvcc/
- Supported vanilla objects  
  https://github.com/Wasted-Audio/hvcc/blob/develop/docs/reference/objects/supported.md
- Tutorial of how to load samples into the pd patch for the HVCC compiler  
  https://www.youtube.com/watch?v=0qgkYWsYdTo


## Licence

The code will be released under an open-source license once the project reaches version 0.0.1.

No warranty is provided. Use at your own risk.
