#include <cstdio>
#include <cstdlib>

#include <vector>
#include <optional>
#include <algorithm>
#include <string>
#include <cassert>
#include <array>
#include <numeric>

#include <vulkan/vulkan.h>

#define VK_CHECK(call)  \
      { \
          auto result = (call);                                                \
          if (result != VK_SUCCESS)                                                \
          {                                                                        \
              std::fprintf(stderr, "Vulkan error at %s:%d: %d\n", __FILE__, __LINE__, result); \
              std::exit(EXIT_FAILURE);                                             \
          }                                                                        \
      }

std::vector<std::byte> readFile(const std::string& path)
{
	FILE *stream = fopen(path.c_str(), "rb");
    assert(stream != nullptr);
	fseek(stream, 0l, SEEK_END);
	const auto size = ftell(stream);
	fseek(stream, 0, SEEK_SET);
	std::vector<std::byte> data(size);
	fread(data.data(), 1, data.size(), stream);
	fclose(stream);
	return data;
}

std::optional<uint32_t> getBestComputeQueue(VkPhysicalDevice physicalDevice)
{
    uint32_t queueFamilyPropertiesCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyPropertiesCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilyProperties(queueFamilyPropertiesCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyPropertiesCount, queueFamilyProperties.data());

    // Look for a queue with just the compute bit set
    auto it = std::find_if(queueFamilyProperties.begin(), queueFamilyProperties.end(), [](const VkQueueFamilyProperties& properties) {
        const VkQueueFlags flags = properties.queueFlags;
        return !(flags & VK_QUEUE_GRAPHICS_BIT) && (flags & VK_QUEUE_COMPUTE_BIT);
    });
    if (it != queueFamilyProperties.end()) {
        return std::distance(queueFamilyProperties.begin(), it);
    }

    // Look for any queue with the compute bit
    it = std::find_if(queueFamilyProperties.begin(), queueFamilyProperties.end(), [](const VkQueueFamilyProperties& properties) {
        const VkQueueFlags flags = properties.queueFlags;
        return flags & VK_QUEUE_COMPUTE_BIT;
    });
    if (it != queueFamilyProperties.end()) {
        return std::distance(queueFamilyProperties.begin(), it);
    }

    return std::nullopt;
}

