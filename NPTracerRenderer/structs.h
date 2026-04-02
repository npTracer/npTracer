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
    VmaAllocationInfo allocInfo{};

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
    VmaAllocationInfo allocInfo = {};
    VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;

    uint32_t width = -1;
    uint32_t height = -1;
    VkFormat format = VK_FORMAT_UNDEFINED;

    // NOTE: `commandBuffer` must be ready for write
    void transitionLayout(VkCommandBuffer commandBuffer, VkImageLayout newLayout,
                          VkAccessFlags2 srcAccessMask, VkAccessFlags2 dstAccessMask,
                          VkPipelineStageFlags2 srcStageMask, VkPipelineStageFlags2 dstStageMask,
                          std::optional<VkImageAspectFlags> overrideAspectFlags = std::nullopt)
    {
        VkImageMemoryBarrier2 barrier{};

        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
        barrier.srcStageMask = srcStageMask;
        barrier.srcAccessMask = srcAccessMask;
        barrier.dstStageMask = dstStageMask;
        barrier.dstAccessMask = dstAccessMask;
        barrier.oldLayout = layout;
        barrier.newLayout = newLayout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image;

        barrier.subresourceRange.aspectMask = overrideAspectFlags.value_or(
            format == VK_FORMAT_D32_SFLOAT ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT);
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;

        VkDependencyInfo dependencyInfo{};

        dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        dependencyInfo.dependencyFlags = {};
        dependencyInfo.imageMemoryBarrierCount = 1;
        dependencyInfo.pImageMemoryBarriers = &barrier;

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

struct NPPipeline
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

struct NPSwapchainParams
{
    VkSurfaceFormatKHR surfaceFormat;
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

enum class NPQueueType : uint8_t
{
    GRAPHICS,
    TRANSFER,
    COMPUTE,
    _COUNT  // sentinel
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

struct NPShaderBindingTable
{
    uint32_t handleSize;
    uint32_t handleAlign;
    uint32_t baseAlign;

    NPBuffer buffer;
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

struct NPAccelerationStructure
{
    VkAccelerationStructureKHR accelerationStructure = VK_NULL_HANDLE;
    NPBuffer handleBuffer;
    NPBuffer scratchBuffer;
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
struct NPMeshRecord
{
    uint32_t vertexOffset;
    uint32_t indexOffset;
    uint32_t indexCount;
    uint32_t vertexCount;

    uint32_t transformIndex;
    uint32_t materialIndex;
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

    uint32_t materialIndex;

    void populateVertices()
    {
        size_t count = _positions.size();

        this->vertices.clear();
        this->vertices.reserve(count);

        for (size_t i = 0; i < count; i++)
        {
            NPVertex v{};
            v.pos = FLOAT4(_positions[i], 0);
            v.color = (i < _colors.size()) ? FLOAT4(_colors[i], 0)
                                           : FLOAT4{ 1.0f, 1.0f, 1.0f, 1.0f };
            v.uv = (i < _uvs.size()) ? _uvs[i] : FLOAT2{ 0.0f, 0.0f };
            v.pad0 = FLOAT2{ 0.0f, 0.0f };
            vertices.push_back(v);
        }
    }
};

// camera
struct NPCameraRecord
{
    alignas(16) FLOAT4X4 view;
    alignas(16) FLOAT4X4 proj;
    alignas(16) FLOAT4X4 invView;
    alignas(16) FLOAT4X4 invProj;
};

using NPCamera = NPCameraRecord;

// lights
struct NPLightRecord
{
    uint32_t lightTransformIndex;
    FLOAT4 color;
    float intensity = UINT_MAX;
};

enum class NPLightType : uint8_t
{
    POINT,
    AREA
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

    uint64_t lightId;
};

struct NPMaterialRecord
{
    FLOAT4 ambient;
    FLOAT4 diffuse;
    FLOAT4 specular;
    FLOAT4 emission;

    uint32_t diffuseTextureIdx;
};

struct NPMaterial : NPMaterialRecord
{
    uint64_t objectId;  // the hash of the object's `SdfPath`
    NPScenePath scenePath;

    NPMaterialRecord toRecord() const
    {
        return { ambient, diffuse, specular, emission };
    }
};

struct NPTextureRecord
{
    void* pixels;
    uint32_t width;
    uint32_t height;
};

using NPTexture = NPTextureRecord;

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
    NPImage* color = nullptr;
    NPImage* depth = nullptr;
    // normals?
};
