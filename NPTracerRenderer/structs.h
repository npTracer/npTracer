#pragma once

#include "framework.h"

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <vk_mem_alloc.h>

#include <string>
#include <array>
#include <optional>
#include <vector>
#include <memory>

// alias-like template types can stay in global namespace
using FLOAT2 = glm::f32vec2;
using FLOAT3 = glm::f32vec3;
using FLOAT4 = glm::f32vec4;
using FLOAT4x4 = glm::f32mat4x4;

template<typename T>
using WRAP_REF = std::reference_wrapper<T>;

template<typename T>
using UPTR = std::unique_ptr<T>;

NP_TRACER_NAMESPACE_BEGIN

using ScenePath = std::string;

enum class eSceneType : uint8_t
{
    ASSIMP,
    DEFAULT
};

enum class eExecutionMode : uint8_t
{
    OFFSCREEN,
    SWAPCHAIN
};

// these values are renderer-level and thus differ from render settings
struct RendererConstants
{
    eExecutionMode executionMode = eExecutionMode::OFFSCREEN;
    eSceneType sceneType = eSceneType::DEFAULT;
};

struct Vertex
{
    FLOAT4 pos;
    FLOAT4 normal;
    FLOAT4 color;
    FLOAT2 uv;
    FLOAT2 pad0;

    // tell vulkan how vertices should be moved through
    static VkVertexInputBindingDescription getBindingDescription()
    {
        return VkVertexInputBindingDescription{ .binding = 0,
                                                .stride = sizeof(Vertex),
                                                .inputRate = VK_VERTEX_INPUT_RATE_VERTEX };
    }

    // tell vulkan what attributes exist within each vertex and how big they are
    static std::array<VkVertexInputAttributeDescription, 4> getAttributeDescriptions()
    {
        return {
            VkVertexInputAttributeDescription{ .location = 0,
                                               .binding = 0,
                                               .format = VK_FORMAT_R32G32B32_SFLOAT,
                                               .offset = offsetof(Vertex, pos) },
            VkVertexInputAttributeDescription{ .location = 1,
                                               .binding = 0,
                                               .format = VK_FORMAT_R32G32B32_SFLOAT,
                                               .offset = offsetof(Vertex, normal) },
            VkVertexInputAttributeDescription{ .location = 2,
                                               .binding = 0,
                                               .format = VK_FORMAT_R32G32B32_SFLOAT,
                                               .offset = offsetof(Vertex, color) },
            VkVertexInputAttributeDescription{ .location = 3,
                                               .binding = 0,
                                               .format = VK_FORMAT_R32G32_SFLOAT,
                                               .offset = offsetof(Vertex, uv) },
        };
    }
};

struct Buffer
{
    VkBuffer buffer = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;
    VmaAllocationInfo allocInfo{};

    void destroy(VmaAllocator allocator)
    {
        if (buffer != VK_NULL_HANDLE)
        {
            vmaDestroyBuffer(allocator, buffer, allocation);
        }
    }

    static std::vector<VkBuffer> extractVkBuffers(const std::vector<Buffer>& buffers)
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

struct Image
{
    VkImage image = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;
    VmaAllocationInfo allocInfo = {};
    VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;

    uint32_t width = -1;
    uint32_t height = -1;
    VkFormat format = VK_FORMAT_UNDEFINED;

    // NOTE: `commandBuffer` must be ready for write
    void transitionLayout(VkCommandBuffer commandBuffer, VkImageLayout newLayout,
                          VkAccessFlags2 srcAccessMask, VkAccessFlags2 dstAccessMask,
                          VkPipelineStageFlags2 srcStageMask, VkPipelineStageFlags2 dstStageMask,
                          std::optional<VkImageAspectFlags> overrideAspect = std::nullopt)
    {
        const VkImageAspectFlags aspectMask = overrideAspect.value_or(
            format == VK_FORMAT_D32_SFLOAT ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT);

        VkImageMemoryBarrier2 barrier{ .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                                       .srcStageMask = srcStageMask,
                                       .srcAccessMask = srcAccessMask,
                                       .dstStageMask = dstStageMask,
                                       .dstAccessMask = dstAccessMask,
                                       .oldLayout = layout,
                                       .newLayout = newLayout,
                                       .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                       .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                       .image = image,
                                       .subresourceRange = { .aspectMask = aspectMask,
                                                             .baseMipLevel = 0,
                                                             .levelCount = 1,
                                                             .baseArrayLayer = 0,
                                                             .layerCount = 1 } };

        VkDependencyInfo dependencyInfo{ .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                                         .dependencyFlags = {},
                                         .imageMemoryBarrierCount = 1,
                                         .pImageMemoryBarriers = &barrier };

        vkCmdPipelineBarrier2(commandBuffer, &dependencyInfo);

        layout = newLayout;
    }

    void destroy(VkDevice device, VmaAllocator allocator)
    {
        if (view != VK_NULL_HANDLE)
        {
            vkDestroyImageView(device, view, nullptr);
            view = VK_NULL_HANDLE;
        }

        if (image != VK_NULL_HANDLE)
        {
            vmaDestroyImage(allocator, image, allocation);
            image = VK_NULL_HANDLE;
            allocInfo = {};
        }
    }
};

struct Pipeline
{
    VkPipelineLayout layout = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;

