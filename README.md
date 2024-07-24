# Vulkan compute playground

## Huh?

`vc` is a small library to run compute shaders with Vulkan.

This is just an educational toy. I started with the example [here](https://www.neilhenning.dev/posts/a-simple-vulkan-compute-example/) and went from there. For anything serious you might want to use [kompute](https://github.com/KomputeProject/kompute) instead.

## Building

This uses C++20 modules, so it requires a fairly new tool set. I used clang 18, cmake 3.28 and ninja 1.11.

## What's `miner.cpp`?

It's a miner for [SHAllenge](https://shallenge.quirino.net/) entries. This was the initial motivation for writing this code.

As it is, it will only compute 2^32 hashes and stop. You'll need to change it a bit if you want it to mine for SHAllenge entries. It also uses the first device, you may need to change this if you have multiple devices (e.g. on a laptop with discrete and integrated GPUs).

I got around 320 Mhashes/sec on my laptop's GTX 1660 Ti. Curious to know how fast it is on other GPUs.

## TODO

* At the moment the library allocates a memory block per buffer. Should use something like [VMA](https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator).
* Miner: if we move the nonce to the end of the SHA-256 message, the miner could pre-compute the first 10 rounds.
