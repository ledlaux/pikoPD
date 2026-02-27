# PD examples for Raspberry Pi PICO  

This repository contains examples of running **PD patches exported with the HVCC compiler from Plugdata** on Raspberry Pi Pico (RP2040, RP2350) boards using the Arduino IDE.

---

# PD → HVCC → Raspberry Pi Pico UF2 Generator (Proof of concept)

This project will automate building **PD patches** (`.pd`) into a **Raspberry Pi Pico UF2** firmware using **hvcc**, **pico-sdk**, and **picotool**. 

Project is in very early stages. Patch in the folder is a simple synthesizer which uses [notein] object and usb midi input. Send CC1 to device midi chanel 1 and it will turn on builtin LED. [print] object sends values to serial console. 

## Features

- Converts Pure Data (`.pd`) patches to C code via **hvcc**
- Uses main.cpp as a template
- Set in `settings.json`:
    - board (pico, pico_w, pico2)
    - core frequency,
    - i2s pins,
    - voice count 
    - sample rate
    - led pin
- Copies extra C files for the build into project folder from `/src`
- Builds firmware using **CMake** in a `build/` folder  
- Check for device in BOOTSEL mode
- Flashes UF2 firmware to Pico2 board and restarts device

## Updates

- [x] led
- [x] usb midi
- [x] serial console 
- [ ] button
- [ ] pot
- [ ] encoder
- [ ] joystic
- [ ] sensors
- [ ] cv in
- [ ] gate in
- [ ] midi clock
- [ ] uart midi
- [ ] screen
- [ ] pwm audio
- [ ] usb audio

### Working state

1. HVCC supported vanilla pd objects should work.
2. Added heavylib object support (hv.osc, hv.lfo and other).
3. [print] objects are parsed automatically, so you can have many of them inside the patch.
4. Getting pico serial console to work together with the usb midi in pico-sdk was tricky, but it now works with [print] objects.
5. Some things are still hardcoded like LED inside the main.cpp. [s LED] needs to be kept inside pd patch.

### Sample loading

After some tests sample array loading works with arduino and pico-sdk for pico boards. Pico stores float values into the ram, 
to overcome limitation and load data to the flash we need to manually set tables to const in Heavy_patchname.cpp:

_float table -> const float table_

## Requirements

- Python 3.10+  
- [hvcc](https://github.com/enzienaudio/hvcc)  
- Raspberry Pi Pico SDK
- pico-extras library (put inside pico-sdk folder)
- Wasted-Audio/heavylib 
- [picotool](https://github.com/raspberrypi/picotool)  

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

