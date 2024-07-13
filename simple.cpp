import vc;

#ifdef USE_RENDERDOC
#include <renderdoc_app.h>
#endif

#include <cassert>
#include <cstdio>
#include <dlfcn.h>
#include <numeric>
#include <span>

int main()
{
    vc::Instance instance;
    auto device = std::move(instance.devices().at(0));

#ifdef USE_RENDERDOC
    RENDERDOC_API_1_1_2 *renderDoc = nullptr;
    if (auto *handle = dlopen("librenderdoc.so", RTLD_NOW | RTLD_NOLOAD))
    {
        auto *getAPI = reinterpret_cast<pRENDERDOC_GetAPI>(dlsym(handle, "RENDERDOC_GetAPI"));
        auto result = getAPI(eRENDERDOC_API_Version_1_1_2, reinterpret_cast<void **>(&renderDoc));
        assert(result == 1);
    }

    if (renderDoc)
        renderDoc->StartFrameCapture(nullptr, nullptr);
#endif

    constexpr auto Size = 32;

    vc::Buffer<float> inBuffer(&device, Size);
    {
        auto values = inBuffer.map();
        std::iota(values.begin(), values.end(), 1);
        inBuffer.unmap();
    }
    vc::Buffer<float> outBuffer(&device, Size);

    vc::Program program(&device, "simple.comp.spv");
    program.bind(inBuffer, outBuffer);

    constexpr auto ThreadCount = 16;
    constexpr auto BlockCount = (Size + ThreadCount - 1) / ThreadCount;
    program.dispatch(BlockCount, 1, 1);
    {
        const auto values = outBuffer.map();
        for (std::size_t i = 0; i < Size; ++i)
            std::printf("%lu: %f\n", i, values[i]);
        outBuffer.unmap();
    }

#ifdef USE_RENDERDOC
    if (renderDoc)
        renderDoc->EndFrameCapture(nullptr, nullptr);
#endif
}
