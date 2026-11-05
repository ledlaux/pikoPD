# pikoPD Ableton OSC integration

This is interesting idea which I tested on pico_w with AbletonOSC library. You can switch clips and scenes in Ableton Live using OSC messages sent from pikoPD device on WiFi network. This can be developed further for building custom Ableton Live controllers with Pimoroni Keypad or other hardware. 

This is first test of this integration which allows to asign hardware buttons to OSC adresses inside PD patch. For now only scenes and clips can be controlled. Adress mapping is found in src/osc_adress.h and routing happens in sendHookHandler function in main.cpp. 

# Test

Compiled UF2 binary is found in the project root folder. It is compiled in AP mode, after power on connect to pikoPD WiFi and open Ableton Live daw with AbletonOSC control surface script loaded inside the Settings/Midi. Press connected hardware buttons and it should switch clips on track 1 and also scene 1. 

AbletonOSC library setup instructions: https://github.com/ideoforms/AbletonOSC


# Setting PD patch for OSC

In the /patches folder.

#N canvas 450 100 850 600 12;  
#X text 22 15 pikoPD - Ableton Live OSC Controller;  
#X text 136 77 Track 1 - Clip launch;  
#X obj 89 178 bng 25 250 50 0 empty empty Clip1 30 12 0 10 #191919 #ffffff #ffffff;  
#X obj 293 164 bng 25 250 50 0 empty empty Clip2 30 12 0 10 #191919 #ffffff #ffffff;  
#X obj 89 122 r btn1 @hv_param;  
#X obj 89 225 s osc_clip_1_2 @hv_param;  
#X obj 158 170 s led1 @hv_param;  
#X obj 293 122 r btn2 @hv_param;  
#X obj 293 215 s osc_clip_1_1 @hv_param;  
#X obj 107 369 bng 25 250 50 0 empty empty Scene1 30 12 0 10 #191919 #ffffff #ffffff;  
#X obj 107 333 r btn3 @hv_param;  
#X obj 107 408 s osc_scene_1 @hv_param;  
#X text 155 284 Scene 1 launch;  
#X connect 2 0 5 0;  
#X connect 3 0 8 0;  
#X connect 4 0 2 0;  
#X connect 4 0 6 0;  
#X connect 7 0 3 0;  
#X connect 9 0 11 0;  
#X connect 10 0 9 0;  

# Hardware configuration

Asign buttons inside board.json 

    "buttons": [
      { "name": "btn1", "pin": 14, "mode": "switch" }, 
      { "name": "btn2", "pin": 13, "mode": "bang" },
      { "name": "btn3", "pin": 12, "mode": "bang" }
    ],

Set "switch" if want to also use led for visual representation of the action. 

# Wifi configuration

AbletonOSC works on 11000 port (is hardcoded in web.h). This implementation works on both AP and STA modes. Check serial console for WiFi connection debugging. 

   "web": {
      "enabled": true,
      "active_mode": 0,
      "ap": {
        "mode_id": 0,
        "ssid": "pikoPD",
        "password": "12345678"
      },
      "sta": {
        "mode_id": 1,
        "ssid": "",
        "password": ""
      },
      "mdns_name": "pikopd",
      "http_port": 80,
      "osc_enabled": true,
      "osc_port": 11001
    }
  

# Compilation and flashing

python3 pikopd.py patches/ableton_osc.pd ableton_osc -f 




