#pragma once
/**
 * Stubs for testing
 */
#include "pico.h"
#include "hardware/pio.h"

// Stub program loader.
void distance_sensor_program_init(PIO pio, uint sm, uint offset, uint pins, irq_handler_t isr);

extern pio_program_t distance_sensor_program;
