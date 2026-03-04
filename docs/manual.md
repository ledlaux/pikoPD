## Toolchain setup ##

### Python 3.10+ ###

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

### Heavy compiler (hvcc) ###

```bash
python3 -m venv venv
source venv/bin/activate
git clone https://github.com/Wasted-Audio/hvcc.git  
cd hvcc/  
pip3 install -e .  
```

### Raspberry Pi Pico SDK ###

```bash
git clone https://github.com/raspberrypi/pico-sdk.git  
cd pico-sdk  
git submodule update --init  
```

Set pico-sdk path environment variable:  
```bash
export PICO_SDK_PATH=/your_path/pico-sdk
```  

### pico-extras ###

Must be places inside the pico-sdk folder.

```bash
cd pico-sdk  
git clone https://github.com/raspberrypi/pico-extras.git  
cd pico-extras  
git submodule update --init  
```

### picotool ###

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
  -x, --skip-hvcc      Disable hvcc file regeneration for manual editing
  -f, --flash          Flash UF2 to Pico (BOOTSEL mode required)
  -s, --serial         Open serial console after reboot
  -v, --verbose        Enable verbose debug output
```


## Hardware settings ##


**Buttons**

TOGGLE:	Latch (On/Off)   
SWITCH:	Press & Release (Momentary)  
BANG:	Trigger, Sends 1.0, then 0.0 after 50ms (can be adjusted)

