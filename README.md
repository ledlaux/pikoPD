# pikoPD(v0.0.1)

This project provides a hardware abstraction layer for developing embedded audio, MIDI, and interactive hardware applications with the **Pure Data (Pd)** environment on **Raspberry Pi Pico** boards. 

It automates the conversion of `.pd` patches into **UF2 firmware** using the **HVCC compiler** and **Pico C/C++ SDK**.

Hardware configuration is managed through a simple configuration file defining hardware, pins, and peripherals. While the build system, combining Python automation and CMake, handles code generation, patch conversion, firmware compilation, and uploading to the target board.

This is a solo hobby project. AI is used as a development assistant for code generation, while the design, feature concepts, hardware integration and testing are carried out by the project author. 

PikoPD is in active development, with the core system implemented and future work focused on optimisation, expanding features, and supporting additional hardware.

Read the [manual](https://github.com/ledlaux/pikoPD/blob/main/docs/manual.md) for  detailed instructions.


## Features

- [x] led
- [x] knob
- [x] cv in 
- [x] button
- [x] encoder
- [x] joystick
- [x] gate in/out
- [x] sensors
- [x] i2s audio
- [x] pwm audio
- [x] uart midi
- [x] usb midi
- [x] usb midi host
- [x] midi clock
- [x] web & osc
- [x] display
- [ ] cv out
- [ ] sd card
- [ ] audio input
- [ ] bluetooth midi


## Requirements

- Python 3.10+
  - jinja2
- arm-none-eabi toolchain
- Heavy compiler (hvcc)
- Raspberry Pi Pico SDK
- pico-extras
- picotool

Toolchain setup instructions can be found in the [manual](https://github.com/ledlaux/pikoPD/blob/main/docs/manual.md).

## Project configuration

- PikoPD supports HVCC compatible vanilla PD and heavylib objects, such as hv.osc~ and hv.lfo~.
- Hardware configuration is done using `board.json` file or interactive [web config tool](https://ledlaux.github.io/pikoPD).
- The `[s @hv_param]` and `[r @hv_param]` object names must exactly match (case-sensitive) names defined in the config file.
- The script automatically includes objects present in the patch and ignores unconnected.
- Debug console, when enabled, will also output PD `[print]` objects. Use it moderately, because it can crash the device.
- If you change board and MIDI mode or encounter compile-time errors remove the project folder or rename it to rebuild files.
- Check PD patch examples in the folder.
- Tested on macOS.
 
## Build

pikopd.py

- Converts Pure Data (`.pd`) patch to C code via **hvcc** compiler
- Copies config files into project folder from `/src`
- Configures hardware using `board.json`
- Uses `main.cpp` as a project template
- Builds firmware using **CMake** in a `build/` folder  
- Checks for device in BOOTSEL mode
- Flashes UF2 firmware to PICO board and restarts device


### Usage

Enter bootloader mode by holding device boot button.

```
python3 pikopd.py patches/heavy.pd project_name -f

optional arguments:
  -h, --help           Show help message and exit
  -b, --board          Path to custom json configuration file
  -f, --flash          Flash UF2 to Pico (BOOTSEL mode required)
  -s, --serial         Open serial console after reboot (works only on MAC)
  -x, --skip-hvcc      Disable hvcc file regeneration for manual editing
  -v, --verbose        Enable verbose compiler console debug output
```
  
## Sample loading

Here is a [tutorial](https://www.youtube.com/watch?v=0qgkYWsYdTo) for a sample loading using Plugdata. Also check example [patch](https://github.com/ledlaux/pikoPD/blob/develop/patches/sample_drums_MPR121.pd).

Video of using MPR121 with samples loaded in PD on RP2040:  

https://github.com/user-attachments/assets/2db6b777-098d-49f5-b9c8-0d9d2602aadc


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

**Monosynth.pd** is a monophonic synthesizer with simple envelope and delay effect. Send CC1 and CC2 to control delay.


## Useful links

- About HVCC compiler  
  https://wasted-audio.github.io/hvcc/
- Supported vanilla objects  
  https://github.com/Wasted-Audio/hvcc/blob/develop/docs/reference/objects/supported.md


## Licence

MIT licence
