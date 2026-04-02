## PikoPD using web

This branch of pikoPD is dedicated to experiments with web protocols and Raspberry Pico W and Pico 2W.
I will be testing HTTP web control and OSC protocol to interact with PD patches running on the device.


## First test - HTTP


Raspberry Pi Pico W connected to Wi-Fi and hosting a webpage with a slider that controls PD parameter.

https://github.com/user-attachments/assets/1167bb92-f829-4321-8264-e8fa96430ee1



## Second test - receive OSC message

Raspberry Pi Pico W connected to Wi-Fi and receiving OSC messages from the mac that controls PD parameter.

https://github.com/user-attachments/assets/80f30ee2-c3fe-4fdc-927f-d8da6f6a1685


## Third test - send OSC message 

Raspberry Pi Pico W sending OSC messages from the PD patch over Wi-Fi.

https://github.com/user-attachments/assets/c7da8355-add5-4c24-b526-2aa6ea40b852


I use liblo for testing on mac. 

First we need to send any pairing OSC message or ping to device and then read the output:

```bash
oscsend pikopd.local 8000 /v f 10
oscdump 9000   
```
