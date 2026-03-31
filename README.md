This is a test branch for the **HC-SR04** distance sensor reading and processing with pikoPD.  
**Since I do not own this device, the code is theoretical and requires hardware testing.**
I do not guarantee that it will work.

To implement the sensor reading I included this library in /lib:
https://github.com/dangarbri/pico-distance-sensor

## Pins
You can set pins in board.json. 
```
 "hc-sr04": [
        { "name": "distance", "trigger": 2, "echo": 3}
      ]
```
Code expect echo pin to be trigger pin +1.

## Patch

In the patches folder there is a patch distance_sensor.pd with [r distance @hv_param] object which should send 0-1.0 values from the sensor. 

## Template
Template will populate PD object hash and include the functions:

**Init**  
```
  {%- for d in active_dist %}
    Pico::addDistanceSensor({{ d.trig }}, {{ d.echo }});
    {%- endfor %}
```
**Process**  
```
{%- for d in active_dist %}
if (Pico::dist_sensor.changed()) {
    // 1. Get the sensor data
    float dist_cm = Pico::dist_sensor.getDistance();

    // 2. Send the captured value to the specific PD hash for this sensor
    hv_sendFloatToReceiver(&pd_prog, {{ d.hash }}, dist_cm);

    // 3. Now this line should work.
    printf("[Distance] Sensor: %s | Val: %.2f cm\n", "{{ d.name }}", dist_cm);
}
{%- endfor %}
```
## Sensor reading functions
The main wrapper functions are in /src/HC-SR04.h also in PicoControls.cpp:

```
void addDistanceSensor(uint32_t trig, uint32_t echo) {
dist_sensor.init(trig, echo, pio1, 3); // uses pio1 and state machine 3
}

bool processDistanceSensor() {
return dist_sensor.changed();
}

float getDistance() {
return dist_sensor.getDistance();
}
```
