module;

#include <algorithm>
#include <concepts>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <vulkan/vulkan.h>

export module vc;

#define VK_CHECK(call)                                                                                                 \
    {                                                                                                                  \
        const auto result = (call);                                                                                    \
        if (result != VK_SUCCESS)                                                                                      \
        {                                                                                                              \
            std::fprintf(stderr, "Vulkan error at %s:%d: %d\n", __FILE__, __LINE__, result);                           \
            std::exit(EXIT_FAILURE);                                                                                   \
        }                                                                                                              \
    }

export namespace vc
{
class Device;

class Instance
{
public:
    Instance();
    ~Instance();

    Instance(const Instance &) = delete;
    Instance(Instance &&rhs);

    Instance &operator=(const Instance &) = delete;
    Instance &operator=(Instance rhs);

    friend void swap(Instance &lhs, Instance &rhs);

    operator VkInstance() const { return m_instance; }

    std::vector<Device> devices() const;

private:
    VkInstance m_instance{VK_NULL_HANDLE};
};

class Device
{
public:
    Device() = default;
    Device(const Instance *instance, VkPhysicalDevice physDevice);
    ~Device();

    Device(const Device &) = delete;
    Device(Device &&rhs);

    Device &operator=(const Device &) = delete;
    Device &operator=(Device rhs);

    friend void swap(Device &lhs, Device &rhs);

    operator VkDevice() const { return m_device; }

    std::uint32_t computeQueueFamilyIndex() const { return m_queueFamilyIndex; }
    VkCommandPool commandPool() const { return m_commandPool; }
    VkCommandBuffer commandBuffer() const { return m_commandBuffer; }
    VkQueue computeQueue() const { return m_computeQueue; }

    std::uint32_t findHostVisibleMemory(VkDeviceSize size) const;

private:
    const Instance *m_instance{nullptr};
    VkPhysicalDevice m_physDevice{VK_NULL_HANDLE};
    std::uint32_t m_queueFamilyIndex{~0u};
    VkDevice m_device{VK_NULL_HANDLE};
    VkCommandPool m_commandPool{VK_NULL_HANDLE};
    VkCommandBuffer m_commandBuffer{VK_NULL_HANDLE};
    VkQueue m_computeQueue{VK_NULL_HANDLE};
};

class Buffer
{
public:
    Buffer() = default;
    Buffer(const Device *device, VkDeviceSize size);
    ~Buffer();

    Buffer(const Buffer &) = delete;
    Buffer(Buffer &&rhs);

    Buffer &operator=(const Buffer &) = delete;
    Buffer &operator=(Buffer rhs);

    friend void swap(Buffer &lhs, Buffer &rhs);

    operator VkBuffer() const { return m_buffer; }

    std::byte *map() const;
    void unmap() const;

private:
    const Device *m_device{nullptr};
    VkDeviceSize m_size{0};
    VkDeviceMemory m_deviceMemory{VK_NULL_HANDLE};
    VkBuffer m_buffer{VK_NULL_HANDLE};
};

class Program
{
public:
    Program() = default;
    Program(const Device *device, const std::string &path);
    ~Program();

    Program(const Program &) = delete;
    Program(Program &&rhs);

    Program &operator=(const Program &) = delete;
    Program &operator=(Program rhs);

    friend void swap(Program &lhs, Program &rhs);

    template<std::convertible_to<VkBuffer>... Buffers>
    void bind(const Buffers &...buffers)
    {
        releasePipeline();
        initPipeline(buffers...);
    }

    void dispatch(uint32_t groupCountX = 1, uint32_t groupCountY = 1, uint32_t groupCountZ = 1) const;

private:
    template<std::convertible_to<VkBuffer>... Buffers>
    void initPipeline(const Buffers &...buffers)
    {
        const auto descriptorSetLayoutBindings = std::invoke(
            []<std::size_t... Idx>(std::index_sequence<Idx...>) {
                return std::array{VkDescriptorSetLayoutBinding{.binding = Idx,
                                                               .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                                               .descriptorCount = 1,
                                                               .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
                                                               .pImmutableSamplers = nullptr}...};
            },
            std::index_sequence_for<Buffers...>{});

        const VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .bindingCount = static_cast<uint32_t>(descriptorSetLayoutBindings.size()),
            .pBindings = descriptorSetLayoutBindings.data()};
        VK_CHECK(
            vkCreateDescriptorSetLayout(*m_device, &descriptorSetLayoutCreateInfo, nullptr, &m_descriptorSetLayout));

