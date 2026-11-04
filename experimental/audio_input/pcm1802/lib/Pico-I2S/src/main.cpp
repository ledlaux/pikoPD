#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "hardware/pio.h"
#include "buffer_queue.h"
#include "dma_reader.h"
#include "dma_writer.h"
#include "i2s_master_clock.h"
#include "i2s_master_input.h"
#include "i2s_master_output.h"

void buffer_pre_read_callback(volatile uint32_t *buffer, size_t buffer_size)
{
    // The I2S data is read shifted by one bit to the right, so we will correct it here
    for (size_t i = 0; i < buffer_size; i++)
    {
        buffer[i] = buffer[i] << 1;
    }
}

int main()
{
    const uint32_t sys_freq = 192000000UL;
    set_sys_clock_hz(sys_freq, true);

    stdio_init_all();

#ifndef NDEBUG
    printf("clk_sys = %u Hz\n", clock_get_hz(clk_sys));
#endif

    const uint32_t DCDC_PSM_CTRL_PIN = 23;

    gpio_init(DCDC_PSM_CTRL_PIN);
    gpio_set_dir(DCDC_PSM_CTRL_PIN, GPIO_OUT);
    gpio_put(DCDC_PSM_CTRL_PIN, 1);

    const PIO pio = pio0;

    const uint8_t SCK_PIN = 16;

    const uint8_t IN_BCK_PIN = 17;
    const uint8_t IN_LRCK_PIN = 18;
    const uint8_t DIN_PIN = 19;

    const uint8_t OUT_BCK_PIN = 20;
    const uint8_t OUT_LRCK_PIN = 21;
    const uint8_t DOUT_PIN = 22;

    const uint32_t sample_freq = 48000;

    I2SMasterClock clock(pio, pio_claim_unused_sm(pio, true), SCK_PIN, sample_freq, 256);
    I2SMasterInput input(pio, pio_claim_unused_sm(pio, true), IN_BCK_PIN, DIN_PIN, sample_freq, 32);
    I2SMasterOutput output(pio, pio_claim_unused_sm(pio, true), OUT_BCK_PIN, DOUT_PIN, sample_freq, 32);

    BufferQueue buffer_queue(8, 1024);
    buffer_queue.clearBuffers();
    buffer_queue.setPreReadCallback(buffer_pre_read_callback);

    // Mark some buffers as ready so that DMA has something to read
    for (size_t i = 0; i < buffer_queue.buffer_count / 2; i++)
    {
        buffer_queue.getWriteBuffer();
    }

    DMAReader dma_reader(pio, input.sm, buffer_queue);
    DMAWriter dma_writer(pio, output.sm, buffer_queue);

    clock.enable();
    input.enable();
    output.enable();

    const uint8_t LED_PIN = 25;

    uint64_t previous_time = to_ms_since_boot(get_absolute_time());

    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);

    while (true)
    {
        uint64_t current_time = to_ms_since_boot(get_absolute_time());

        if (current_time - previous_time > 500)
        {
            gpio_xor_mask(1u << LED_PIN);
            previous_time = current_time;
        }
    }
}
