# PD → HVCC → Raspberry Pi Pico UF2 Generator 

This project automates building **PD patches** (`.pd`) into a **Raspberry Pi Pico UF2** firmware using **hvcc**, **pico-sdk**, and **picotool**. It is currently a **Proof of Concept**. Core logic is established, but there is a significant amount of coding ahead. You can support this project:  
[![Buy Me a coffee](https://img.shields.io/badge/Buy%20Me%20a%20Coffee-FFDD00?style=for-the-badge&logo=buy-me-a-coffee&logoColor=black)](https://www.buymeacoffee.com/ledlaux)  


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


## Default patch

The Pure Data patch in the folder is a simple synthesizer that uses the [notein] object and USB MIDI input.   
Sending CC1 to the device on MIDI channel 1 will turn on the LED (check the pins in setup.json). 

There are two [print] objects that send normalized values (0.0–1.0) from PD to the serial console:

**MIDI CC Input**  
Receives MIDI CC1 and prints value

**ADC Knob Input (GPIO26)**  
Reads analog knob using a receiver object and prints its value

Use the -s flag to enable loading of serial console in the terminal after flashing.

It has been tested on macOS, so if something does not work as expected on your system, please open a GitHub issue.

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
4. PD patch send and receive object names must corespond to the category and name set in the settings.json.
5. If you change board type in settings.json remove project folder or rename it in the command to rebuild files.


### Sample loading

After some tests sample array loading works with arduino and pico-sdk for pico boards. Pico stores float values into the ram, 
to overcome limitation and load data to the flash we need to manually set tables to const in Heavy_patchname.cpp:

_float table -> const float table_

## Requirements

- Python 3.10+
  - jinja2
- [hvcc](https://github.com/enzienaudio/hvcc)  
- [Raspberry Pi Pico SDK](https://github.com/raspberrypi/pico-sdk)
- [pico-extras](https://github.com/raspberrypi/pico-extras) (put inside pico-sdk folder)
- [Wasted-Audio/heavylib](https://github.com/Wasted-Audio/heavylib) 
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




