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

#include "glm/gtx/compatibility.hpp"

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

using SCENE_PATH = std::string;

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
    bool bFlipUVY = true;  // Hydra assumes bottom-left UV origin, Vulkan top-left
};

// constants set at pipeline creation time
struct SpecializationConstants
{
    uint32_t kFlipUVY = 1u;
};

struct PushConstants
{
    uint32_t numLights;
    uint32_t frameIndex;
};

struct Vertex
{
    FLOAT4 pos = FLOAT4(0.f, 0.f, 0.f, 1.f);
    FLOAT4 normal = FLOAT4(0.f, 0.f, 0.f, 1.f);
    FLOAT4 tangent = FLOAT4(0.f, 0.f, 0.f, 1.f);
    FLOAT4 color = FLOAT4(1.f);
    FLOAT2 uv = FLOAT2(0.f);
    FLOAT2 pad0 = FLOAT2(0.f);

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

    void destroy(VmaAllocator allocator) const
    {
        if (buffer != VK_NULL_HANDLE) vmaDestroyBuffer(allocator, buffer, allocation);
    }

    static std::vector<VkBuffer> extractVkBuffers(const std::vector<Buffer>& buffers)
    {
        std::vector<VkBuffer> vkBuffers;
        vkBuffers.reserve(buffers.size());
        for (const auto& buffer : buffers)
            vkBuffers.push_back(buffer.buffer);

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

    void destroy(VkDevice device, VmaAllocator allocator) const
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

    void destroy(VkDevice device) const
    {
        if (pool != VK_NULL_HANDLE) vkDestroyDescriptorPool(device, pool, nullptr);

        if (layout != VK_NULL_HANDLE) vkDestroyDescriptorSetLayout(device, layout, nullptr);
    }
};

enum class eQueueType : uint8_t
{
    GRAPHICS,
    TRANSFER,
    COMPUTE,
    COUNT_  // sentinel
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

    void destroy(VkDevice device) const
    {
        if (commandPool != VK_NULL_HANDLE) vkDestroyCommandPool(device, commandPool, nullptr);
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

    void destroy(VmaAllocator allocator) const
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

    void destroyBuffers(VmaAllocator allocator) const
    {
        // VkDestroyAccelerationStructure requires context so destroy it outside of struct

        handleBuffer.destroy(allocator);
        scratchBuffer.destroy(allocator);
    }
};

// primitive types

// meshes

enum eStylizationId : uint32_t
{
    GREYSCALE = 1u,
    TOON,
    STRIPES,
    CROSSHATCH,
    STYLIZATION_ID_COUNT_
};

// define shared members in this macro to maintain synchronization without explicit inheritance
#define MESH_SHARED_MEMBERS                                                                        \
    uint32_t materialIndex = UINT32_MAX;                                                           \
    eStylizationId stylizationId = eStylizationId::STYLIZATION_ID_COUNT_;

struct MeshRecord
{
    uint32_t vertexOffset;
    uint32_t indexOffset;
    uint32_t indexCount;
    uint32_t vertexCount;

    uint32_t transformIndex;
    MESH_SHARED_MEMBERS
};

struct Mesh
{
    SCENE_PATH scenePath;

    FLOAT4x4 transform = FLOAT4x4(1.f);  // i.e. objectToWorld

    std::vector<uint32_t> indices;
    std::vector<Vertex> vertices;

    // NOTE: since Hydra does not guarantee creating materials before meshes, we save the material's unique `SdfPath` to fill in the `materialIndex` during 'finalization'
    SCENE_PATH materialScenePath;
    MESH_SHARED_MEMBERS

    bool bMaterialNeedsFinalization = false;
};

// camera
struct CameraRecord
{
    FLOAT4x4 invView;
    FLOAT4x4 invProj;
};

struct Camera : CameraRecord
{
    FLOAT4x4 view;
    FLOAT4x4 proj;

    [[nodiscard]] CameraRecord toRecord() const
    {
        return CameraRecord{ *this };
    }
};

// lights
struct LightRecord
{
    FLOAT4x4 transform = FLOAT4x4{ 1.f };
    FLOAT4 color = FLOAT4{ 1.f, 1.f, 1.f, 1.f };  // 4 channels for alignment purposes
    float intensity = 1.f;
};

struct Light : LightRecord
{
    SCENE_PATH scenePath;

    [[nodiscard]] LightRecord toRecord() const
    {
        return LightRecord{ *this };
    }
};

// materials
struct MaterialRecord
{
    FLOAT4 diffuse = FLOAT4(1.f);
    FLOAT4 ambient = FLOAT4(0.f, 0.f, 0.f, 1.f);
    FLOAT4 specular = FLOAT4(0.f, 0.f, 0.f, 1.f);
    FLOAT4 emission = FLOAT4(0.f, 0.f, 0.f, 1.f);  // w stores emission intensity
    FLOAT2 metallic = FLOAT2(0.f);

    uint32_t diffuseTextureIndex = UINT32_MAX;
    uint32_t normalTextureIndex = UINT32_MAX;
    uint32_t metallicTextureIndex = UINT32_MAX;
};

struct Material : MaterialRecord
{
    SCENE_PATH scenePath;

    [[nodiscard]] MaterialRecord toRecord() const
    {
        return MaterialRecord{ *this };
    }
};

// textures
struct TextureRecord
{
    void* pixels;  // pixels should have 4 channels
    uint32_t width;
    uint32_t height;
};

struct Texture : TextureRecord
{
    void* pixels;  // pixels should have 4 channels
    uint32_t width;
    uint32_t height;
    bool unorm = false;
};

struct RenderSettings
{
    // general settings
    uint32_t maxDepth = 1;
    uint32_t samplesPerPixel = 1;
};

struct RendererTargets
{
    Image* color = nullptr;
    Image* depth = nullptr;
    // normals?
};

NP_TRACER_NAMESPACE_END
