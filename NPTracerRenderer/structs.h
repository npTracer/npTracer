#pragma once

#define GLM_ENABLE_EXPERIMENTAL
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include <glm/glm.hpp>
#include <array>
#include <optional>
#include <vector>

#include <glm/gtx/compatibility.hpp>

using FLOAT2 = glm::f32vec2;
using FLOAT3 = glm::f32vec3;
using FLOAT4X4 = glm::f32mat4;

struct Vertex
{
    glm::vec2 pos;
    glm::vec3 color;

    // tell vulkan how vertices should be moved through
    static VkVertexInputBindingDescription getBindingDescription()
    {
        return { 0, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX };
    }

    // tell vulkan what attributes exist within each vertex and how big they are
    static std::array<VkVertexInputAttributeDescription, 2> getAttributeDescriptions()
    {
        return { VkVertexInputAttributeDescription(0, 0, VK_FORMAT_R32G32_SFLOAT,
                                                   offsetof(Vertex, pos)),
                 VkVertexInputAttributeDescription(1, 0, VK_FORMAT_R32G32B32_SFLOAT,
                                                   offsetof(Vertex, color)) };
    }
};

struct Buffer
{
    VkBuffer buffer = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;
    VmaAllocationInfo allocInfo;

    void destroy(VmaAllocator allocator)
    {
        if (buffer != VK_NULL_HANDLE)
        {
            vmaDestroyBuffer(allocator, buffer, allocation);
        }
    }
};

struct SwapchainParams
{
    VkSurfaceFormatKHR format;
    VkPresentModeKHR presentMode;
    VkExtent2D extent;
};

struct Frame
{
    VkSemaphore donePresentingSemaphore;
    VkFence doneExecutingFence;
    VkCommandBuffer commandBuffer;

    Buffer uboBuffer;
    VkDescriptorSet descriptorSet;

    void destroy(VkDevice device, VmaAllocator allocator)
    {
        vkDestroyFence(device, doneExecutingFence, nullptr);
        vkDestroySemaphore(device, donePresentingSemaphore, nullptr);
        uboBuffer.destroy(allocator);
    }
};

enum class QueueFamily
{
    GRAPHICS,
    TRANSFER,
    COMPUTE
};

struct Queue
{
    VkQueue queue = VK_NULL_HANDLE;
    std::optional<uint32_t> index;
    VkCommandPool commandPool;

    explicit operator bool() const
    {
        return index.has_value();
    }

    void destroy(VkDevice device)
    {
        if (commandPool != VK_NULL_HANDLE)
        {
            vkDestroyCommandPool(device, commandPool, nullptr);
        }
    }
};

// shared structs
struct UniformBufferObject
{
    alignas(16) FLOAT4X4 model;
    alignas(16) FLOAT4X4 view;
    alignas(16) FLOAT4X4 proj;
};