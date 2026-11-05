#include "sensor.pio.h"

void distance_sensor_program_init(PIO pio, uint sm, uint offset, uint pins, irq_handler_t isr) {}

// Using arbitrary values to simulate a test program                                
// for testing
pio_program_t distance_sensor_program = {
    (uint16_t*)0xDEAD, 0x10, 0x10
};
