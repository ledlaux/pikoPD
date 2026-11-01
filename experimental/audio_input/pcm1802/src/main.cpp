#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

#include "pico/stdlib.h"
#include "pico/multicore.h"

#include "hardware/clocks.h"
#include "hardware/pio.h"
#include "hardware/sync.h"

#include "buffer_queue.h"
#include "dma_reader.h"
#include "dma_writer.h"
#include "i2s_master_clock.h"
#include "i2s_master_input.h"
#include "i2s_master_output.h"

#include "Heavy_audio.hpp"

// ---------------------------------------------------------
// Heavy DSP configuration
// ---------------------------------------------------------

#define SAMPLE_RATE 22050 //rp2040 
#define BLOCK_SIZE  64    //rp2040 don't change

#define AUDIO_BUFFER_WORDS 2048

#define DSP_QUEUE_COUNT 4

// ---------------------------------------------------------
// Heavy output startup fade
// ---------------------------------------------------------

#define HEAVY_OUTPUT_FADE_MS 250

static uint32_t heavy_output_fade_position = 0;

static const uint32_t heavy_output_fade_samples =
    SAMPLE_RATE * HEAVY_OUTPUT_FADE_MS / 1000;

// ---------------------------------------------------------
// Heavy DSP instance
// ---------------------------------------------------------

Heavy_audio pd_prog((double)SAMPLE_RATE);

// ---------------------------------------------------------
// Audio queues shared between cores
// ---------------------------------------------------------

// Core 0 produces input buffers.
// Core 1 consumes input buffers.

static uint32_t input_queue[DSP_QUEUE_COUNT][AUDIO_BUFFER_WORDS];

static volatile uint32_t input_write_index = 0;
static volatile uint32_t input_read_index = 0;

// Core 1 produces processed buffers.
// Core 0 consumes processed buffers.

static uint32_t output_queue[DSP_QUEUE_COUNT][AUDIO_BUFFER_WORDS];

static volatile uint32_t output_write_index = 0;
static volatile uint32_t output_read_index = 0;

// Diagnostics.
static volatile uint32_t input_queue_drops = 0;
static volatile uint32_t output_underruns = 0;

// ---------------------------------------------------------
// PCM conversion
// ---------------------------------------------------------

static inline float i2s_to_float(uint32_t sample)
{
    return (float)((int32_t)sample) *
           (1.0f / 2147483648.0f);
}

static inline uint32_t float_to_i2s(float sample)
{
    if (!isfinite(sample))
        return 0;

    if (sample >= 1.0f)
        sample = 0.9999999995f;

    if (sample < -1.0f)
        sample = -1.0f;

    const int64_t value =
        (int64_t)((double)sample * 2147483648.0);

    return (uint32_t)(int32_t)value;
}

// ---------------------------------------------------------
// Heavy processing buffers
// ---------------------------------------------------------

static float hv_input[BLOCK_SIZE * 2];
static float hv_output[BLOCK_SIZE * 2];

// ---------------------------------------------------------
// Process one Heavy block
// ---------------------------------------------------------

static void processAudioBlock(
    const volatile uint32_t *input,
    volatile uint32_t *output)
{
    // -----------------------------------------------------
    // Convert interleaved I2S input to planar Heavy input.
    // PCM1802 active sample is the second word per frame.
    // -----------------------------------------------------

    for (int i = 0; i < BLOCK_SIZE; i++)
    {
        uint32_t raw = input[i * 2 + 1] << 1;

        float sample = i2s_to_float(raw);

        // Duplicate mono input to Heavy stereo input.
        hv_input[i] = sample;
        hv_input[BLOCK_SIZE + i] = sample;
    }

    // -----------------------------------------------------
    // Heavy DSP
    // -----------------------------------------------------

    pd_prog.processInline(
        hv_input,
        hv_output,
        BLOCK_SIZE
    );

    // -----------------------------------------------------
    // Fade Heavy output and convert to interleaved I2S.
    // Fade position advances once per stereo frame.
    // -----------------------------------------------------

    for (int i = 0; i < BLOCK_SIZE; i++)
    {
        float gain = 1.0f;

        if (heavy_output_fade_position < heavy_output_fade_samples)
        {
            gain = (float)heavy_output_fade_position /
                   (float)heavy_output_fade_samples;

            heavy_output_fade_position++;
        }

        float left = hv_output[i] * gain;
        float right = hv_output[BLOCK_SIZE + i] * gain;

        output[i * 2] =
            float_to_i2s(left);

        output[i * 2 + 1] =
            float_to_i2s(right);
    }
}

