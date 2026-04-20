This is a test branch for the **HC-SR04** distance sensor reading and processing with pikoPD.  

To implement the sensor reading this library is included in /lib:  
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

In the patches folder there is a patch distance_sensor.pd with `[r distance @hv_param]` object which should send 0-1.0 values from the sensor. 

## Template
Template will populate PD object hash and include the functions:

Check for active HC-SR04 object name in the board.json and receives in manifest.json.

```
{%- set active_dist = [] -%}
{%- if board.inputs.sensors['hc-sr04'] -%}
    {%- for d in board.inputs.sensors['hc-sr04'] if d.name in receives -%}
        {%- set _ = active_dist.append({
            'trig': d.trigger, 
            'echo': d.echo, 
            'hash': receives[d.name]
        }) -%}
    {%- endfor -%}
{%- endif -%}
```

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
The main wrapper functions are in /src/HC-SR04.h 
```
#pragma once
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/timer.h"
#include "distance_sensor.h"
#include <cmath>
#include <cstdio> 

namespace Pico {
class SonicSensor {
private:
    float current_val = 0.0f;
    int last_reported = -1;
    DistanceSensor* hw = nullptr;
    struct repeating_timer timer;

    static bool timer_callback(struct repeating_timer *t) {
        auto* self = (SonicSensor*)t->user_data;
        if (self->hw && !self->hw->is_sensing) {
            self->hw->TriggerRead();
        }
        return true; 
    }

public:
    void init(uint32_t trig, uint32_t echo, pio_hw_t* pio, int sm) {
        hw = new DistanceSensor{pio, (uint)sm, (uint)trig};
        add_repeating_timer_ms(60, timer_callback, this, &timer);

        #if ENABLE_DEBUG
        printf("[Distance] Started: Trig GP%d, Echo GP%d (PIO%d, SM%d)\n", 
                trig, echo, (pio == pio1), sm);
        #endif
    }

    bool changed() {
        if (!hw) return false;
        
        int d = hw->distance;
        if (std::abs(d - last_reported) > 1) {
            last_reported = d;
            current_val = (float)d;

            #if ENABLE_DEBUG
            printf("[Distance] New Value: %0.1f cm\n", current_val);
            #endif

            return true;
        }
        return false;
    }

    float getDistance() const { return current_val; }
};

extern SonicSensor dist_sensor;
void addDistanceSensor(uint32_t trig, uint32_t echo);
}
```


also in PicoControls.cpp:

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

## Debug prints

Enable debug console in board.json (set to true). Uncomment printf lines or connect `[r distance @hv_param]` object to `[print distance]` in PD patch. 
