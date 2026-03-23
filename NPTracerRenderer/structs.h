#pragma once

#define GLM_ENABLE_EXPERIMENTAL
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include <glm/glm.hpp>
#include <glm/gtx/compatibility.hpp>

#include <array>
#include <optional>
#include <vector>

using FLOAT2 = glm::f32vec2;
using FLOAT3 = glm::f32vec3;
using FLOAT4X4 = glm::f32mat4;

struct Vertex
{
    FLOAT3 pos;
    FLOAT3 color;
    FLOAT2 uv;

    // tell vulkan how vertices should be moved through
    static VkVertexInputBindingDescription getBindingDescription()
    {
        return { 0, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX };
    }

    // tell vulkan what attributes exist within each vertex and how big they are
    static std::array<VkVertexInputAttributeDescription, 3> getAttributeDescriptions()
    {
        return {
            VkVertexInputAttributeDescription{0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, pos)},
            VkVertexInputAttributeDescription{1, 0, VK_FORMAT_R32G32B32_SFLOAT,
                                              offsetof(Vertex, color)},
            VkVertexInputAttributeDescription{2, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, uv)},
        };
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

struct Image
{
    VkImage image = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;
    VmaAllocationInfo allocInfo;

    void destroy(VkDevice device, VmaAllocator allocator)
    {
        if (image != VK_NULL_HANDLE)
        {
            vmaDestroyImage(allocator, image, allocation);
            image = VK_NULL_HANDLE;
        }

        if (view != VK_NULL_HANDLE)
        {
            vkDestroyImageView(device, view, nullptr);
            view = VK_NULL_HANDLE;
        }
    }
};

struct SwapchainParams
{
    VkSurfaceFormatKHR format;
    VkPresentModeKHR presentMode;
    VkExtent2D extent;
    VkFormat depthFormat;
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

struct MeshData
{
    uint32_t objectId;
    
    std::vector<uint32_t> indices;
    std::vector<FLOAT3> positions;

    std::vector<FLOAT3> normals;
    std::vector<FLOAT2> uvs;

    std::vector<FLOAT3> colors;
    std::vector<uint32_t> materialIds;
    
    FLOAT4X4 objectToWorld;
    FLOAT4X4 worldToObject;
    
    FLOAT3 bboxMin;
    FLOAT3 bboxMax;
};

enum class LightType : uint8_t
{
    Directional,
    Point,
    Area
};

struct LightData
{
    LightType type;

    FLOAT4X4 transform;

    FLOAT3 color;
    float intensity;

    // for area lights
    FLOAT3 u;
    FLOAT3 v;

    // for point / area
    float radius;

    uint32_t lightId;
};

struct CameraData
{
    // extrinsics
    FLOAT3 cameraPos;
    FLOAT3 cameraForward;
    FLOAT3 cameraUp;
    
    // intrinsics
    float fov;
    float aspect;
};

struct RenderSettings
{
    // general settings
    uint32_t maxDepth;
    uint32_t samplesPerPixel;
    
    // stylization-specific
    uint32_t stylizationId;
    uint32_t flags; // firstHitOnly, etc.
};

struct RendererPayload
{
    std::vector<MeshData> meshes;
    std::vector<LightData> lights;
    
    CameraData cam;
    
    RenderSettings settings;
};

struct VkRenderTarget
{
    VkImage image;
    VkImageView view;
    VkFormat format;
    uint32_t width;
    uint32_t height;
    VkImageLayout layout;
};

struct VkRendererAovs
{
    VkRenderTarget color;
    // depth, normals? in the future
};