// ---------------------------------------------------------
// Core 1: asynchronous Heavy DSP worker
// ---------------------------------------------------------

static void heavyProcessingCore()
{
    while (true)
    {
        // -------------------------------------------------
        // Wait for an input buffer.
        // -------------------------------------------------

        uint32_t input_read = input_read_index;

        while (input_read == input_write_index)
        {
            __wfe();

            input_read = input_read_index;
        }

        __dmb();

        // -------------------------------------------------
        // Wait until an output slot is available.
        // -------------------------------------------------

        uint32_t output_write = output_write_index;

        while ((uint32_t)(
                   output_write - output_read_index
               ) >= DSP_QUEUE_COUNT)
        {
            __wfe();

            output_write = output_write_index;
        }

        __dmb();

        // -------------------------------------------------
        // Get queue buffers.
        // -------------------------------------------------

        const uint32_t input_slot =
            input_read % DSP_QUEUE_COUNT;

        const uint32_t output_slot =
            output_write % DSP_QUEUE_COUNT;

        const uint32_t *src =
            input_queue[input_slot];

        uint32_t *dst =
            output_queue[output_slot];

        // -------------------------------------------------
        // Process complete audio buffer.
        // -------------------------------------------------

        const size_t heavy_block_words =
            BLOCK_SIZE * 2;

        for (size_t offset = 0;
             offset + heavy_block_words <= AUDIO_BUFFER_WORDS;
             offset += heavy_block_words)
        {
            processAudioBlock(
                &src[offset],
                &dst[offset]
            );
        }

        // -------------------------------------------------
        // Publish completed output buffer.
        // -------------------------------------------------

        __dmb();

        output_write_index = output_write + 1;

        // Release consumed input buffer.
        __dmb();

        input_read_index = input_read + 1;
    }
}

// ---------------------------------------------------------
// DMA callback
//
// Called before DMAWriter starts transmitting a buffer.
// No Heavy processing or waiting here.
// ---------------------------------------------------------

void buffer_pre_read_callback(
    volatile uint32_t *buffer,
    size_t buffer_size)
{
    if (buffer_size != AUDIO_BUFFER_WORDS)
    {
        for (size_t i = 0; i < buffer_size; i++)
        {
            buffer[i] = 0;
        }

        output_underruns++;
        return;
    }

    // -----------------------------------------------------
    // 1. Submit captured audio to core 1.
    // -----------------------------------------------------

    uint32_t input_write = input_write_index;
    uint32_t input_read = input_read_index;

    if ((uint32_t)(input_write - input_read) < DSP_QUEUE_COUNT)
    {
        uint32_t slot = input_write % DSP_QUEUE_COUNT;

        for (size_t i = 0; i < buffer_size; i++)
        {
            input_queue[slot][i] = buffer[i];
        }

        __dmb();

        input_write_index = input_write + 1;

        __sev();
    }
    else
    {
        input_queue_drops++;
    }

    // -----------------------------------------------------
    // 2. Retrieve processed audio from core 1.
    // -----------------------------------------------------

    uint32_t output_read = output_read_index;
    uint32_t output_write = output_write_index;

    if (output_read != output_write)
    {
        __dmb();

        uint32_t slot = output_read % DSP_QUEUE_COUNT;

        for (size_t i = 0; i < buffer_size; i++)
        {
            buffer[i] = output_queue[slot][i];
        }

        // Release processed output buffer.
        __dmb();

        output_read_index = output_read + 1;

        __sev();
    }
    else
    {
        // No processed audio available yet.
        for (size_t i = 0; i < buffer_size; i++)
        {
            buffer[i] = 0;
        }

        output_underruns++;
    }
}

// ---------------------------------------------------------
// Main
// ---------------------------------------------------------