int main(int argc, char *argv[])
{
    const VkApplicationInfo applicationInfo = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pNext = nullptr,
        .pApplicationName = "test",
        .applicationVersion = 0,
        .pEngineName = "test",
        .engineVersion = 0,
        .apiVersion = VK_MAKE_VERSION(1, 0, 9)
    };

    const VkInstanceCreateInfo instanceCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .pApplicationInfo = &applicationInfo,
        .enabledLayerCount = 0,
        .ppEnabledLayerNames = nullptr,
        .enabledExtensionCount = 0,
        .ppEnabledExtensionNames = nullptr
    };

    VkInstance instance;
    VK_CHECK(vkCreateInstance(&instanceCreateInfo, nullptr, &instance));

    std::printf("instance=%p\n", instance);

    uint32_t physicalDeviceCount = 0;
    VK_CHECK(vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, nullptr));

    std::printf("physicalDeviceCount=%u\n", physicalDeviceCount);

    std::vector<VkPhysicalDevice> physicalDevices(physicalDeviceCount);
    VK_CHECK(vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, physicalDevices.data()));

    for (auto physicalDevice : physicalDevices) {
        VkPhysicalDeviceProperties properties;
        vkGetPhysicalDeviceProperties(physicalDevice, &properties);
        std::printf("device %p: %s\n", physicalDevice, properties.deviceName);
    }

    for (auto physicalDevice : physicalDevices) {
        auto queueFamilyIndex = getBestComputeQueue(physicalDevice);
        if (queueFamilyIndex) {
            std::printf("*** physicalDevice %p: queueFamilyIndex %u\n", physicalDevice, *queueFamilyIndex);
        }

        const float queuePriority = 1.0F;
        const VkDeviceQueueCreateInfo deviceQueueCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .queueFamilyIndex = *queueFamilyIndex,
            .queueCount = 1,
            .pQueuePriorities = nullptr
        };

        const VkDeviceCreateInfo deviceCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .queueCreateInfoCount = 1,
            .pQueueCreateInfos = &deviceQueueCreateInfo,
            .enabledLayerCount = 0,
            .ppEnabledLayerNames = 0,
            .enabledExtensionCount = 0,
            .ppEnabledExtensionNames = nullptr,
            .pEnabledFeatures = nullptr
        };

        VkDevice device;
        VK_CHECK(vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &device));

        std::printf("**** physicalDevice %p: device %p\n", physicalDevice, device);

        constexpr VkDeviceSize ArraySize = 32;
        constexpr VkDeviceSize BufferSize = ArraySize * sizeof(float);
        constexpr VkDeviceSize MemorySize = BufferSize * 2;

        // Look for a memory type with:
        //   * HOST_VISIBLE_BIT / HOST_COHERENT_BIT flags
        //   * heap large enough for bufferSize

        const auto memoryTypeIndex = [physicalDevice]() -> std::optional<uint32_t> {
            VkPhysicalDeviceMemoryProperties memoryProperties;
            vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);

            std::printf("**** memory type count: %d heap count: %d\n", memoryProperties.memoryTypeCount, memoryProperties.memoryHeapCount);

            for (std::size_t i = 0; i < memoryProperties.memoryTypeCount; ++i) {
                const VkMemoryType& memoryType = memoryProperties.memoryTypes[i];
                std::printf("******* memory %u flags %u/%u heapSize %lu\n", i, !!(memoryType.propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT), !!(memoryType.propertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT), memoryProperties.memoryHeaps[memoryType.heapIndex].size);

                const auto flags = memoryType.propertyFlags;
                if ((flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) && (flags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
                    const auto& heap = memoryProperties.memoryHeaps[memoryType.heapIndex];
                    if (MemorySize < heap.size)
                    {
                        return i;
                    }
                }
            }

            return std::nullopt;
        }();
        if (memoryTypeIndex) {
            std::printf("**** memory type index=%u\n", *memoryTypeIndex);

            const VkMemoryAllocateInfo memoryAllocateInfo = {
                .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                .pNext = nullptr,
                .allocationSize = MemorySize,
                .memoryTypeIndex = *memoryTypeIndex
            };

            VkDeviceMemory memory;
            VK_CHECK(vkAllocateMemory(device, &memoryAllocateInfo, nullptr, &memory));
            std::printf("*** allocated!!! memory=%p\n", memory);

            float *data;
            VK_CHECK(vkMapMemory(device, memory, 0, BufferSize, 0, reinterpret_cast<void **>(&data)));
            std::iota(data, data + ArraySize, 1);
            vkUnmapMemory(device, memory);

            const VkBufferCreateInfo bufferCreateInfo = {
                .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0,
                .size = BufferSize,
                .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
                .queueFamilyIndexCount = 1,
                .pQueueFamilyIndices = &queueFamilyIndex.value()
            };

            VkBuffer inBuffer;
            VK_CHECK(vkCreateBuffer(device, &bufferCreateInfo, nullptr, &inBuffer));
            VK_CHECK(vkBindBufferMemory(device, inBuffer, memory, 0));

            std::printf("* inBuffer=%p\n", inBuffer);

            VkBuffer outBuffer;
            VK_CHECK(vkCreateBuffer(device, &bufferCreateInfo, nullptr, &outBuffer));
            VK_CHECK(vkBindBufferMemory(device, outBuffer, memory, BufferSize));

            std::printf("* outBuffer=%p\n", outBuffer);

            auto shaderCode = readFile("simple.comp.spv");
            std::printf("shader data %lu\n", shaderCode.size());

            VkShaderModuleCreateInfo shaderModuleCreateInfo = {
                .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0,
                .codeSize = shaderCode.size(),
                .pCode = reinterpret_cast<uint32_t *>(shaderCode.data())
            };

            VkShaderModule shaderModule;
            VK_CHECK(vkCreateShaderModule(device, &shaderModuleCreateInfo, 0, &shaderModule));
            std::printf("* shaderModule=%p\n", shaderModule);

            const std::array<VkDescriptorSetLayoutBinding, 2> descriptorSetLayoutBindings = {
                VkDescriptorSetLayoutBinding {
                    .binding = 0,
                    .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                    .descriptorCount = 1,
                    .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
                    .pImmutableSamplers = nullptr
                },
                VkDescriptorSetLayoutBinding {
                    .binding = 1,
                    .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                    .descriptorCount = 1,
                    .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
                    .pImmutableSamplers = nullptr
                },
            };

            VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0,
                .bindingCount = descriptorSetLayoutBindings.size(),
                .pBindings = descriptorSetLayoutBindings.data()
            };

            VkDescriptorSetLayout descriptorSetLayout;
            VK_CHECK(vkCreateDescriptorSetLayout(device, &descriptorSetLayoutCreateInfo, nullptr, &descriptorSetLayout));
            std::fprintf(stderr, "* descriptorSetLayout=%p\n", descriptorSetLayout);

            VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0,
                .setLayoutCount = 1,
                .pSetLayouts = &descriptorSetLayout,
                .pushConstantRangeCount = 0,
                .pPushConstantRanges = nullptr
            };

            VkPipelineLayout pipelineLayout;
            VK_CHECK(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout));
            std::fprintf(stderr, "* pipelineLayout=%p\n", pipelineLayout);

            VkComputePipelineCreateInfo computePipelineCreateInfo = {
                .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0,
                .stage = VkPipelineShaderStageCreateInfo {
                    .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                    .pNext = nullptr,
                    .flags = 0,
                    .stage = VK_SHADER_STAGE_COMPUTE_BIT,
                    .module = shaderModule,
                    .pName = "main",
                    .pSpecializationInfo = nullptr
                },
                .layout = pipelineLayout,
                .basePipelineHandle = VK_NULL_HANDLE,
                .basePipelineIndex = 0
            };

            VkPipeline pipeline;
            VK_CHECK(vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &pipeline));
            std::fprintf(stderr, "* pipeline=%p\n", pipeline);

            VkDescriptorPoolSize descriptorPoolSize = {
                .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .descriptorCount = 2,
            };

            VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0,
                .maxSets = 1,
                .poolSizeCount = 1,
                .pPoolSizes = &descriptorPoolSize
            };

            VkDescriptorPool descriptorPool;
            VK_CHECK(vkCreateDescriptorPool(device, &descriptorPoolCreateInfo, nullptr, &descriptorPool));
            std::fprintf(stderr, "* descriptorPool=%p\n", descriptorPool);

            VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                .pNext = nullptr,
                .descriptorPool = descriptorPool,
                .descriptorSetCount = 1,
                .pSetLayouts = &descriptorSetLayout
            };

            VkDescriptorSet descriptorSet;
            VK_CHECK(vkAllocateDescriptorSets(device, &descriptorSetAllocateInfo, &descriptorSet));
            std::fprintf(stderr, "* descriptorSet=%p\n", descriptorSet);

            VkDescriptorBufferInfo inDescriptorBufferInfo = {
                .buffer = inBuffer,
                .offset = 0,
                .range = VK_WHOLE_SIZE
            };

            VkDescriptorBufferInfo outDescriptorBufferInfo = {
                .buffer = outBuffer,
                .offset = 0,
                .range = VK_WHOLE_SIZE
            };

            std::array<VkWriteDescriptorSet, 2> writeDescriptorSet = {
                VkWriteDescriptorSet {
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .pNext = nullptr,
                    .dstSet = descriptorSet,
                    .dstBinding = 0,
                    .dstArrayElement = 0,
                    .descriptorCount = 1,
                    .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                    .pImageInfo = nullptr,
                    .pBufferInfo = &inDescriptorBufferInfo,
                    .pTexelBufferView = nullptr
                },
                VkWriteDescriptorSet {
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .pNext = nullptr,
                    .dstSet = descriptorSet,
                    .dstBinding = 1,
                    .dstArrayElement = 0,
                    .descriptorCount = 1,
                    .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                    .pImageInfo = nullptr,
                    .pBufferInfo = &outDescriptorBufferInfo,
                    .pTexelBufferView = nullptr
                }
            };

            vkUpdateDescriptorSets(device, writeDescriptorSet.size(), writeDescriptorSet.data(), 0, nullptr);

            VkCommandPoolCreateInfo commandPoolCreateInfo = {
                .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0,
                .queueFamilyIndex = *queueFamilyIndex
            };

            VkCommandPool commandPool;
            VK_CHECK(vkCreateCommandPool(device, &commandPoolCreateInfo, nullptr, &commandPool));
            std::fprintf(stderr, "* commandPool=%p\n", commandPool);

            VkCommandBufferAllocateInfo commandBufferAllocateInfo = {
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                .pNext = nullptr,
                .commandPool = commandPool,
                .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                .commandBufferCount = 1,
            };

            VkCommandBuffer commandBuffer;
            VK_CHECK(vkAllocateCommandBuffers(device, &commandBufferAllocateInfo, &commandBuffer));
            std::fprintf(stderr, "* commandBuffer=%p\n", commandBuffer);

            VkCommandBufferBeginInfo commandBufferBeginInfo = {
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                .pNext = nullptr,
                .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
                .pInheritanceInfo = nullptr
            };

            VK_CHECK(vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo));
            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
            vkCmdDispatch(commandBuffer, ArraySize, 1, 1);
            VK_CHECK(vkEndCommandBuffer(commandBuffer));

            VkQueue queue;
            vkGetDeviceQueue(device, *queueFamilyIndex, 0, &queue);

            VkSubmitInfo submitInfo = {
                .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                .pNext = nullptr,
                .waitSemaphoreCount = 0,
                .pWaitSemaphores = nullptr,
                .pWaitDstStageMask = nullptr,
                .commandBufferCount = 1,
                .pCommandBuffers = &commandBuffer,
                .signalSemaphoreCount = 0,
                .pSignalSemaphores = nullptr
            };

            VK_CHECK(vkQueueSubmit(queue, 1, &submitInfo, 0));

            VK_CHECK(vkQueueWaitIdle(queue));

            VK_CHECK(vkMapMemory(device, memory, 0, MemorySize, 0, reinterpret_cast<void **>(&data)));
            for (std::size_t i = 0; i < ArraySize; ++i) {
                std::printf("--> %u: %f %f\n", i, data[i], data[i + ArraySize]);
            }
            vkUnmapMemory(device, memory);

            vkDestroyBuffer(device, inBuffer, nullptr);
            vkDestroyBuffer(device, outBuffer, nullptr);
            vkFreeMemory(device, memory, nullptr);
        }

        vkDestroyDevice(device, nullptr);
    }

    vkDestroyInstance(instance, nullptr);
}
