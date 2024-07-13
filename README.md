# Vulkan compute playground

## Huh?

`vc` is a small library to run compute shaders with Vulkan. I wrote it because:

1. I wanted to play with Vulkan;
2. I wanted to try my hand at [SHAllenge](https://shallenge.quirino.net/), which is basically a competition for the fastest SHA-256 hasher.

I started with the example [here](https://www.neilhenning.dev/posts/a-simple-vulkan-compute-example/) and went from there.

## Building

This uses C++20 modules, so it requires a fairly new tool set. I used clang 18, cmake 3.28 and ninja 1.11.

## Where is the SHAllenge miner?

See `miner.cpp`. Note that, as it is, it will only compute 2^32 hashes and stop. You'll need to change it a bit if you want it to mine for SHAllenge entries. It also uses the first device, you may need to change this if you have multiple devices (e.g. on a laptop with discrete and integrated GPUs).

I got around 320 Mhashes/sec on my laptop's GTX 1660 Ti. Curious to know how fast it is on other GPUs.

My SHAllenge username is [CPP_R00LZ_RUST_DR00LZ](https://shallenge.quirino.net/username?username=CPP_R00LZ_RUST_DR00LZ).

## TODO

* At the moment the library allocates a memory block per buffer. Should use something like [VMA](https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator).
* If we move the nonce to the end of the SHA-256 message, the miner could pre-compute the first 10 rounds.
