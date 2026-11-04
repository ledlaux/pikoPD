#include "pico/stdlib.h"
#include "hardware/dma.h"
#include "dma_reader.h"

BufferQueue *DMAReader::buffer_queues[NUM_DMA_CHANNELS] = {nullptr};

void __isr DMAReader::dma_handler()
{
    for (uint8_t dma_channel = 0; dma_channel < NUM_DMA_CHANNELS; dma_channel++)
    {
        if (!dma_channel_get_irq0_status(dma_channel))
        {
            continue;
        }

        BufferQueue *buffer_queue = buffer_queues[dma_channel];
        if (buffer_queue != nullptr)
        {
            dma_channel_acknowledge_irq0(dma_channel);

            volatile uint32_t *buffer = buffer_queue->getWriteBuffer();
            assert(buffer != nullptr);

            dma_channel_set_write_addr(dma_channel, buffer, false);
            dma_channel_set_trans_count(dma_channel, buffer_queue->buffer_size, true);
        }
    }
}

DMAReader::DMAReader(PIO pio, uint8_t sm, BufferQueue &buffer_queue) : pio(pio), sm(sm), dma_channel(dma_claim_unused_channel(true))
{
    dma_channel_config config = dma_channel_get_default_config(dma_channel);
    channel_config_set_transfer_data_size(&config, DMA_SIZE_32);
    channel_config_set_read_increment(&config, false);
    channel_config_set_write_increment(&config, true);
    channel_config_set_dreq(&config, pio_get_dreq(pio, sm, false));

    volatile uint32_t *buffer = buffer_queue.getWriteBuffer();
    assert(buffer != nullptr);

    dma_channel_configure(dma_channel, &config, buffer, &pio->rxf[sm], buffer_queue.buffer_size, false);

    buffer_queues[dma_channel] = &buffer_queue;

    dma_channel_set_irq0_enabled(dma_channel, true);
    irq_set_exclusive_handler(DMA_IRQ_0, dma_handler);
    irq_set_enabled(DMA_IRQ_0, true);

    dma_channel_start(dma_channel);
}

DMAReader::~DMAReader()
{
    dma_channel_set_irq0_enabled(dma_channel, false);

    buffer_queues[dma_channel] = nullptr;

    dma_channel_abort(dma_channel);
    dma_channel_unclaim(dma_channel);
}
