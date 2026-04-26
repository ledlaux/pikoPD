#include "pio.h"

/**
 * Simulates adding a program to the PIO
 * Using an ID means that each time a program is loaded, it will
 * have a different result which can be used to verify that it is
 * distinct from a separate program that was loaded
 * @return ID representing the offset into memory for a given program
 */
uint pio_add_program(PIO pio, const pio_program_t*  program) {
    static uint simulated_offset = 0;
    return simulated_offset++;
};

uint32_t pio_sm_get(PIO pio, uint sm) {
    return 0xDEADBEEF;
}

bool pio_interrupt_get(PIO pio, uint sm) {
    return true;
}