        const VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {.sType =
                                                                         VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                                                                     .pNext = nullptr,
                                                                     .flags = 0,
                                                                     .setLayoutCount = 1,
                                                                     .pSetLayouts = &m_descriptorSetLayout,
                                                                     .pushConstantRangeCount = 0,
                                                                     .pPushConstantRanges = nullptr};
        VK_CHECK(vkCreatePipelineLayout(*m_device, &pipelineLayoutCreateInfo, nullptr, &m_pipelineLayout));

        const VkComputePipelineCreateInfo computePipelineCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .stage = VkPipelineShaderStageCreateInfo{.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                                                     .pNext = nullptr,
                                                     .flags = 0,
                                                     .stage = VK_SHADER_STAGE_COMPUTE_BIT,
                                                     .module = m_shaderModule,
                                                     .pName = "main",
                                                     .pSpecializationInfo = nullptr},
            .layout = m_pipelineLayout,
            .basePipelineHandle = VK_NULL_HANDLE,
            .basePipelineIndex = 0};
        VK_CHECK(
            vkCreateComputePipelines(*m_device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &m_pipeline));

        const VkDescriptorPoolSize descriptorPoolSize = {
            .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = sizeof...(Buffers),
        };
        const VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = {.sType =
                                                                         VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                                                                     .pNext = nullptr,
                                                                     .flags = 0,
                                                                     .maxSets = 1,
                                                                     .poolSizeCount = 1,
                                                                     .pPoolSizes = &descriptorPoolSize};
        VK_CHECK(vkCreateDescriptorPool(*m_device, &descriptorPoolCreateInfo, nullptr, &m_descriptorPool));

        const VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .pNext = nullptr,
            .descriptorPool = m_descriptorPool,
            .descriptorSetCount = 1,
            .pSetLayouts = &m_descriptorSetLayout};
        VK_CHECK(vkAllocateDescriptorSets(*m_device, &descriptorSetAllocateInfo, &m_descriptorSet));

        const auto bufferInfos = std::array{
            VkDescriptorBufferInfo{.buffer = static_cast<VkBuffer>(buffers), .offset = 0, .range = VK_WHOLE_SIZE}...};
        const auto writeDescriptorSets = std::invoke(
            [this, &bufferInfos]<std::size_t... Idx>(std::index_sequence<Idx...>) {
                return std::array{VkWriteDescriptorSet{.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                                                       .pNext = nullptr,
                                                       .dstSet = m_descriptorSet,
                                                       .dstBinding = Idx,
                                                       .dstArrayElement = 0,
                                                       .descriptorCount = 1,
                                                       .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                                       .pImageInfo = nullptr,
                                                       .pBufferInfo = &bufferInfos[Idx],
                                                       .pTexelBufferView = nullptr}...};
            },
            std::index_sequence_for<Buffers...>{});
        vkUpdateDescriptorSets(*m_device, writeDescriptorSets.size(), writeDescriptorSets.data(), 0, nullptr);
    }

    void releasePipeline();

    const Device *m_device{nullptr};
    VkShaderModule m_shaderModule{VK_NULL_HANDLE};
    VkDescriptorSetLayout m_descriptorSetLayout{VK_NULL_HANDLE};
    VkPipelineLayout m_pipelineLayout{VK_NULL_HANDLE};
    VkPipeline m_pipeline{VK_NULL_HANDLE};
    VkDescriptorPool m_descriptorPool{VK_NULL_HANDLE};
    VkDescriptorSet m_descriptorSet{VK_NULL_HANDLE};
};

} // namespace vc

