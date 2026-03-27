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

# Sensors


## MPR121

```json
"sensors": {
      "mpr121": [
        { "name": "mpr1", "i2c_bus": "i2c0", "sda": 4, "scl": 5, "irq": 3, "addr_index": 0 },
        { "name": "mpr2", "i2c_bus": "i2c1", "sda": 6, "scl": 7, "irq": 8, "addr_index": 0 }
      ]
    }
```
PikoPD supports up to 4 MPR121 capacitive touch sensor devices on each of the i2c bus. To use two or more mpr121 on the same i2c bus you will have to phisically change it's adress and set *addr_index*: 

1. 0x5A,
2. 0x5B,
3. 0x5C,
4. 0x5D

I decided to use IRQ pin as obligatory to make polling more efficient. 

To use this sensor in the PD  patch create [r pad1 @hv_param] object for each pad in numerical order. Script will automatically asign pad objects to each of the devices (0-12, 13-24...) set in *board.json*. 


## CNY70

**Support for this device is experimental!**

```
"sensors": {
      "cny70": [
        { "name": "cny", "adc_pin": 28 }
      ]
    }
```
The CNY70 is a short-range reflective optical sensor (often called an optoisolator or phototransistor). Unlike a button that you physically click, this sensor detects how close an object is by bouncing light off it.

The sensor contains two main parts inside its square plastic housing:

- Infrared (IR) Emitter: An LED that constantly sends out invisible light.
- Phototransistor (Receiver): A light-sensitive component that "looks" for that IR light.

When you place a finger or an object in front of the sensor (within a few millimeters), the IR light reflects off the object and hits the receiver. The sensor then outputs a voltage based on how much light was reflected.

There are many tutorials in the internet which shows different ways how to wire that thing. I use [this](https://github.com/ledlaux/pikoPD/blob/mpr121/docs/images/cny70.png) which works for my cny70 sensor. 

To read this device connect it to the ADC pin and add [r cny@hv_param] object in the PD patch. 



# Polyphonic input

The Pure Data `[poly]` object works with `[notein]` on PICO, but it is resource-intensive.      

To make MIDI note processing lightweight, a custom voice allocation system with oldest voice stealing was implemented using `[r NOTE]` objects.  

To use the custom system:  
1. Set **voice count** to 2 or more in `board.json`.  
2. Add `[NOTE1, [NOTE2]...` objects for each voice.
4. Use `[unpack]` to extract **note**, **velocity**, and **channel** in the PD patch.  

Check example in the patch folder. 


# MIDI CC

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

Sample loading works despite the limitations (link to tutorial is in the next section). Pico stores sample data into the ram, so to load it to the flash we need to manually set tables to *const* in Heavy_patchname.cpp:

_float table -> const float table_

Then run command with -x flag to skip file rebuilding. 


# Useful links

- About HVCC compiler  
  https://wasted-audio.github.io/hvcc/
- Supported vanilla objects  
  https://github.com/Wasted-Audio/hvcc/blob/develop/docs/reference/objects/supported.md
- Tutorial of how to load samples into the pd patch for the HVCC compiler  
  https://www.youtube.com/watch?v=0qgkYWsYdTo



# Changelog
