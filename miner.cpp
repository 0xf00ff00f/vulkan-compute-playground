#include "vc.hpp"

#include <cassert>
#include <cstring>

using namespace std::string_view_literals;

constexpr std::string_view Charset = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"sv;

using Hash = std::array<unsigned char, 32>;

bool operator<(const Hash &lhs, const Hash &rhs)
{
    for (std::size_t i = 0; i < 32; ++i)
    {
        if (lhs[i] < rhs[i])
            return true;
        if (rhs[i] < lhs[i])
            return false;
    }
    return false;
}

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
    Hash m_bestHash;
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
    m_bestHash.fill(0xff);
    m_batchSize = 0;

    std::string message;
    message.resize(prefix.size() + nonceSize);
    std::copy(prefix.begin(), prefix.end(), message.begin());
    enumerate(message, prefix.size());

    doCompute(message.size());
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

    m_program.dispatch((m_batchSize + LocalSize - 1) / LocalSize, 1, 1);

    for (std::size_t index = 0; index < m_batchSize; ++index)
    {
        std::array<uint8_t, 32> hash;
        const auto *state = reinterpret_cast<uint8_t *>(&m_state[index * 8]);
        std::copy(state, state + 32, hash.begin());
        if (hash < m_bestHash)
        {
            const auto *message = reinterpret_cast<uint8_t *>(&m_data[index * 16]);
            std::printf("%.*s: ", static_cast<int>(messageSize), message);
            for (auto c : hash)
                std::printf("%02x", c);
            std::printf("\n");
            m_bestHash = hash;
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
