# PikoPD manual 


PikoPD project automates building **Pure Data patches** (`.pd`) into a **UF2** firmware using **hvcc compiler** and **Raspberry Pi Pico C/C++ SDK**. 

The goal of this project is to develop an interface between the Raspberry Pi Pico, its peripherals (such as knobs, buttons, sensors), and PD, providing an interactive workflow for creating embedded audio and MIDI tools.

In your patches with pikoPD you can use hvcc compiler supported **vanilla PD** objects and **heavylib** objects like hv.osc, hv.lfo and other. 


# Toolchain setup

## Python 3.10+ 

  - jinja2


## Cmake and arm-none-eabi-gcc

### Mac:
```bash
brew install cmake
brew install git  
xcode-select --install  
brew install arm-none-eabi-gcc
```
If you encounter *nosys.specs* error  after installation of the arm-none-eabi-gcc homebrew version

```bash
brew uninstall --force arm-none-eabi-gcc
brew uninstall --force arm-none-eabi-binutils
brew install gcc-arm-embedded
```

I would suggest to use ARM version

Download:  
[https://developer.arm.com/downloads/-/a ... -downloads](https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads)

Then add this to the PATH:
```bash
echo 'export PATH="/Applications/ArmGNUToolchain/14.3.rel1/arm-none-eabi/bin:$PATH"' >> ~/.bash_profile && source ~/.bash_profile
```
### Linux:  
```bash
sudo apt install cmake git python3 build-essential gcc-arm-none-eabi libnewlib-arm-none-eabi libstdc++-arm-none-eabi- newlib
```  

## Heavy compiler (hvcc) 

```bash
python3 -m venv venv
source venv/bin/activate
git clone https://github.com/Wasted-Audio/hvcc.git  
cd hvcc/  
pip3 install -e .  
```

## Raspberry Pi Pico SDK 

```bash
git clone https://github.com/raspberrypi/pico-sdk.git  
cd pico-sdk  
git submodule update --init  
```

Set pico-sdk path environment variable:  
```bash
export PICO_SDK_PATH=/your_path/pico-sdk
```  

## pico-extras 

Must be places inside the pico-sdk folder.

```bash
cd pico-sdk  
git clone https://github.com/raspberrypi/pico-extras.git  
cd pico-extras  
git submodule update --init  
```

## picotool 

### Mac:  
```bash
brew install picotool
```  

### Linux:  

```bash
git clone https://github.com/raspberrypi/picotool
cd picotool
mkdir build && cd build
cmake .. -DPICO_SDK_PATH=$PICO_SDK_PATH
make -j8
sudo make install
```

## Usage

Enter bootloader mode by holding device boot button

```
python3 pikopd.py patches/heavy.pd project_name 

optional arguments:
  -h, --help           Show help message and exit
  -b, --board          Path to custom json configuration file
  -f, --flash          Flash UF2 to Pico (BOOTSEL mode required)
  -s, --serial         Open serial console after reboot
  -x, --skip-hvcc      Disable hvcc file regeneration for manual editing
  -v, --verbose        Enable verbose compiler console debug output
```


# Hardware configuration

Hardware configuration is done by adjusting the `board.json` file.  
This file defines how the board hardware (LEDs, inputs, joystick, etc.) is mapped to GPIO pins and how it behaves.

Set in `board.json`:
  
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


## Audio setup

- I2S (PCM5102)
  
```json
  "audio_mode": "I2S",
  "sample_rate": 48000,
  "channel": 2,
  "buffer_size": 64,
  "i2s_data_pin": 9,
  "i2s_bclk_pin": 10
```
LRCK pin is asigned automatically as next after the BCLK.

- PWM 
  
```json
  "audio_mode": "PWM",
  "sample_rate": 48000,
  "buffer_size": 64,
  "pwm_pin": 10
```

## Buttons

```json
"buttons": [
  { "name": "btn1", "pin": 23, "mode": "toggle" },
  { "name": "btn2", "pin": 11, "mode": "switch" },
  { "name": "btn3", "pin": 14, "mode": "bang" }
]
```

TOGGLE:	Latch (On/Off)   
SWITCH:	Press & Release (Momentary)  
BANG:	Trigger, Sends 1.0, then 0.0 after 50ms (can be adjusted)

## ADC 

```json
"adc_pins": [
  { "name": "knob", "pin": 26, "type": "knob" },
  { "name": "cv1", "pin": 27, "type": "cv_in" }
]
```

| Type    | Description                                             |
| ------- | ------------------------------------------------------- |
| `knob`  | Standard analog control such as a knob or potentiometer |
| `cv_in` | Control voltage input for external analog signals       |


Raspberry Pico can't sample audio so PD `[adc]` object will not work without an external adc.

## LED 

PikoPD boards support 4 LED modes.

- **pd** – Maps a Pure Data `[send]` object directly to the LED.
- **status** – LED turns on when the board is powered and working correctly.
- **midi** – LED blinks in response to incoming MIDI messages.
- **clock** – LED blinks in response to incoming MIDI clock.

```json
"leds": [
  { "name": "led1", "pin": 25, "mode": "pd" },
  { "name": "status", "pin": 24, "mode": "status" },
  { "name": "clock", "pin": 23, "mode": "clock" },
  { "name": "ledRGB", "pin": 16, "is_rgb": true, "mode": "midi" }
]
```

**Builtin LED pins**
| Board     | Pin | Notes                             |
| --------- | --- | --------------------------------- |
| Pico      | 25  | Single-color LED                  |
| Pico W    | 25  | Single-color LED                  |
| Pico 2    | 25  | Single-color LED                  |
| Pico Zero | 16  | RGB NeoPixel LED (`is_rgb: true`) |

Code supports up to 12 different led connection.


**RGB led** in PD accepts 1 value (intensity) or 2 values (hue and intensity) in range f0.0-1.0.  
Use `[pack f f]` object before `[s ledRGB]` to send 2 values. 


## Joystick 

PikoPD supports 2 joystick connection (each uses 2 adc pins), which can output values in either **regular** or **MIDI 1–127** range.


```json
"joystick": [
  { "name": "joy", "joy_x": 26, "joy_y": 27, "midi_range": true }
]
```

## Encoder

PikoPD supports 4 incremental rotary encoders 

```json
"encoders": [
      { "name": "enc", "pin_a": 5, "pin_b": 6 }
]
```

Use this construct in your patch from [encoder.pd](https://github.com/ledlaux/pikoPD/blob/main/patches/encoder.pd):

- `[r enc @hv_param]` receives incremental encoder changes -1 / +1   
- The value is accumulated using `[f]` and `[+]`
- `[mod]` wraps the value into a fixed range 
- Adjust `mod` to set the number of encoder steps (e.g., `mod 8`, `mod 16`)

# Sensors


## MPR121

```json
"sensors": {
      "mpr121": [
        { "name": "mpr1", "i2c_bus": "i2c0", "sda": 4, "scl": 5, "irq": 6, "addr_index": 0 },
        { "name": "mpr2", "i2c_bus": "i2c1", "sda": 6, "scl": 7, "irq": 8, "addr_index": 0 }
      ]
    }
```
PikoPD supports up to 4 MPR121 capacitive touch sensor devices on each of the i2c buses. To use two or more mpr121 on the same i2c bus you will have to phisically change it's adress and set *addr_index*: 

1. 0x5A,
2. 0x5B,
3. 0x5C,
4. 0x5D

IRQ pin is used by default to make processing more efficient. 

To use this sensor in the PD  patch create `[r pad1 @hv_param]` object for each pad in numerical order. Script will automatically asign pad objects to each of the devices (0-12, 13-24...) set in *board.json*. 


## CNY70


```json
"sensors": {
      "cny70": [
        { "name": "cny", "adc_pin": 28 }
      ]
    }
```
The CNY70 is a short-range reflective optical sensor (often called an optoisolator or phototransistor). 

Sensor contains two main parts inside its square plastic housing:

- Infrared (IR) Emitter – An LED that continuously emits invisible infrared light
- Phototransistor (Receiver) – A light-sensitive component that detects reflected IR light

When you place a finger or an object in front of the sensor (within a few millimeters), the IR light reflects off the object and hits the receiver. The sensor then outputs a voltage based on how much light was reflected. Since the CNY70 is produced by multiple manufacturers, many wiring variations and tutorials are available online.

To use this sensor in a PD patch, connect its output to an ADC pin and add `[r cny @hv_param]` object.


# Project configuration

- Hardware configuration is done by adjusting the `board.json`.
- Use only **hvcc supported PD objects** in your patches.
- The `[s name @hv_param]` and `[r name @hv_param]` object names in PD patch must exactly match (case-sensitive) the name defined in `board.json`.
- Check for the correct `@hv_param` argument for you PD objects. 
- You can rename sends and receives as you wish. Currently, there is no enforced naming convention.
- There’s no need to remove objects from board.json. The script automatically includes only objects present in the patch and ignores unconnected.
- Verify the correct pin configuration (e.g., **pin 1 corresponds to GPIO1**).
- If you change board and MIDI mode or encounter compile-time errors remove the project folder or rename it to rebuild files.
- Tested on macOS.
- If something does not work as expected on your system, please open a [GitHub issue](https://github.com/ledlaux/pikoPD/issues).


# Polyphonic input

The Pure Data `[poly]` object works with `[notein]` on PICO, but it is resource-intensive.      

To make MIDI note processing lightweight, a custom voice allocation system with oldest voice stealing was implemented using `[r NOTE]` objects.  

To use the custom system:  
1. Set **voice count** to 2 or more in `board.json`.  
2. Add `[NOTE1, [NOTE2]...` objects for each voice.
4. Use `[unpack]` to extract **note**, **velocity**, and **channel** in the PD patch.  

Check example in the patch folder. 


# MIDI 

```json
"midi_mode": "usb"     // Usb midi device "PikoPD"
"midi_mode": "uart"    // Default pins tx 0, rx 1 
"midi_mode": "host"    // Use otg cable to power pico and usb midi device
```
Midi clock and start/stop messages work with PD `[midirealtimein]` object.

| CC Number | Parameter              | 
|-----------|------------------------|
| 7         | Master Volume          | 
| 8        | Limiter Bypass         | 
| 90        | Delay Time             |
| 91        | Delay Send Level       | 
| 92        | Delay Feedback Amount  |
| 93        | Delay Bypass           | 
| 120       | Debug Toggle           | 

You can enable the masterFX in the board.json. To use safe volume it is recomended to keep limiter on. I added a simple delay utilising delayline from DaisySP library. You can use your own fx by adding code to audioFunc after the pd audio processing in the main.cpp.  


# Sample loading

Sample loading works despite the limitations. Here is a [tutorial](https://www.youtube.com/watch?v=0qgkYWsYdTo) for a sample loading using Plugdata.

By design, hvcc-generated code stores samples in float arrays in RAM. PikoPD applies a patch to store them in flash memory, making it possible to load more.

# Serial monitor 

```json
  "console": true
```

Debug console will also output PD [print] objects, which are parsed automatically. Use it moderately, because it can crash the device. 

# WEB config tool

Select your board model (Raspberry Pico, Pico W, Zero or Pico 2).
Upload your .pd patch to see available parameters or load board.json configuration file.
Click a pin on the board and add a component, or drag a parameter tag directly onto a pin.
Export the board.json and place it in the pikoPD folder.

# WEB control and OSC

For devices with Wi-Fi like picoW and pico2W WEB and OSC control will be added soon. Check web code branch for more info. 

# Useful links

- About hvcc compiler  
  https://wasted-audio.github.io/hvcc/
- hvcc suported PD object list
  https://github.com/Wasted-Audio/hvcc/blob/develop/docs/reference/objects/supported.md

