#pragma once

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <vulkan/vulkan.h>

#define VK_CHECK(call)                                                                                                 \
    {                                                                                                                  \
        auto result = (call);                                                                                          \
        if (result != VK_SUCCESS)                                                                                      \
        {                                                                                                              \
            std::fprintf(stderr, "Vulkan error at %s:%d: %d\n", __FILE__, __LINE__, result);                           \
            std::exit(EXIT_FAILURE);                                                                                   \
        }                                                                                                              \
    }

namespace vc
{

namespace
{

template<std::size_t... Idx>
std::array<VkDescriptorSetLayoutBinding, sizeof...(Idx)> makeDescriptorSetLayoutBindings(std::index_sequence<Idx...>)
{
    return std::array{VkDescriptorSetLayoutBinding{.binding = Idx,
                                                   .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                                   .descriptorCount = 1,
                                                   .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
                                                   .pImmutableSamplers = nullptr}...};
}

}; // namespace

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

    std::uint32_t findHostVisibleMemory(VkDeviceSize size) const;

private:
    const Instance *m_instance{nullptr};
    VkPhysicalDevice m_physDevice{VK_NULL_HANDLE};
    std::uint32_t m_queueFamilyIndex{~0u};
    VkDevice m_device{VK_NULL_HANDLE};
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

    std::span<std::byte> map() const;
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

    template<typename... Buffers>
    void bindBuffers(const Buffers &...buffers)
    {
        releasePipeline();
        initPipeline(buffers...);
    }

    void run() const;

private:
    template<typename... Buffers>
    void initPipeline(const Buffers &...buffers)
    {
        const auto descriptorSetLayoutBindings = makeDescriptorSetLayoutBindings(std::index_sequence_for<Buffers...>{});

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
        std::array<VkWriteDescriptorSet, sizeof...(buffers)> writeDescriptorSets;
        for (std::size_t i = 0; i < sizeof...(buffers); ++i)
        {
            writeDescriptorSets[i] = VkWriteDescriptorSet{.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                                                          .pNext = nullptr,
                                                          .dstSet = m_descriptorSet,
                                                          .dstBinding = static_cast<uint32_t>(i),
                                                          .dstArrayElement = 0,
                                                          .descriptorCount = 1,
                                                          .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                                          .pImageInfo = nullptr,
                                                          .pBufferInfo = &bufferInfos[i],
                                                          .pTexelBufferView = nullptr};
        }
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
