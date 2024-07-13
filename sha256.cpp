import vc;

#include <array>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <span>
#include <string_view>

std::array<uint8_t, 32> sha256(std::string_view message)
{
    assert(message.size() < 56);

    std::array<uint32_t, 16> data;
    data.fill(0);
    {
        auto *u8 = reinterpret_cast<uint8_t *>(data.data());
        std::memcpy(u8, message.data(), message.size());
        u8[message.size()] = 0x80;
        data[15] = __builtin_bswap32(message.size() * 8);
    }

    const std::array<uint32_t, 8> state = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                                           0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};

    vc::Instance instance;
    auto device = std::move(instance.devices().at(0));

    vc::Buffer<uint32_t> dataBuffer(&device, data);
    vc::Buffer<uint32_t> stateBuffer(&device, state);

    vc::Program program(&device, "sha256.comp.spv");
    program.bind(stateBuffer, dataBuffer);
    program.dispatch(1, 1, 1);

    std::array<uint8_t, 32> hash;
    {
        const auto bufferData = stateBuffer.map();
        auto *hashData = reinterpret_cast<uint32_t *>(hash.data());
        for (std::size_t i = 0; i < 8; ++i)
            hashData[i] = __builtin_bswap32(bufferData[i]);
        stateBuffer.unmap();
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
