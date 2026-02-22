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
- [ ] button
- [ ] pot
- [ ] encoder
- [ ] joystic
- [ ] sensors
- [ ] cv in
- [ ] gate in
- [ ] midi clock
- [ ] uart midi
- [x] usb midi
- [ ] screen
- [ ] pwm audio
- [ ] usb audio


## Requirements

- Python 3.10+  
- [hvcc](https://github.com/enzienaudio/hvcc)  
- Raspberry Pi Pico SDK
- pico-extras library
- [picotool](https://github.com/raspberrypi/picotool)  

## Usage

Enter bootloader mode by holding device boot button

```bash
python3 pikopd.py patches/heavy.pd MyProjectRoot 

optional arguments:
  -h, --help           Show help message and exit
  -f, --flash          Flash UF2 to Pico (BOOTSEL mode required)
  -s, --serial         Open serial console after reboot
  -v, --verbose        Enable verbose debug output
```


