# pikoPD(v0.0.1)

Automate building of **Pure Data patches** (`.pd`) into a **UF2** firmware using **HVCC compiler** and **Raspberry Pi Pico C/C++ SDK** using python script. 

This project provides a hardware abstraction layer linking the Raspberry Pi Pico and its peripherals to Pure Data, streamlining the creation of interactive embedded audio and MIDI tools.

The core logic of the system has been implemented, although considerable amount of testing remains. Development is ongoing to expand features and hardware support. 

For a hardware configuration you can use interactive **web config tool**: [Live Demo](https://ledlaux.github.io/pikoPD).

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


## Project configuration

- PikoPD supports hvcc-compatible vanilla PD objects and heavylib objects, such as hv.osc~ and hv.lfo~.
- All hardware configuration is done using `board.json` file.
- Check PD patch examples in the folder.
- The `[s @hv_param]` and `[r @hv_param]` object names must exactly match (case-sensitive) names defined in the config file.
- The script automatically includes objects present in the patch and ignores unconnected.
- Debug console, when enabled, will also output PD `[print]` objects. Use it moderately, because it can crash the device.
- If you change board and MIDI mode or encounter compile-time errors remove the project folder or rename it to rebuild files.   
- Tested on macOS.


## Requirements

- Python 3.10+
  - jinja2
- arm-none-eabi toolchain
- Heavy compiler (hvcc)
- Raspberry Pi Pico SDK
- pico-extras
- picotool

Toolchain setup instructions can be found in the [manual](https://github.com/ledlaux/pikoPD/blob/main/docs/manual.md).
 
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
  -s, --serial         Open serial console after reboot
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
