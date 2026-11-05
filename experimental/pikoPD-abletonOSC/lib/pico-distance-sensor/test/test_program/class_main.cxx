#include <stdio.h>
#include "pico/stdlib.h"
#include "distance_sensor.h"

void starting_countdown() {
    for (uint i = 5; i != 0; i--) {
        printf("Starting in %d\n", i);
        sleep_ms(1000);
    }
}

int main() {
    stdio_init_all();
    starting_countdown();
    DistanceSensor sensor{pio0, 0, 0};

    while (true) {
        sensor.TriggerRead();
        while (sensor.is_sensing) {
            sleep_us(10);
        }
        printf("Got reading: %u\n", sensor.distance);
        sleep_ms(1000);
    }
}
