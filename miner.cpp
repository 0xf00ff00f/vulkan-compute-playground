#include "vc.hpp"

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

    void search(std::string_view prefix, std::size_t nonceSize);

private:
    void enumerate(std::string &message, std::size_t index);
    void doCompute(std::size_t messageSize);

    static constexpr auto BatchSize = 65536;
    static constexpr auto LocalSize = 256;

    vc::Device *m_device;
    vc::Program m_program;
    vc::Buffer m_dataBuffer;
    vc::Buffer m_stateBuffer;
    uint32_t *m_data{nullptr};
    uint32_t *m_state{nullptr};
    std::size_t m_batchSize{0};
    std::array<uint32_t, 8> m_bestHash;
    std::size_t m_hashCount{0};
};

Miner::Miner(vc::Device *device)
    : m_device(device)
    , m_program(m_device, "sha256-miner.comp.spv")
    , m_dataBuffer(m_device, BatchSize * 16 * sizeof(uint32_t))
    , m_stateBuffer(m_device, BatchSize * 8 * sizeof(uint32_t))
{
    m_program.bind(m_stateBuffer, m_dataBuffer);
    m_data = reinterpret_cast<uint32_t *>(m_dataBuffer.map());
    m_state = reinterpret_cast<uint32_t *>(m_stateBuffer.map());
}

Miner::~Miner()
{
    m_dataBuffer.unmap();
    m_stateBuffer.unmap();
}

void Miner::search(std::string_view prefix, std::size_t nonceSize)
{
    m_bestHash.fill(~0u);
    m_batchSize = 0;
    m_hashCount = 0;

    const auto timeStart = std::chrono::steady_clock::now();

    std::string message;
    message.resize(prefix.size() + nonceSize);
    std::copy(prefix.begin(), prefix.end(), message.begin());
    enumerate(message, prefix.size());

    doCompute(message.size());

    const auto timeEnd = std::chrono::steady_clock::now();
    const auto elapsed = timeEnd - timeStart;
    const auto hashesPerSec = static_cast<double>(m_hashCount) * 1000 /
                              std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count() / 1000000;
    std::printf("%lu hashes, %lu ms (%.2f Mhashes/sec)\n", m_hashCount,
                std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count(), hashesPerSec);
}

void Miner::enumerate(std::string &message, std::size_t index)
{
    if (index == message.size())
    {
        uint32_t *data = &m_data[m_batchSize * 16];
        {
            assert(message.size() < 56);
            auto *u8Data = reinterpret_cast<uint8_t *>(data);
            std::memset(u8Data, 0, 16 * sizeof(uint32_t));
            std::memcpy(u8Data, message.data(), message.size());
            u8Data[message.size()] = 0x80;
            data[15] = __builtin_bswap32(message.size() * 8);
        }

        ++m_batchSize;
        if (m_batchSize == BatchSize)
            doCompute(message.size());
    }
    else
    {
        for (auto c : Charset)
        {
            message[index] = c;
            enumerate(message, index + 1);
        }
    }
}

void Miner::doCompute(std::size_t messageSize)
{
    if (m_batchSize == 0)
        return;

    const auto groupCount = (m_batchSize + LocalSize - 1) / LocalSize;
    m_program.dispatch((m_batchSize + LocalSize - 1) / LocalSize, 1, 1);
    m_hashCount += groupCount * LocalSize;

    for (std::size_t index = 0; index < m_batchSize; ++index)
    {
        const auto *hash = &m_state[index * 8];
        const auto isBest = [this, hash]() -> bool {
            for (std::size_t i = 0; i < 8; ++i)
            {
                if (hash[i] < m_bestHash[i])
                    return true;
                if (m_bestHash[i] < hash[i])
                    return false;
            }
            return false;
        }();
        if (isBest)
        {
            const auto *message = reinterpret_cast<uint8_t *>(&m_data[index * 16]);
            std::printf("%.*s: ", static_cast<int>(messageSize), message);
            for (std::size_t i = 0; i < 8; ++i)
                std::printf("%08x", hash[i]);
            std::printf("\n");
            std::copy(hash, hash + 8, m_bestHash.begin());
        }
    }

    m_batchSize = 0;
}

int main()
{
    vc::Instance instance;
    auto device = std::move(instance.devices().at(0));

    const std::string_view prefix = "hello/";

    Miner miner(&device);
    miner.search(prefix, 4);
}
