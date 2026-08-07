/**
 * MIT License
 * 
 * Copyright (c) 2022 Daniel Garcia-Briseno
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <stdio.h>
#include "pico.h"
#include "pico/stdlib.h"
#include "sensor.pio.h"
#include "hardware/pio.h"

#define SENSE_COMPLETE_IRQ   0b0001

volatile uint32_t pio_counter = 0;

void print_debug_info() {
    printf("FSTAT: 0x%08X\n", pio0_hw->fstat);
    printf("FDEBUG: 0x%08X\n", pio0_hw->fdebug);
}

void sense_irq() {
    uint32_t irq = pio0_hw->irq;
    printf("Working with irq: 0x%08X\n", irq);
    if (irq & SENSE_COMPLETE_IRQ) {
        int count = pio_sm_get_rx_fifo_level(pio0, 0);
        printf("There are %d items in the rx fifo\n", count);
        if (count > 0) {
            pio_counter = (~pio_sm_get(pio0, 0) * 2);
        }
    }

    hw_clear_bits(&pio0_hw->irq, irq);
}

void begin_sense() {
    pio_sm_put_blocking(pio0, 0, 0xFFFFFFFF);
}

int main() {
    stdio_init_all();
    sleep_ms(5000);
    puts("Testing the HC-SR04 distance sensor with PIO");
    PIO pio = pio0;
    int sm = 0;
    uint offset = pio_add_program(pio, &distance_sensor_program);
    printf("Loaded program to pio offset 0x%08X\n", offset);
    distance_sensor_program_init(pio, sm, offset, 0, sense_irq);

    uint count = 0;
    while (true) {
        printf("Sense count %d: \n", count++);
        begin_sense();
        // Wait for ISR to notify us that the sense is done.
        while (pio_counter == 0) {
            sleep_us(100);
        }
        printf("Got counter value %d\n", pio_counter);
        printf("Sensor distance %d\n", pio_counter / 58);
        pio_counter = 0;

        sleep_ms(1000);
    }
}

