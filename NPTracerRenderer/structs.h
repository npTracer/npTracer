#pragma once

#define GLM_ENABLE_EXPERIMENTAL
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <string>
#include <array>
#include <optional>
#include <vector>

using FLOAT2 = glm::f32vec2;
using FLOAT3 = glm::f32vec3;
using FLOAT4 = glm::f32vec4;
using FLOAT4X4 = glm::f32mat4;

using NPScenePath = std::string;
using NPScenePathCollection = std::vector<NPScenePath>;

struct NPVertex
{
    FLOAT4 pos;
    FLOAT4 normal;
    FLOAT4 color;
    FLOAT2 uv;
    FLOAT2 pad0;

    // tell vulkan how vertices should be moved through
    static VkVertexInputBindingDescription getBindingDescription()
    {
        return { 0, sizeof(NPVertex), VK_VERTEX_INPUT_RATE_VERTEX };
    }

    // tell vulkan what attributes exist within each vertex and how big they are
    static std::array<VkVertexInputAttributeDescription, 4> getAttributeDescriptions()
    {
        return {
            VkVertexInputAttributeDescription{ 0, 0, VK_FORMAT_R32G32B32_SFLOAT,
                                               offsetof(NPVertex, pos) },
            VkVertexInputAttributeDescription{ 1, 0, VK_FORMAT_R32G32B32_SFLOAT,
                                   offsetof(NPVertex, normal) },
            VkVertexInputAttributeDescription{ 2, 0, VK_FORMAT_R32G32B32_SFLOAT,
                                               offsetof(NPVertex, color) },
            VkVertexInputAttributeDescription{ 3, 0, VK_FORMAT_R32G32_SFLOAT,
                                               offsetof(NPVertex, uv) },
        };
    }
};

struct NPBuffer
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

    static std::vector<VkBuffer> extractVkBuffers(const std::vector<NPBuffer>& buffers)
    {
        std::vector<VkBuffer> vkBuffers;
        vkBuffers.reserve(buffers.size());
        for (const auto& buffer : buffers)
        {
            vkBuffers.push_back(buffer.buffer);
        }

        return vkBuffers;
    }
};

struct NPImage
{
    VkImage image = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;
    VmaAllocationInfo allocInfo;

    uint32_t width = 0;
    uint32_t height = 0;
    VkFormat format = VK_FORMAT_UNDEFINED;

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

struct NPPipeline
{
    VkPipelineLayout layout = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;

    void NPPipeline::destroy(VkDevice device)
    {
        if (pipeline != VK_NULL_HANDLE)
        {
            vkDestroyPipeline(device, pipeline, nullptr);
            pipeline = VK_NULL_HANDLE;
        }

        if (layout != VK_NULL_HANDLE)
        {
            vkDestroyPipelineLayout(device, layout, nullptr);
            layout = VK_NULL_HANDLE;
        }
    }
};

struct NPFrame
{
    VkSemaphore donePresentingSemaphore;
    VkFence doneExecutingFence;
    VkCommandBuffer commandBuffer;

    void destroy(VkDevice device, VmaAllocator allocator)
    {
        vkDestroyFence(device, doneExecutingFence, nullptr);
        vkDestroySemaphore(device, donePresentingSemaphore, nullptr);
    }
};

enum class NPQueueType : uint8_t
{
    GRAPHICS,
    TRANSFER,
    COMPUTE
};

struct NPQueue
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
struct NPCameraRecord
{
    alignas(16) FLOAT4X4 model;
    alignas(16) FLOAT4X4 view;
    alignas(16) FLOAT4X4 proj;
};

struct NPMeshRecord
{
    uint32_t vertexOffset;
    uint32_t indexOffset;
    uint32_t indexCount;
    uint32_t vertexCount;
    
    uint32_t transformIndex;
};

struct NPMesh
{
    uint64_t objectId;  // the hash of the mesh's `SdfPath`
    NPScenePath scenePath;

    std::vector<uint32_t> indices;
    std::vector<NPVertex> vertices;

    // NOTE: this vertex data should be stored flattened.
    // i.e. `_positions.size() == indices.size()`, etc.
    std::vector<FLOAT3> _positions;
    std::vector<FLOAT3> _normals;
    std::vector<FLOAT2> _uvs;
    std::vector<FLOAT3> _colors;
    std::vector<uint32_t> _materialIds;

    FLOAT4X4 objectToWorld;
    FLOAT4X4 worldToObject;  // model matrix

    FLOAT3 bboxMin;
    FLOAT3 bboxMax;

    void populateVertices()
    {
        size_t count = _positions.size();

        this->vertices.clear();
        this->vertices.reserve(count);

        for (size_t i = 0; i < count; i++)
        {
            NPVertex v{};
            v.pos = FLOAT4(_positions[i], 0);
            v.color = (i < _colors.size()) ? FLOAT4(_colors[i], 0): FLOAT4{ 1.0f, 1.0f, 1.0f, 1.0f };
            v.uv = (i < _uvs.size()) ? _uvs[i] : FLOAT2{ 0.0f, 0.0f };
            v.pad0 = FLOAT2{0.0f, 0.0f};
            vertices.push_back(v);
        }
    }
};

enum class NPLightType : uint8_t
{
    POINT,
    AREA
};

struct NPLightRecord
{
    uint32_t lightTransformIndex;
    FLOAT4 color;
    float intensity;
};

struct NPLight
{
    NPLightType type;

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

enum class NPStylizationFunction : uint8_t
{
    PASSTHROUGH
};

struct NPRenderSettings
{
    // general settings
    uint32_t maxDepth = 1;
    uint32_t samplesPerPixel = 1;

    // stylization-specific
    NPStylizationFunction stylizationFunction = NPStylizationFunction::PASSTHROUGH;
};

struct NPRendererAovs
{
    NPImage* color;
    NPImage* depth;
    // normals?
};

struct SwapchainParams
{
    VkSurfaceFormatKHR format;
    VkPresentModeKHR presentMode;
    VkExtent2D extent;
    VkFormat depthFormat;
};

struct NPDescriptorSetLayout
{
    VkDescriptorSetLayout layout;
    VkDescriptorPool pool;
    
    void destroy(VkDevice device)
    {
        if (pool != VK_NULL_HANDLE)
        {
            vkDestroyDescriptorPool(device, pool, nullptr);
        }
        
        if (layout != VK_NULL_HANDLE)
        {
            vkDestroyDescriptorSetLayout(device, layout, nullptr);
        }
    }
};