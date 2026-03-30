#pragma once

#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/timer.h"
#include "distance_sensor.h"
#include <cmath>
#include <cstdio> 


class DistanceSensorHandler {
private:
    float current_val = 0.0f;
    int last_reported = -1;
    DistanceSensor* hw = nullptr;
    struct repeating_timer timer;

    static bool timer_callback(struct repeating_timer *t) {
        auto* self = (DistanceSensorHandler*)t->user_data;
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
