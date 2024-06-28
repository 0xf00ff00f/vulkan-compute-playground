#include "vc.h"

#include <numeric>

int main()
{
    vc::Instance instance;

    auto devices = instance.devices();
    for (const auto &device : devices)
    {
        printf("%p\n", static_cast<VkDevice>(device));

        vc::Buffer inBuffer(&device, 32 * sizeof(float));
        {
            auto bytes = inBuffer.map();
            auto *p = reinterpret_cast<float *>(bytes.data());
            std::iota(p, p + 32, 1);
            inBuffer.unmap();
        }
        vc::Buffer outBuffer(&device, 32 * sizeof(float));

        vc::Program program(&device, "simple.comp.spv");
        program.bindBuffers(inBuffer, outBuffer);

        program.dispatch(32, 1, 1);
        {
            const auto bytes = outBuffer.map();
            const auto *p = reinterpret_cast<const float *>(bytes.data());
            for (std::size_t i = 0; i < 32; ++i)
                std::printf("%lu: %f\n", i, p[i]);
            outBuffer.unmap();
        }
    }
}
