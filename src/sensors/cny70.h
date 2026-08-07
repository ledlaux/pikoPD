#pragma once

#include "pico/stdlib.h"
#include "hardware/adc.h"
#include <cmath>

namespace Pico {

    struct CNY70 {
        int adc_ch;
        float smooth_value;
        float last_val; 
        float alpha;
        int threshold;
        int max_sensor;
        int dead_zone;
        int output_id;
    };

    inline CNY70 cny70[1];
    inline int n_cny70 = 0;

    // Helper declaration assuming start_adc() is available globally
    void start_adc();

    inline void addCNY70(int pin, int threshold, int max_sensor, float alpha, int dead_zone, int output_id) {
        start_adc();
        adc_gpio_init(pin);
        auto &s = cny70[0];
        s.adc_ch = pin - 26;
        s.threshold = threshold;
        s.max_sensor = max_sensor;
        s.alpha = alpha;
        s.dead_zone = dead_zone;
        s.output_id = output_id;
        s.last_val = -1.0f; 
        s.smooth_value = 0.0f;
        n_cny70++;
    }

    inline bool processCNY70(int i, float &outVal, float &rawOut) {
        if (i < 0 || i >= n_cny70) return false;
        auto &s = cny70[i];
    
        adc_select_input(s.adc_ch);
        uint32_t sum = 0;
        for (int j = 0; j < 16; j++) {
            sum += adc_read();
        }
        
        // Average of 16 samples scaled up to 12-bit ADC range equivalent (sum / 16.0f * 4.0f)
        rawOut = (float)sum / 64.0f; 
    
        // Calculate normalized value above threshold
        float current_norm = 0.0f;
        if (rawOut > (float)s.threshold) {
            float range = fmaxf(1.0f, (float)s.max_sensor - (float)s.threshold);
            current_norm = fminf(1.0f, (rawOut - (float)s.threshold) / range);
        }
    
        // Exponential moving average filter
        if (s.last_val < -0.5f) {
            s.smooth_value = current_norm;
            s.last_val = 0.0f;
        } else {
            s.smooth_value += (current_norm - s.smooth_value) * s.alpha;
        }
    
        // Dead-zone and boundary check to trigger updates
        float dz = (float)s.dead_zone * 0.001f;
        if (fabsf(s.smooth_value - s.last_val) > dz || s.smooth_value == 0.0f || s.smooth_value == 1.0f) {
            s.last_val = s.smooth_value;
            outVal = s.smooth_value;
            return true;
        }
    
        return false;
    }

} // namespace Pico