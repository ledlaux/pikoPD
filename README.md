# PD examples for Raspberry Pi PICO  

This repository contains examples of running **PD patches exported with the HVCC compiler from Plugdata** on Raspberry Pi Pico (RP2040, RP2350) boards using the Arduino IDE.


---


# PD → HVCC → Raspberry Pi Pico UF2 Generator (FIRST TEST)

This project will automate building **PD patches** (`.pd`) into a **Raspberry Pi Pico UF2** firmware using **hvcc**, **pico-sdk**, and **picotool**. Custom templates for Pico-sdk and ArduinoIDE will be supported.

This version generates simple synthesizer with polyphonic usb midi input. 

## Features

- Converts Pure Data (`.pd`) patches to C code via **hvcc**  
- Use `settings.json` to set core frequency, i2s pins, voice count and sample rate
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

---


Work in progress~

