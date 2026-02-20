# PD → HVCC → Raspberry Pi Pico UF2 Generator (FIRST TEST)

This project will automate building **PD patches** (`.pd`) into a **Raspberry Pi Pico UF2** firmware using **hvcc**, **pico-sdk**, and **picotool**. Extra files and custom templates will be supported.

This version generates simple synthesizer with polyphonic usb midi input. 

## Features

- Converts Pure Data (`.pd`) patches to C code via **hvcc**
- Uses main.cpp as a template 
- Set core frequency, i2s pins, voice count and sample rate in `settings.json` 
- Copies extra C files into project folder  
- Builds firmware using **CMake** in a `build/` folder  
- Automatically flashes UF2 to Pico2 devices  

## Requirements

- Python 3.10+  
- [hvcc](https://github.com/enzienaudio/hvcc)  
- Raspberry Pi Pico SDK  
- [picotool](https://github.com/raspberrypi/picotool)  

## Usage

Enter bootloader mode by holding boot on power on.

```bash
python3 pikoPDuploader.py PdPatches/heavy.pd MyProjectRoot --flash
```
Reset device.
