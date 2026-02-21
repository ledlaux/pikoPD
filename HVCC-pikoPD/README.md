# PD → HVCC → Raspberry Pi Pico UF2 Generator (Proof of concept)

This project will automate building **PD patches** (`.pd`) into a **Raspberry Pi Pico UF2** firmware using **hvcc**, **pico-sdk**, and **picotool**. 

Project is in very early stages. Patch in the folder is a simple synthesizer which uses [notein] object and usb midi input. Send CC1 to device midi chanel 1 and it will turn on builtin LED. PD is printing cc values into serial console. 

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
- Flashes UF2 to Pico2 board and restarts device 

## Requirements

- Python 3.10+  
- [hvcc](https://github.com/enzienaudio/hvcc)  
- Raspberry Pi Pico SDK  
- [picotool](https://github.com/raspberrypi/picotool)  

## Usage

Enter bootloader mode by holding device boot button

```bash
python3 pikoPDuploader.py PdPatches/heavy.pd MyProjectRoot --flash
```

