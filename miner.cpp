#include "vc.hpp"

extern "C" {
#include "sha256.h"
}

#include <cassert>
#include <chrono>
#include <cstring>

using namespace std::string_view_literals;

constexpr std::string_view Charset = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"sv;

class Miner
{
public:
    explicit Miner(vc::Device *device);
    ~Miner();

    void search(std::string_view prefix);

private:
    static constexpr auto BatchSize = 65536;
    static constexpr auto LocalSize = 256;
    static constexpr auto NonceSize = 8;

    int dumpResult(std::string_view prefix, uint32_t nonceIndex) const;

    struct Input
    {
        uint32_t minLeadingZeros;
        uint32_t nonceIndexBase;
        uint32_t prefixSize;
        uint32_t messagePrefix[16];
    };

    vc::Device *m_device;
    vc::Program m_program;
    vc::Buffer m_inputBuffer;
    vc::Buffer m_resultBuffer;
    Input *m_input{nullptr};
    uint32_t *m_result{nullptr};
    std::size_t m_batchSize{0};
    std::size_t m_hashCount{0};
};

Miner::Miner(vc::Device *device)
    : m_device(device)
    , m_program(m_device, "sha256-miner.comp.spv")
    , m_inputBuffer(m_device, /* sizeof(Input) */ 256)
    , m_resultBuffer(m_device, /* sizeof(uint32_t) */ 256)
{
    m_program.bind(m_inputBuffer, m_resultBuffer);
    m_input = reinterpret_cast<Input *>(m_inputBuffer.map());
    m_result = reinterpret_cast<uint32_t *>(m_resultBuffer.map());
}

Miner::~Miner()
{
    m_inputBuffer.unmap();
    m_resultBuffer.unmap();
}

void Miner::search(std::string_view prefix)
{
    std::array<uint32_t, 16> message;
    const std::size_t messageSize = prefix.size() + NonceSize;
    message.fill(0);
    {
        auto *messageU8 = reinterpret_cast<uint8_t *>(message.data());
        std::copy(prefix.begin(), prefix.end(), messageU8);
        messageU8[messageSize] = 0x80;
    }
    message[15] = __builtin_bswap32(messageSize * 8);

    m_batchSize = 0;
    m_hashCount = 0;

    const auto timeStart = std::chrono::steady_clock::now();

    uint32_t nonceIndexBase = 0;
    uint32_t minLeadingZeros = 4;

    for (int i = 0; i < 65536; ++i)
    {
        m_input->minLeadingZeros = minLeadingZeros;
        m_input->nonceIndexBase = nonceIndexBase;
        m_input->prefixSize = prefix.size();
        std::copy(message.begin(), message.end(), m_input->messagePrefix);

        *m_result = ~0u;

        const auto groupCount = (BatchSize + LocalSize - 1) / LocalSize;
        m_program.dispatch(groupCount / LocalSize, 1, 1);
        m_hashCount += BatchSize;

        if (*m_result != ~0u)
        {
            int leadingZeros = dumpResult(prefix, *m_result);
            minLeadingZeros = leadingZeros + 1;
        }

        nonceIndexBase += BatchSize;
    }

    const auto timeEnd = std::chrono::steady_clock::now();
    const auto elapsed = timeEnd - timeStart;
    const auto hashesPerSec = static_cast<double>(m_hashCount) * 1000 /
                              std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count() / 1000000;
    std::printf("%lu hashes, %lu ms (%.2f Mhashes/sec)\n", m_hashCount,
                std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count(), hashesPerSec);
}

int Miner::dumpResult(std::string_view prefix, uint32_t nonceIndex) const
{
    constexpr std::array<char, 16> Charset = {0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
                                              0x38, 0x39, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46};
    std::string message;
    message.resize(prefix.size() + NonceSize);
    std::copy(prefix.begin(), prefix.end(), message.begin());
    for (std::size_t i = 0; i < 8; ++i)
    {
        message[prefix.size() + i] = Charset[(nonceIndex >> (4 * i)) & 0xf];
    }

    std::array<BYTE, 32> hash;
    SHA256_CTX ctx;
    sha256_init(&ctx);
    sha256_update(&ctx, reinterpret_cast<const BYTE *>(message.data()), message.size());
    sha256_final(&ctx, hash.data());

    int leadingZeros = 0;
    bool done = false;
    for (auto b : hash)
    {
        for (int i = 7; i >= 0; --i)
        {
            if (b & (1 << i))
            {
                done = true;
                break;
            }
            ++leadingZeros;
        }
        if (done)
            break;
    }

    std::printf("%s: ", message.c_str());
    for (auto b : hash)
        std::printf("%02x", b);
    std::printf("\n");

    return leadingZeros;
}

int main()
{
    vc::Instance instance;
    auto device = std::move(instance.devices().at(1));

    const std::string_view prefix = "hello/";

    Miner miner(&device);
    miner.search(prefix);
}
