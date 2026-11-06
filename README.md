# pikoPD

This project provides a hardware abstraction layer for developing embedded audio, MIDI, and interactive hardware applications with the **Pure Data (Pd)** environment on **Raspberry Pi Pico** boards. Turn `.pd` patches into **UF2 firmware** using the **HVCC compiler** and **Pico C/C++ SDK**.

Hardware configuration is managed through a simple configuration file defining hardware, pins, and peripherals. While the build system, combining Python automation and CMake, handles code generation, patch conversion, firmware compilation, and uploading to the target board.

This started as solo hobby project which would not be possible without AI assistance, while the design, feature concepts, hardware integration, and testing are carried out by me.

PikoPD is in active development, with the core system implemented and future work focused on optimisation, expanding features, and supporting additional hardware. Feel free to open issues, start discussions, or join the development!

Read the [manual](https://github.com/ledlaux/pikoPD/blob/main/docs/manual.md) for detailed instructions.

## Hardware features

- [x] led
- [x] knob
- [x] button
- [x] encoder
- [x] cv in/out 
- [x] gate in/out
- [x] joystick
- [x] sensors
- [x] i2s & pwm audio
- [x] uart midi
- [x] usb midi
- [x] usb midi host
- [x] midi clock
- [x] web & OSC
- [x] display
- [ ] audio input
- [ ] sd card
- [ ] usb audio
- [ ] bluetooth midi

# Architecture

PikoPD takes advantage of the Pico and Pico 2 multicore processing:

### Core 0 — Hardware & Control
* Reads buttons, sensors, GPIO, and analog inputs using polling and PIO.
* Handles ADC/CV inputs and MIDI communication.
* Maps hardware controls to Pure Data patch parameters.

### Core 1 — Audio Engine
* Handles real-time audio generation 
* Uses PIO for accurate audio output timing (I2S or high-frequency PWM).

## Requirements

- Python 3.10+
  - Jinja2
  - pyserial
- arm-none-eabi toolchain
- CMake
- Ninja
- Heavy compiler (HVCC)
- Raspberry Pi Pico SDK
- pico-extras
- picotool

Toolchain setup instructions can be found in the [manual](https://github.com/ledlaux/pikoPD/blob/main/docs/manual.md).

## Configuration

- Hardware configuration is done using `board.json` file or interactive [web config tool](https://ledlaux.github.io/pikoPD).
- The `[s @hv_param]` and `[r @hv_param]` object names must exactly match (case-sensitive) names defined in the config file.
- The script includes objects present in the patch and ignores unconnected. But it is recommended to remove unused items from config.
- Debug console, when enabled, will also output PD `[print]` objects. Use it moderately, because it can crash the device.
- If you change board and MIDI mode or encounter compile-time errors project folder needs to be cleaned first. Use -c flag.
- Use HVCC compatible vanilla PD and heavylib objects, such as hv.osc~ and hv.lfo~.
- Check PD patch examples in the folder.
 
## Build

pikopd.py

- Converts Pure Data (`.pd`) patch to C code via **hvcc** compiler
- Copies config files into project folder from `/src`
- Configures hardware using `board.json`
- Uses `main.cpp` as a project template
- Builds firmware using **CMake** in a `build/` folder  
- Switches device to BOOTSEL mode automatically
- Flashes UF2 firmware to the PICO board and restarts device

### Usage

Enter bootloader mode by holding device boot button.

```
python3 pikopd.py patches/heavy.pd project_name -f

optional arguments:
  -h, --help           Show help message and exit
  -b, --board          Path to custom json configuration file
  -f, --flash          Flash UF2 to Pico 
  -c, --clean          Clean the project folder and recompile
  -s, --serial         Open serial console after reboot
  -x, --skip-hvcc      Disable hvcc file regeneration for manual editing
  -v, --verbose        Enable verbose compiler console debug output
```
  
## Sample loading

Here is a [tutorial](https://www.youtube.com/watch?v=0qgkYWsYdTo) for a sample loading using Plugdata. Also check example [patch](https://github.com/ledlaux/pikoPD/blob/develop/patches/sample_drums_MPR121.pd).

Video of using MPR121 with samples loaded in PD on RP2040:  

https://github.com/user-attachments/assets/2db6b777-098d-49f5-b9c8-0d9d2602aadc

## Audio Input and FX

Raspberry Pico can't sample audio with builtin analog to digital converters, so PD `[adc]` object will not work without an external hardware.   

Some successful audio input test example using PCM1802 (PCM1808 also should work) and PD can be found in the [experimental](https://github.com/ledlaux/pikoPD/tree/develop/experimental/audio_input/pcm1802) folder. 

This is not yet fully integrated in the main project and needs separate project build because of the tricky pio and dma settings. Hope to integrate it fully in the future updates, because it is the most requested feature.

But don't expect that this devices can run big reverbs. RAM is very limited. But if I get my hands on a Pico 2 board equipped with PSRAM, I'll definitely look into how to integrate that and improve this features. For now I have added Delay and Reverb processing as Master FX which can be enabled in the config. 

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


# WEB Config Tool

I started to develop interactive hardware config tool for the pikoPD projects which is available online:
https://ledlaux.github.io/pikoPD/

You can upload your .pd patch and map controls to the corresponding patch receivers and senders. 
Click a pin on the board to add a component, or drag a PD object tag directly onto a pin to asign.
Some of the hardware objects like enc (encoder) are automatically recognized and placed in the coresponding category.     
Finaly export the board.json.    

This tool still needs further development, and unfortunately I don't have the time or capacity to work on it right now. 

In the future, it could evolve into a hub for sharing custom user firmwares and a playful educational tool for exploring and interacting with pikoPD projects.



## Useful links

- About HVCC compiler  
  https://wasted-audio.github.io/hvcc/
- Supported vanilla objects  
  https://github.com/Wasted-Audio/hvcc/blob/develop/docs/reference/objects/supported.md


## Licence

MIT licence