int main()
{
    // -----------------------------------------------------
    // Clock configuration - unchanged
    // -----------------------------------------------------

    const uint32_t sys_freq = 250000000UL;

    set_sys_clock_hz(sys_freq, true);

    stdio_init_all();

#ifndef NDEBUG
    printf("clk_sys = %u Hz\n", clock_get_hz(clk_sys));
#endif

    // -----------------------------------------------------
    // DCDC power control - unchanged
    // -----------------------------------------------------

    const uint32_t DCDC_PSM_CTRL_PIN = 23;

    gpio_init(DCDC_PSM_CTRL_PIN);
    gpio_set_dir(DCDC_PSM_CTRL_PIN, GPIO_OUT);
    gpio_put(DCDC_PSM_CTRL_PIN, 1);

    // -----------------------------------------------------
    // PIO configuration - unchanged
    // -----------------------------------------------------

    const PIO pio = pio0;

    const uint8_t SCK_PIN = 16;

    const uint8_t IN_BCK_PIN = 17;
    const uint8_t IN_LRCK_PIN = 18;
    const uint8_t DIN_PIN = 19;

    const uint8_t OUT_BCK_PIN = 10;
    const uint8_t OUT_LRCK_PIN = 11;
    const uint8_t DOUT_PIN = 9;

    const uint32_t sample_freq = SAMPLE_RATE;

    I2SMasterClock clock(
        pio,
        pio_claim_unused_sm(pio, true),
        SCK_PIN,
        sample_freq,
        256
    );

    I2SMasterInput input(
        pio,
        pio_claim_unused_sm(pio, true),
        IN_BCK_PIN,
        DIN_PIN,
        sample_freq,
        32
    );

    I2SMasterOutput output(
        pio,
        pio_claim_unused_sm(pio, true),
        OUT_BCK_PIN,
        DOUT_PIN,
        sample_freq,
        32
    );

    // -----------------------------------------------------
    // Existing BufferQueue - unchanged
    // -----------------------------------------------------

    BufferQueue buffer_queue(8, AUDIO_BUFFER_WORDS);

    buffer_queue.clearBuffers();

    buffer_queue.setPreReadCallback(
        buffer_pre_read_callback
    );

    // Mark initial buffers as ready.
    for (size_t i = 0;
         i < buffer_queue.buffer_count / 2;
         i++)
    {
        buffer_queue.getWriteBuffer();
    }

    // -----------------------------------------------------
    // Initialize DSP buffers and queues.
    // -----------------------------------------------------

    memset(input_queue, 0, sizeof(input_queue));
    memset(output_queue, 0, sizeof(output_queue));

    memset(hv_input, 0, sizeof(hv_input));
    memset(hv_output, 0, sizeof(hv_output));

    // Reset queue indices and diagnostics.
    input_write_index = 0;
    input_read_index = 0;

    output_write_index = 0;
    output_read_index = 0;

    input_queue_drops = 0;
    output_underruns = 0;

    // Reset Heavy output fade.
    heavy_output_fade_position = 0;

    // -----------------------------------------------------
    // Launch Heavy DSP on core 1.
    // -----------------------------------------------------

    multicore_launch_core1(heavyProcessingCore);

    // -----------------------------------------------------
    // DMA configuration - unchanged
    // -----------------------------------------------------

    DMAReader dma_reader(
        pio,
        input.sm,
        buffer_queue
    );

    DMAWriter dma_writer(
        pio,
        output.sm,
        buffer_queue
    );

    // -----------------------------------------------------
    // Start I2S - unchanged.
    // Keep these calls adjacent and in this order.
    // -----------------------------------------------------

    clock.enable();
    input.enable();
    output.enable();

    // -----------------------------------------------------
    // LED
    // -----------------------------------------------------

    const uint8_t LED_PIN = 25;

    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);

    uint64_t previous_time =
        to_ms_since_boot(get_absolute_time());

    while (true)
    {
        uint64_t current_time =
            to_ms_since_boot(get_absolute_time());

        if (current_time - previous_time > 500)
        {
            gpio_xor_mask(1u << LED_PIN);

            previous_time = current_time;
        }

        tight_loop_contents();
    }
}

// ---------------------------------------------------------
// Pico atomic compatibility stubs 
// ---------------------------------------------------------

#if defined(PICO_PLATFORM) || defined(PICO_RP2040)

extern "C" {

bool __atomic_test_and_set(volatile void* ptr, int memorder)
{
    (void)memorder;

    bool old = *(volatile bool*)ptr;

    *(volatile bool*)ptr = true;

    return old;
}

void __atomic_clear(volatile void* ptr, int memorder)
{
    (void)memorder;

    *(volatile bool*)ptr = false;
}

}

#endif
