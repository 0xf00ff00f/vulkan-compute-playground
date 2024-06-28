#include "vc.hpp"

#include <cassert>
#include <cstring>
#include <numeric>

std::array<uint8_t, 32> sha256(std::string_view message)
{
    std::array<uint32_t, 16> data;
    data.fill(0);
    {
        auto *u8 = reinterpret_cast<uint8_t *>(data.data());
        std::memcpy(u8, message.data(), message.size());
        u8[message.size()] = 0x80;
        for (std::size_t i = 0; i < 15; ++i)
            data[i] = __builtin_bswap32(data[i]);
        data[15] = message.size() * 8;
    }

    const std::array<uint32_t, 8> state = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                                           0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};

    vc::Instance instance;
    auto device = std::move(instance.devices().at(0));

    vc::Buffer dataBuffer(&device, data.size() * sizeof(uint32_t));
    {
        auto *bufferData = reinterpret_cast<uint32_t *>(dataBuffer.map());
        std::copy(data.begin(), data.end(), bufferData);
        dataBuffer.unmap();
    }

    vc::Buffer stateBuffer(&device, state.size() * sizeof(uint32_t));
    {
        auto *bufferData = reinterpret_cast<uint32_t *>(stateBuffer.map());
        std::copy(state.begin(), state.end(), bufferData);
        stateBuffer.unmap();
    }

    vc::Program program(&device, "sha256.comp.spv");
    program.bindBuffers(stateBuffer, dataBuffer);
    program.dispatch(1, 1, 1);

    std::array<uint8_t, 32> hash;
    {
        const auto *bufferData = reinterpret_cast<const uint32_t *>(stateBuffer.map());
        auto *hashData = reinterpret_cast<uint32_t *>(hash.data());
        for (std::size_t i = 0; i < 8; ++i)
            hashData[i] = __builtin_bswap32(bufferData[i]);
    }
    return hash;
}

int main()
{
    const auto hash = sha256("hello");
    for (auto c : hash)
        std::printf("%02x", c);
    std::putchar('\n');
}
