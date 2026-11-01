#include <math.h>
#include "pico/stdlib.h"
#include "buffer_queue.h"

BufferQueue::BufferQueue(uint8_t buffer_count, size_t buffer_size) : buffer_count(buffer_count), buffer_size(buffer_size)
{
    assert(buffer_count > 0 && buffer_count < 32);
    assert(buffer_size > 0);

    buffer = new (std::align_val_t(32)) uint32_t[buffer_count * buffer_size];
}

void BufferQueue::clearBuffers()
{
    for (size_t i = 0; i < buffer_size; i++)
    {
        buffer[i] = 0;
    }
}

volatile uint32_t *BufferQueue::getWriteBuffer()
{
    if (isBufferReady(current_write_buffer))
    {
        return nullptr;
    }

    setBufferReady(current_write_buffer);

    current_write_buffer++;
    if (current_write_buffer >= buffer_count)
    {
        current_write_buffer = 0;
    }

    return &buffer[current_write_buffer * buffer_size];
}

const volatile uint32_t *const BufferQueue::getReadBuffer()
{
    if (!isBufferReady(current_read_buffer))
    {
        return nullptr;
    }

    clearBufferReady(current_read_buffer);

    uint32_t index = current_read_buffer;

    current_read_buffer++;
    if (current_read_buffer >= buffer_count)
    {
        current_read_buffer = 0;
    }

    if (preReadCallback != nullptr)
    {
        preReadCallback(&buffer[index * buffer_size], buffer_size);
    }

    return &buffer[index * buffer_size];
}

void BufferQueue::setPreReadCallback(BufferCallback preReadCallback)
{
    this->preReadCallback = preReadCallback;
}

BufferQueue::~BufferQueue()
{
    delete[] buffer;
}
