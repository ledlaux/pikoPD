/*

An extended version of PicoOSC that adds OSCServer function

https://github.com/madskjeldgaard/PicoOSC


MIT License

Copyright (c) 2023 Mads Kjeldgaard
Copyright (c) 2026 Vadims Maksimovs

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

*/

#pragma once

#include <climits>
#include <cstdint>
#include <cstring>
#include <type_traits>

#include "lwip/pbuf.h"
#include "lwip/udp.h"

namespace picoosc {

static constexpr size_t MAX_MESSAGE_SIZE   = 1024;
static constexpr size_t MAX_ADDRESS_SIZE   = 255;
static constexpr size_t MAX_TYPE_TAG_SIZE  = 255;
static constexpr size_t MAX_ARGUMENTS      = 64;
static constexpr size_t MAX_ARGUMENT_SIZE  = 1024;
static constexpr size_t MAX_BUNDLE_SIZE    = 1024;
static constexpr size_t MAX_MESSAGES       = 255;
static constexpr size_t MAX_TIMESTAMP_SIZE = 8;
static constexpr size_t MAX_TIMETAG_SIZE   = 8;

template<typename T>
T swap_endian(T u) {
    static_assert(CHAR_BIT == 8, "CHAR_BIT != 8");
    union {
        T u;
        unsigned char u8[sizeof(T)];
    } source, dest;
    source.u = u;
    for (size_t k = 0; k < sizeof(T); k++)
        dest.u8[k] = source.u8[sizeof(T) - k - 1];
    return dest.u;
}

class OSCServer {
public:
    OSCServer(uint16_t port, udp_recv_fn callback) {
        mPcb = udp_new();
        if (mPcb) {
            if (udp_bind(mPcb, IP_ADDR_ANY, port) == ERR_OK) {
                udp_recv(mPcb, callback, nullptr);
            } else {
                udp_remove(mPcb);
                mPcb = nullptr;
            }
        }
    }

    ~OSCServer() {
        if (mPcb) udp_remove(mPcb);
    }

    bool isOpen() const { return mPcb != nullptr; }

private:
    struct udp_pcb* mPcb;
};

class OSCClient {
public:
    OSCClient(const char* address, uint16_t port) {
        ipaddr_aton(address, &mAddr);
        mPort = port;
        mPcb = udp_new();
    }

    ~OSCClient() {
        if (mPcb) udp_remove(mPcb);
    }

    int send(const char* buffer, uint16_t size) {
        struct pbuf* p = pbuf_alloc(PBUF_TRANSPORT, size, PBUF_RAM);
        if (!p) return 1;
        std::memcpy(p->payload, buffer, size);
        const auto error = udp_sendto(mPcb, p, &mAddr, mPort);
        pbuf_free(p);
        return (error == ERR_OK) ? 0 : 1;
    }

private:
    udp_pcb* mPcb;
    ip_addr_t mAddr;
    uint16_t mPort;
};

class OSCMessage {
public:
    OSCMessage() : mBufferSize(0) {
        clear();
    }

    void clear() {
        mBufferSize = 0;
        std::memset(mBuffer, 0, MAX_MESSAGE_SIZE);
    }

    void addAddress(const char* address) {
        if (std::strlen(address) > MAX_ADDRESS_SIZE) return;
        std::memcpy(mBuffer + mBufferSize, address, std::strlen(address));
        mBufferSize += std::strlen(address);
        mBuffer[mBufferSize++] = '\0';
        padBuffer();
    }

    template<typename T>
    void add(T value) {
        if (mBufferSize + 8 > MAX_MESSAGE_SIZE) return;

        constexpr bool isFloat = std::is_same<T, float>::value;
        constexpr bool isInt = std::is_same<T, int32_t>::value;

        mBuffer[mBufferSize++] = ',';
        if constexpr (isFloat) mBuffer[mBufferSize++] = 'f';
        else if constexpr (isInt) mBuffer[mBufferSize++] = 'i';
        mBuffer[mBufferSize++] = '\0'; 
        padBuffer();

        T swapped = swap_endian<T>(value);
        std::memcpy(mBuffer + mBufferSize, &swapped, sizeof(T));
        mBufferSize += sizeof(T);
        padBuffer();
    }

    const char* data() const { return mBuffer; }
    std::size_t size() const { return mBufferSize; }

private:
    void padBuffer() {
        while (mBufferSize % 4 != 0) {
            mBuffer[mBufferSize++] = '\0';
        }
    }
    std::size_t mBufferSize;
    char mBuffer[MAX_MESSAGE_SIZE];
};

} 

