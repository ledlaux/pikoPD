#pragma once

#include "hardware/pio.h"
#include "buffer_queue.h"

class DMAReader
{
private:
    static BufferQueue *buffer_queues[NUM_DMA_CHANNELS];

    static void __isr dma_handler();

    DMAReader(const DMAReader &) = delete;

public:
    const PIO pio;
    const uint8_t sm;
    const uint8_t dma_channel;

    DMAReader(PIO pio, uint8_t sm, BufferQueue &buffer_queue);

    ~DMAReader();
};
