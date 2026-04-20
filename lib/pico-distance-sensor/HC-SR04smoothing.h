#pragma once

#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/timer.h"
#ifdef DISTANCE_SENSOR_ENABLED
    #include "distance_sensor.h"
#endif
#include <cmath>
#include <cstdio> 

namespace Pico {
class SonicSensor {
private:
    float current_val = 0.0f;
    float smoothed_val = 0.0f;
    int last_reported = -1;
    DistanceSensor* hw = nullptr;
    struct repeating_timer timer;
    
    // ADJUST THESE TO YOUR LIKING:
    const float alpha = 0.2f;      // Smoothing: 1.0 = no smoothing, 0.01 = very slow/smooth
    const int threshold = 3;       // Hysteresis: Only update if distance changes by > 3cm

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
        // Increase 60 to 100ms to reduce CPU/Sensor noise if needed
        add_repeating_timer_ms(100, timer_callback, this, &timer); 
    }

    bool changed() {
        if (!hw) return false;
        
        float raw_d = (float)hw->distance;

        // 1. Exponential Moving Average (Smoothing)
        // formula: smoothed = (alpha * current) + ((1 - alpha) * previous)
        smoothed_val = (alpha * raw_d) + ((1.0f - alpha) * smoothed_val);

        // 2. Hysteresis (Threshold check)
        if (std::abs(smoothed_val - current_val) > threshold) {
            current_val = smoothed_val;
            return true;
        }
        return false;
    }

    float getDistance() const { return current_val; }
};

extern SonicSensor dist_sensor;
void addDistanceSensor(uint32_t trig, uint32_t echo);
}