namespace vc
{

namespace
{

uint32_t findComputeQueueFamily(VkPhysicalDevice physDevice)
{
    uint32_t queueFamilyPropertiesCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physDevice, &queueFamilyPropertiesCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilyProperties(queueFamilyPropertiesCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physDevice, &queueFamilyPropertiesCount, queueFamilyProperties.data());

    auto it = std::find_if(queueFamilyProperties.begin(), queueFamilyProperties.end(),
                           [](const VkQueueFamilyProperties &properties) -> bool {
                               const VkQueueFlags flags = properties.queueFlags;
                               return flags & VK_QUEUE_COMPUTE_BIT;
                           });
    if (it == queueFamilyProperties.end())
        return ~0u;

    return std::distance(queueFamilyProperties.begin(), it);
}

std::optional<std::vector<std::byte>> readFile(const std::string &path)
{
    FILE *stream = fopen(path.c_str(), "rb");
    if (!stream)
        return std::nullopt;

    fseek(stream, 0l, SEEK_END);
    const auto size = ftell(stream);
    fseek(stream, 0, SEEK_SET);
    std::vector<std::byte> data(size);
    fread(data.data(), 1, data.size(), stream);
    fclose(stream);

    return data;
}

} // namespace

Program::Program(const Device *device, const std::string &path)
    : m_device(device)
{
    auto shaderCode = readFile(path);
    if (shaderCode.has_value())
    {
        const VkShaderModuleCreateInfo shaderModuleCreateInfo = {.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                                                                 .pNext = nullptr,
                                                                 .flags = 0,
                                                                 .codeSize = shaderCode->size(),
                                                                 .pCode =
                                                                     reinterpret_cast<uint32_t *>(shaderCode->data())};

        VK_CHECK(vkCreateShaderModule(*m_device, &shaderModuleCreateInfo, 0, &m_shaderModule));
    }
}

Program::~Program()
{
    if (m_shaderModule)
        vkDestroyShaderModule(*m_device, m_shaderModule, nullptr);

    releasePipeline();
}

void Program::releasePipeline()
{
    if (m_descriptorPool)
        vkDestroyDescriptorPool(*m_device, m_descriptorPool, nullptr);

    if (m_pipeline)
        vkDestroyPipeline(*m_device, m_pipeline, nullptr);

    if (m_pipelineLayout)
        vkDestroyPipelineLayout(*m_device, m_pipelineLayout, nullptr);

    if (m_descriptorSetLayout)
        vkDestroyDescriptorSetLayout(*m_device, m_descriptorSetLayout, nullptr);
}

void Program::dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) const
{
    const auto commandBuffer = m_device->commandBuffer();
    const auto queue = m_device->computeQueue();

    const VkCommandBufferBeginInfo commandBufferBeginInfo = {.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                                                             .pNext = nullptr,
                                                             .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
                                                             .pInheritanceInfo = nullptr};
    VK_CHECK(vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo));
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipeline);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipelineLayout, 0, 1, &m_descriptorSet, 0,
                            nullptr);
    vkCmdDispatch(commandBuffer, groupCountX, groupCountY, groupCountZ);
    VK_CHECK(vkEndCommandBuffer(commandBuffer));

    const VkSubmitInfo submitInfo = {.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                                     .pNext = nullptr,
                                     .waitSemaphoreCount = 0,
                                     .pWaitSemaphores = nullptr,
                                     .pWaitDstStageMask = nullptr,
                                     .commandBufferCount = 1,
                                     .pCommandBuffers = &commandBuffer,
                                     .signalSemaphoreCount = 0,
                                     .pSignalSemaphores = nullptr};
    VK_CHECK(vkQueueSubmit(queue, 1, &submitInfo, 0));

    VK_CHECK(vkQueueWaitIdle(queue));
}

Buffer::Buffer(const Device *device, VkDeviceSize size)
    : m_device(device)
    , m_size(size)
{
    const auto memoryTypeIndex = device->findHostVisibleMemory(size);
    if (memoryTypeIndex != ~0u)
    {
        const VkMemoryAllocateInfo memoryAllocateInfo = {.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                                                         .pNext = nullptr,
                                                         .allocationSize = m_size,
                                                         .memoryTypeIndex = memoryTypeIndex};
        VK_CHECK(vkAllocateMemory(*m_device, &memoryAllocateInfo, nullptr, &m_deviceMemory));

        uint32_t computeQueueFamilyIndex = device->computeQueueFamilyIndex();
        const VkBufferCreateInfo bufferCreateInfo = {.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                                                     .pNext = nullptr,
                                                     .flags = 0,
                                                     .size = m_size,
                                                     .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                                     .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
                                                     .queueFamilyIndexCount = 1,
                                                     .pQueueFamilyIndices = &computeQueueFamilyIndex};
        VK_CHECK(vkCreateBuffer(*m_device, &bufferCreateInfo, nullptr, &m_buffer));

        VK_CHECK(vkBindBufferMemory(*m_device, m_buffer, m_deviceMemory, 0));
    }
}

