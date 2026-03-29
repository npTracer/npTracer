#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#include <vector>
#include <unordered_map>

#include "structs.h"

class Context
{
public:
    int kFrameCount;
    bool framebufferResized = false;

    // vulkan basics
    VkInstance instance = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VmaAllocator allocator = VK_NULL_HANDLE;
    NPImage depthImage;
    VkFormat depthFormat;
    std::vector<NPFrame> frames;

    // queues
    std::unordered_map<NPQueueType, NPQueue> queues;
    std::vector<uint32_t> queueFamilyIndices;  // stored indices based on if they are supported
    VkCommandBuffer transferCommandBuffer = VK_NULL_HANDLE;

    void setFrameCount(const int frameCount)
    {
        kFrameCount = frameCount;
    }

    void createInstance(bool enableDebug);
    void createPhysicalDevice();
    void createLogicalDeviceAndQueues();
    void createAllocator();
    void createSyncAndFrameObjects();

    // command buffers
    void createCommandBuffer(VkCommandBuffer& commandBuffer, NPQueueType queueFamily);
    void beginCommandBuffer(VkCommandBuffer commandBuffer, VkCommandBufferUsageFlags flags = 0);
    void endCommandBuffer(VkCommandBuffer commandBuffer, NPQueueType queueFamily,
                          VkPipelineStageFlags waitDstFlags = 0, VkFence fence = VK_NULL_HANDLE);
    void freeCommandBuffer(VkCommandBuffer commandBuffer, NPQueueType queueFamily);

    // buffers
    bool createBuffer(NPBuffer& handle, VkDeviceSize size, VkBufferUsageFlags usage,
                      VmaAllocationCreateFlags allocationFlags) const;
    bool createDeviceLocalBuffer(NPBuffer& handle, const void* data, VkDeviceSize size,
                                 VkBufferUsageFlags usage);
    void copyBuffer(NPBuffer& src, NPBuffer& dst, VkDeviceSize size);

    // images
    void createImage(NPImage& handle, VkImageType type, VkFormat format, uint32_t width,
                     uint32_t height, VkImageUsageFlags usage,
                     VmaAllocationCreateFlags allocationFlags, bool shouldCreateView = true) const;
    void createTextureImage(NPImage& handle);
    void createDepthImage(uint32_t width, uint32_t height);
    void createTextureSampler(VkSampler& sampler);
    void copyBufferToImage(VkCommandBuffer commandBuffer, NPBuffer& src, NPImage& dst,
                           uint32_t width, uint32_t height);
    void copyImageToBuffer(VkCommandBuffer commandBuffer, NPImage& src, NPBuffer& dst,
                           uint32_t width, uint32_t height);
    void transitionImageLayout(VkCommandBuffer commandBuffer, VkImage image,
                               VkImageLayout oldLayout, VkImageLayout newLayout,
                               VkAccessFlags2 srcAccessMask, VkAccessFlags2 dstAccessMask,
                               VkPipelineStageFlags2 srcStageMask,
                               VkPipelineStageFlags2 dstStageMask,
                               VkImageAspectFlags aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT);

    // utility
    NPFrame& getCurrentFrame(uint32_t currentFrame);

    VkShaderModule createShaderModule(const std::vector<char>& code) const;
    void waitIdle();
    void destroyDebugMessenger();
    void destroy();

private:
    // debug
    VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;

    void createDebugMessenger(bool enableDebug);

    static VKAPI_ATTR VkBool32 VKAPI_CALL sDebugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT type,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData);

    static void sPopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);
};
