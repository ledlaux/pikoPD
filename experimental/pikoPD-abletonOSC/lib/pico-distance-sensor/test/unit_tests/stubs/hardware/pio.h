/**
 * Stub header
 */

#pragma once

#include "pico.h"
typedef uint PIO;

#define pio0 0
#define pio1 0

/**                                                                                 
 * Interrupt request handler function type                                          
 */                                                                                 
typedef void(* irq_handler_t) (void);

/**
 * Struct for a pio program
 */
typedef struct {                                                                    
    const uint16_t * instructions;                                                  
    uint8_t length;                                                                 
    int8_t origin;                                                                  
} pio_program_t;                                                                    

uint pio_add_program(PIO pio, const pio_program_t*  program);
uint32_t pio_sm_get(PIO pio, uint sm);
void test_set_irq_pending(PIO pio, uint sm);
bool pio_interrupt_get(PIO pio, uint sm);
void pio_sm_put(PIO pio, uint sm, uint32_t data);