Buffer::~Buffer()
{
    if (m_buffer)
        vkDestroyBuffer(*m_device, m_buffer, nullptr);

    if (m_deviceMemory)
        vkFreeMemory(*m_device, m_deviceMemory, nullptr);
}

Buffer::Buffer(Buffer &&rhs)
    : m_device(std::exchange(rhs.m_device, nullptr))
    , m_size(std::exchange(rhs.m_size, 0))
    , m_deviceMemory(std::exchange(rhs.m_deviceMemory, VK_NULL_HANDLE))
    , m_buffer(std::exchange(rhs.m_buffer, VK_NULL_HANDLE))
{
}

Buffer &Buffer::operator=(Buffer rhs)
{
    swap(*this, rhs);
    return *this;
}

void swap(Buffer &lhs, Buffer &rhs)
{
    using std::swap;
    swap(lhs.m_device, rhs.m_device);
    swap(lhs.m_size, rhs.m_size);
    swap(lhs.m_deviceMemory, rhs.m_deviceMemory);
    swap(lhs.m_buffer, rhs.m_buffer);
}

std::byte *Buffer::map() const
{
    std::byte *data;
    VK_CHECK(vkMapMemory(*m_device, m_deviceMemory, 0, m_size, 0, reinterpret_cast<void **>(&data)));
    return data;
}

void Buffer::unmap() const
{
    vkUnmapMemory(*m_device, m_deviceMemory);
}

Device::Device(const Instance *instance, VkPhysicalDevice physDevice)
    : m_instance(instance)
    , m_physDevice(physDevice)
    , m_queueFamilyIndex(findComputeQueueFamily(physDevice))
{
    if (m_queueFamilyIndex != ~0u)
    {
        const float queuePriority = 1.0F;
        const VkDeviceQueueCreateInfo deviceQueueCreateInfo = {.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                                                               .pNext = nullptr,
                                                               .flags = 0,
                                                               .queueFamilyIndex = m_queueFamilyIndex,
                                                               .queueCount = 1,
                                                               .pQueuePriorities = &queuePriority};

        const VkDeviceCreateInfo deviceCreateInfo = {.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
                                                     .pNext = nullptr,
                                                     .flags = 0,
                                                     .queueCreateInfoCount = 1,
                                                     .pQueueCreateInfos = &deviceQueueCreateInfo,
                                                     .enabledLayerCount = 0,
                                                     .ppEnabledLayerNames = nullptr,
                                                     .enabledExtensionCount = 0,
                                                     .ppEnabledExtensionNames = nullptr,
                                                     .pEnabledFeatures = nullptr};

        VK_CHECK(vkCreateDevice(m_physDevice, &deviceCreateInfo, nullptr, &m_device));

        const VkCommandPoolCreateInfo commandPoolCreateInfo = {.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                                                               .pNext = nullptr,
                                                               .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                                                               .queueFamilyIndex = m_queueFamilyIndex};

        VK_CHECK(vkCreateCommandPool(m_device, &commandPoolCreateInfo, nullptr, &m_commandPool));

        const VkCommandBufferAllocateInfo commandBufferAllocateInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .pNext = nullptr,
            .commandPool = m_commandPool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1,
        };

        VK_CHECK(vkAllocateCommandBuffers(m_device, &commandBufferAllocateInfo, &m_commandBuffer));

        vkGetDeviceQueue(m_device, m_queueFamilyIndex, 0, &m_computeQueue);
    }
}

Device::~Device()
{
    if (m_commandBuffer)
        vkFreeCommandBuffers(m_device, m_commandPool, 1, &m_commandBuffer);

    if (m_commandPool)
        vkDestroyCommandPool(m_device, m_commandPool, nullptr);

    if (m_device)
        vkDestroyDevice(m_device, nullptr);
}

