#pragma once

#include "framework.h"

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <vk_mem_alloc.h>

#include <string>
#include <array>
#include <optional>
#include <vector>

NP_TRACER_NAMESPACE_BEGIN

using FVec2 = glm::f32vec2;
using FVec3 = glm::f32vec3;
using FVec4 = glm::f32vec4;
using FMat4 = glm::f32mat4;

template<typename T>
using WrapRef = std::reference_wrapper<T>;

using ScenePath = std::string;
using ScenePathCollection = std::vector<ScenePath>;

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

    // `false` assumes top-left of image coordinate system is {0,0} (vulkan-default), `true` assumes bottom-left
    bool flipNDCY = true;
};

struct Vertex
{
    FVec4 pos;
    FVec4 normal;
    FVec4 color;
    FVec2 uv;
    FVec2 pad0;

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
    uint32_t materialIndex;
};

struct Mesh
{
    uint64_t objectId;  // the hash of the mesh's `SdfPath`
    ScenePath scenePath;

    std::vector<uint32_t> indices;
    std::vector<Vertex> vertices;

    // NOTE: this vertex data should be stored flattened.
    // i.e. `_positions.size() == indices.size()`, etc.
    std::vector<FVec3> _positions;
    std::vector<FVec3> _normals;
    std::vector<FVec2> _uvs;
    std::vector<FVec3> _colors;
    std::vector<uint32_t> _materialIds;

    FMat4 objectToWorld;
    FMat4 worldToObject;  // model matrix

    FVec3 bboxMin;
    FVec3 bboxMax;

    uint32_t materialIndex;

    void populateVertices()
    {
        size_t count = _positions.size();

        this->vertices.clear();
        this->vertices.reserve(count);

        for (size_t i = 0; i < count; i++)
        {
            Vertex v{ .pos = FVec4(_positions[i], 0),
                      .normal = {},
                      .color = (i < _colors.size()) ? FVec4(_colors[i], 0)
                                                    : FVec4{ 1.0f, 1.0f, 1.0f, 1.0f },
                      .uv = (i < _uvs.size()) ? _uvs[i] : FVec2{ 0.0f, 0.0f },
                      .pad0 = FVec2{ 0.0f, 0.0f } };
            vertices.push_back(v);
        }
    }
};

// camera
struct CameraRecord
{
    FMat4 view;
    FMat4 proj;
    FMat4 invView;
    FMat4 invProj;
};

using Camera = CameraRecord;

// lights
struct LightRecord
{
    uint32_t lightTransformIndex;
    FVec4 color;
    float intensity = UINT_MAX;
};

enum class LightType : uint8_t
{
    POINT,
    AREA
};

struct Light
{
    LightType type;

    FMat4 transform;

    FVec3 color;
    float intensity;

    // for area lights
    FVec3 u;
    FVec3 v;

    // for point / area
    float radius;

    uint64_t lightId;
};

struct MaterialRecord
{
    FVec4 diffuse = FVec4(0.f, 0.f, 0.f, 1.f);
    FVec4 ambient = FVec4(0.f, 0.f, 0.f, 1.f);
    FVec4 specular = FVec4(0.f, 0.f, 0.f, 1.f);
    FVec4 emission = FVec4(0.f, 0.f, 0.f, 1.f);

    uint32_t diffuseTextureIdx = UINT32_MAX;
};

struct Material : MaterialRecord
{
    uint64_t objectId = UINT64_MAX;  // the hash of the object's `SdfPath`
    ScenePath scenePath;

    MaterialRecord toRecord() const
    {
        return MaterialRecord(*this);
    }
};

// pixels should represent 4 channels
struct TextureRecord
{
    void* pixels;
    uint32_t width;
    uint32_t height;
};

using NPTexture = TextureRecord;

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
