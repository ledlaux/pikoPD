# Toolchain setup 

### Python 3.10+ 

  - jinja2


### arm-none-eabi-gcc toolchain ###

Mac:
```bash
brew install cmake
brew install git  
xcode-select --install  
brew install arm-none-eabi-gcc
```   

Linux:  
```bash
sudo apt install cmake git python3 build-essential gcc-arm-none-eabi libnewlib-arm-none-eabi libstdc++-arm-none-eabi- newlib
```  

### Heavy compiler (hvcc) 

```bash
python3 -m venv venv
source venv/bin/activate
git clone https://github.com/Wasted-Audio/hvcc.git  
cd hvcc/  
pip3 install -e .  
```

### Raspberry Pi Pico SDK 

```bash
git clone https://github.com/raspberrypi/pico-sdk.git  
cd pico-sdk  
git submodule update --init  
```

Set pico-sdk path environment variable:  
```bash
export PICO_SDK_PATH=/your_path/pico-sdk
```  

### pico-extras 

Must be places inside the pico-sdk folder.

```bash
cd pico-sdk  
git clone https://github.com/raspberrypi/pico-extras.git  
cd pico-extras  
git submodule update --init  
```

### picotool 

Mac:  
```bash
brew install picotool
```  

Linux:  

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

## ADC Inputs

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


## LED configuration

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
Use [pack f f] object before [s ledRGB] to send 2 values. 


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

## Supported MIDI CC

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