Device::Device(Device &&rhs)
    : m_instance(std::exchange(rhs.m_instance, nullptr))
    , m_physDevice(std::exchange(rhs.m_physDevice, VK_NULL_HANDLE))
    , m_queueFamilyIndex(std::exchange(rhs.m_queueFamilyIndex, ~0u))
    , m_device(std::exchange(rhs.m_device, VK_NULL_HANDLE))
    , m_commandPool(std::exchange(rhs.m_commandPool, VK_NULL_HANDLE))
    , m_commandBuffer(std::exchange(rhs.m_commandBuffer, VK_NULL_HANDLE))
    , m_computeQueue(std::exchange(rhs.m_computeQueue, VK_NULL_HANDLE))
{
}

Device &Device::operator=(Device rhs)
{
    swap(*this, rhs);
    return *this;
}

std::uint32_t Device::findHostVisibleMemory(VkDeviceSize size) const
{
    VkPhysicalDeviceMemoryProperties memoryProperties;
    vkGetPhysicalDeviceMemoryProperties(m_physDevice, &memoryProperties);

    for (std::uint32_t i = 0; i < memoryProperties.memoryTypeCount; ++i)
    {
        const VkMemoryType &memoryType = memoryProperties.memoryTypes[i];
        const auto flags = memoryType.propertyFlags;
        if ((flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) && (flags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
        {
            const auto &heap = memoryProperties.memoryHeaps[memoryType.heapIndex];
            if (size <= heap.size)
                return i;
        }
    }

    return ~0u;
}

void swap(Device &lhs, Device &rhs)
{
    using std::swap;
    swap(lhs.m_instance, rhs.m_instance);
    swap(lhs.m_physDevice, rhs.m_physDevice);
    swap(lhs.m_queueFamilyIndex, rhs.m_queueFamilyIndex);
    swap(lhs.m_device, rhs.m_device);
    swap(lhs.m_commandPool, rhs.m_commandPool);
    swap(lhs.m_commandBuffer, rhs.m_commandBuffer);
    swap(lhs.m_computeQueue, rhs.m_computeQueue);
}

Instance::Instance()
{
    const VkApplicationInfo applicationInfo = {.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
                                               .pNext = nullptr,
                                               .pApplicationName = "test",
                                               .applicationVersion = 0,
                                               .pEngineName = "test",
                                               .engineVersion = 0,
                                               .apiVersion = VK_API_VERSION_1_3};

    const auto layers = std::array{"VK_LAYER_KHRONOS_validation"};
    const VkInstanceCreateInfo instanceCreateInfo = {.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
                                                     .pNext = nullptr,
                                                     .flags = 0,
                                                     .pApplicationInfo = &applicationInfo,
                                                     .enabledLayerCount = layers.size(),
                                                     .ppEnabledLayerNames = layers.data(),
                                                     .enabledExtensionCount = 0,
                                                     .ppEnabledExtensionNames = nullptr};

    VK_CHECK(vkCreateInstance(&instanceCreateInfo, nullptr, &m_instance));
}

Instance::~Instance()
{
    if (m_instance)
        vkDestroyInstance(m_instance, nullptr);
}

Instance::Instance(Instance &&rhs)
    : m_instance(std::exchange(rhs.m_instance, VK_NULL_HANDLE))
{
}

Instance &Instance::operator=(Instance rhs)
{
    swap(*this, rhs);
    return *this;
}

void swap(Instance &lhs, Instance &rhs)
{
    std::swap(lhs.m_instance, rhs.m_instance);
}

std::vector<Device> Instance::devices() const
{
    uint32_t physDeviceCount = 0;
    VK_CHECK(vkEnumeratePhysicalDevices(m_instance, &physDeviceCount, nullptr));

    std::vector<VkPhysicalDevice> physDevices(physDeviceCount);
    VK_CHECK(vkEnumeratePhysicalDevices(m_instance, &physDeviceCount, physDevices.data()));

    std::vector<Device> devices;
    devices.reserve(physDevices.size());
    for (auto physDevice : physDevices)
    {
        devices.emplace_back(this, physDevice);
    }
    return devices;
}

} // namespace vc
