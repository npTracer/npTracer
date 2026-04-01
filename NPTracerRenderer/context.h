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

    // swapchain
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    SwapchainParams swapchainParams;
    std::vector<VkImage> swapchainImages;
    std::vector<VkImageView> swapchainImageViews;
    std::vector<VkSemaphore> doneRenderingSemaphores;

    // queues
    std::unordered_map<NPQueueType, NPQueue> queues;
    std::vector<uint32_t> queueFamilyIndices;  // stored indices based on if they are supported
    VkCommandBuffer transferCommandBuffer = VK_NULL_HANDLE;

    void setFrameCount(const int frameCount)
    {
        kFrameCount = frameCount;
    }

    void createWindow(GLFWwindow*& window, int width, int height);
    void createInstance(bool enableDebug);
    void createPhysicalDevice();
    void createLogicalDeviceAndQueues();
    void createAllocator();
    void createSyncAndFrameObjects();

    // swapchain
    void createSurface(GLFWwindow* window);
    void createSwapchain(GLFWwindow* window);
    void recreateSwapchain(GLFWwindow* window);
    void cleanupSwapchain();

    // command buffers
    void createCommandBuffer(VkCommandBuffer& commandBuffer, NPQueueType queueFamily);
    void beginCommandBuffer(VkCommandBuffer commandBuffer, VkCommandBufferUsageFlags flags = 0);
    void endCommandBuffer(VkCommandBuffer commandBuffer, NPQueueType queueFamily,
                          VkPipelineStageFlags waitDstFlags = 0, VkFence fence = VK_NULL_HANDLE,
                          VkSemaphore waitSemaphores = VK_NULL_HANDLE,
                          VkSemaphore signalSemaphores = VK_NULL_HANDLE);
    void freeCommandBuffer(VkCommandBuffer commandBuffer, NPQueueType queueFamily);

    // buffers
    bool createBuffer(NPBuffer& handle, VkDeviceSize size, VkBufferUsageFlags usage,
                      VmaAllocationCreateFlags allocationFlags) const;
    bool createDeviceLocalBuffer(NPBuffer& handle, const void* data, VkDeviceSize size,
                                 VkBufferUsageFlags usage);
    void copyBuffer(NPBuffer& src, NPBuffer& dst, VkDeviceSize size);
    VkDeviceAddress getBufferDeviceAddress(NPBuffer& buffer);

    // images
    void createImage(NPImage& handle, VkImageType type, VkFormat format, uint32_t width,
                     uint32_t height, VkImageUsageFlags usage,
                     VmaAllocationCreateFlags allocationFlags,
                     VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT,
                     bool shouldCreateView = true) const;
    void createTextureImage(NPImage& handle, void* pixels, uint32_t width, uint32_t height);
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

    // descriptors
    void createDescriptorSetLayout(
        NPDescriptorSetLayout& descriptorSetLayout,
        std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding>& bindings);
    void allocateDesciptorSet(VkDescriptorSet& descriptorSet,
                              NPDescriptorSetLayout& descriptorSetLayout);
    void writeDescriptorSetBuffers(
        VkDescriptorSet& descriptorSet, std::unordered_map<uint32_t, NPBuffer*>& bindingBufferMap,
        std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding>& bindingMap);
    void writeDescriptorSetImages(VkDescriptorSet& descriptorSet, uint32_t binding,
                                  const std::vector<NPImage>& images, VkSampler& sampler);

    // utility
    NPFrame& getCurrentFrame(uint32_t currentFrame);

    VkShaderModule createShaderModule(const std::vector<char>& code) const;
    void waitIdle();
    void destroyDebugMessenger();
    void destroy();

private:
    static void sFramebufferResizeCallback(GLFWwindow* window, int width, int height);

    // debug
    VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;

    void createDebugMessenger(bool enableDebug);

    static VKAPI_ATTR VkBool32 VKAPI_CALL sDebugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT type,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData);

    static void sPopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);
};