    void destroy(VkDevice device)
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

struct Frame
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

struct SwapchainParams
{
    VkSurfaceFormatKHR surfaceFormat;
    VkPresentModeKHR presentMode;
    VkExtent2D extent;
};

struct DescriptorSetLayout
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

enum class QueueType : uint8_t
{
    GRAPHICS,
    TRANSFER,
    COMPUTE,
    _COUNT  // sentinel
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

struct ShaderBindingTable
{
    uint32_t handleSize;
    uint32_t handleAlign;
    uint32_t baseAlign;

    Buffer buffer;
    VkDeviceAddress deviceAddress;

    VkStridedDeviceAddressRegionKHR rgen{};
    VkStridedDeviceAddressRegionKHR miss{};
    VkStridedDeviceAddressRegionKHR hit{};
    VkStridedDeviceAddressRegionKHR callable{};

    void destroy(VmaAllocator allocator)
    {
        buffer.destroy(allocator);
    }
};

struct AccelerationStructure
{
    VkAccelerationStructureKHR accelerationStructure = VK_NULL_HANDLE;
    Buffer handleBuffer;
    Buffer scratchBuffer;
    VkDeviceAddress deviceAddress;

    void destroyBuffers(VkDevice device, VmaAllocator allocator)
    {
        // VkDestroyAccelerationStructure requires context so destroy it outside of struct

        handleBuffer.destroy(allocator);
        scratchBuffer.destroy(allocator);
    }
};

// primitive types

// meshes
struct MeshRecord
{
    uint32_t vertexOffset;
    uint32_t indexOffset;
    uint32_t indexCount;
    uint32_t vertexCount;

    uint32_t transformIndex;
    uint32_t materialIndex = UINT32_MAX;
};

struct Mesh
{
    ScenePath scenePath;

    FLOAT4x4 transform = FLOAT4x4(1.f);  // i.e. objectToWorld

    std::vector<uint32_t> indices;
    std::vector<Vertex> vertices;

    // NOTE: this vertex data should be stored flattened.
    // i.e. `_positions.size() == indices.size()`, etc.
    std::vector<FLOAT3> _positions;
    std::vector<FLOAT3> _normals;
    std::vector<FLOAT2> _uvs;
    std::vector<FLOAT3> _colors;

    // NOTE: since Hydra does not guarantee creating materials before meshes, we save the material's unique `SdfPath` to fill in the `materialIndex` during 'finalization'
    ScenePath _materialScenePath;
    uint32_t materialIndex = UINT32_MAX;

    bool bMaterialNeedsFinalization = false;

    void populateVertices()
    {
        size_t count = _positions.size();

        this->vertices.clear();
        this->vertices.reserve(count);

        for (size_t i = 0; i < count; i++)
        {
            Vertex v{ .pos = FLOAT4(_positions[i], 0),
                      .normal = {},
                      .color = (i < _colors.size()) ? FLOAT4(_colors[i], 0)
                                                    : FLOAT4{ 1.0f, 1.0f, 1.0f, 1.0f },
                      .uv = (i < _uvs.size()) ? _uvs[i] : FLOAT2{ 0.0f, 0.0f },
                      .pad0 = FLOAT2{ 0.0f, 0.0f } };
            vertices.push_back(v);
        }
    }
};

// camera
struct CameraRecord
{
    FLOAT4x4 view;
    FLOAT4x4 proj;
    FLOAT4x4 invView;
    FLOAT4x4 invProj;
};

using Camera = CameraRecord;

// lights
struct LightRecord
{
    FLOAT4x4 transform = FLOAT4x4{ 1.f };
    FLOAT4 color = FLOAT4{ 1.f, 1.f, 1.f, 1.f };  // 4 channels for alignment purposes
    float intensity = 1.f;
    float exposure = 0.f;
};

enum class LightType : uint8_t
{
    POINT,
    AREA
};

struct Light : LightRecord
{
    LightType type = LightType::POINT;

    LightRecord toRecord() const
    {
        return LightRecord(*this);
    }
};

// materials
struct MaterialRecord
{
    FLOAT4 diffuse = FLOAT4(0.f, 0.f, 0.f, 1.f);
    FLOAT4 ambient = FLOAT4(0.f, 0.f, 0.f, 1.f);
    FLOAT4 specular = FLOAT4(0.f, 0.f, 0.f, 1.f);
    FLOAT4 emission = FLOAT4(0.f, 0.f, 0.f, 1.f);

    uint32_t diffuseTextureIndex = UINT32_MAX;
};

struct Material : MaterialRecord
{
    ScenePath scenePath;

    MaterialRecord toRecord() const
    {
        return MaterialRecord(*this);
    }
};

// textures
struct TextureRecord
{
    void* pixels;  // pixels should have 4 channels
    uint32_t width;
    uint32_t height;
};

using Texture = TextureRecord;

enum class StylizationFunction : uint8_t
{
    PASSTHROUGH
};

struct RenderSettings
{
    // general settings
    uint32_t maxDepth = 1;
    uint32_t samplesPerPixel = 1;

    // stylization-specific
    StylizationFunction stylizationFunction = StylizationFunction::PASSTHROUGH;
};

struct RendererAovs
{
    Image* rgb = nullptr;
    Image* depth = nullptr;
    // normals?
};

NP_TRACER_NAMESPACE_END
