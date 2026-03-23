#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <iostream>
#include <stdexcept>
#include <vector>
#include <vulkan/vulkan.h>
#include <unordered_map>

#include "utils.h"
#include "structs.h"
#include "vk_mem_alloc.h"


class Context
{
public:
    void createWindow(GLFWwindow*& window, int width, int height);
    void createInstance(bool enableDebug);
    void createSurface(GLFWwindow* window);
    void createPhysicalDevice();
    void createLogicalDeviceAndQueues();
    void createSwapchain(GLFWwindow* window);
    void createSwapchainImageViews();
    void createGraphicsPipeline();
    void createCommandBuffer(VkCommandBuffer& commandBuffer, QueueFamily queueFamily);
    void createSyncAndFrameObjects();

    void beginCommandBuffer(VkCommandBuffer commandBuffer, VkCommandBufferUsageFlags flags = 0);
    void endCommandBuffer(VkCommandBuffer commandBuffer, QueueFamily queueFamily);
    void recordRenderingCommands(VkCommandBuffer commandBuffer, uint32_t imageIndex);
    void transitionImageLayout(VkCommandBuffer commandBuffer, VkImage image,
                               VkImageLayout oldLayout, VkImageLayout newLayout,
                               VkAccessFlags2 srcAccessMask, VkAccessFlags2 dstAccessMask,
                               VkPipelineStageFlags2 srcStageMask,
                               VkPipelineStageFlags2 dstStageMask);

    void drawFrame(GLFWwindow* window);
    void recreateSwapchain(GLFWwindow* window);
    void cleanupSwapchain();

    // resources
    void createDescriptorSetLayout();
    void createDescriptorPool();
    void createDescriptorSets();

    void createRenderingResources(); // placeholder function for testing rendering functionality
    void createAllocator();
    void createBuffer(Buffer& handle, VkDeviceSize size, VkBufferUsageFlags usage,
                      VmaAllocationCreateFlags allocationFlags);

    void createDeviceLocalBuffer(Buffer& handle, void* data, VkDeviceSize size,
                                 VkBufferUsageFlags usage);
    void createTextureImage();
    void createTextureSampler();
    void createImage(Image& handle, VkDeviceSize size, VkImageType type, VkFormat format,
                     uint32_t width, uint32_t height, VkImageUsageFlags usage,
                     VmaAllocationCreateFlags allocationFlags);

    void copyBuffer(Buffer& src, Buffer& dst, VkDeviceSize size);
    void copyBufferToImage(VkCommandBuffer commandBuffer, Buffer& src, Image& dst, uint32_t width,
                           uint32_t height);
    void updateUniformBuffer();

    // utility
    VkShaderModule createShaderModule(const std::vector<char>& code) const;
    void waitIdle();
    void destroy();
    void destroyDebugMessenger();

private:
    const std::vector<Vertex> vertices = {
        { { -0.5f, -0.5f }, { 1.0f, 0.0f, 0.0f }, { 1.0f, 0.0f } },
        { { 0.5f, -0.5f }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f } },
        { { 0.5f, 0.5f }, { 0.0f, 0.0f, 1.0f }, { 0.0f, 1.0f } },
        { { -0.5f, 0.5f }, { 1.0f, 1.0f, 1.0f }, { 1.0f, 1.0f } }
    };

    const std::vector<uint16_t> indices = { 0, 1, 2, 2, 3, 0 };

    static constexpr int FRAME_COUNT = 2;
    uint32_t currentFrame = 0;
    bool framebufferResized = false;

    VkInstance instance = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;

    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    SwapchainParams swapchainParams;
    std::vector<VkImage> swapchainImages;
    std::vector<VkImageView> swapchainImageViews;
    std::vector<VkSemaphore> doneRenderingSemaphores;

    std::vector<Frame> frames;

    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;

    std::unordered_map<QueueFamily, Queue> queues;
    std::array<uint32_t, 2> queueFamilyIndices;
    VkCommandBuffer transferCommandBuffer = VK_NULL_HANDLE;

    VmaAllocator allocator = VK_NULL_HANDLE;
    Buffer vertexBuffer;
    Buffer indexBuffer;

    Image textureImage;
    VkSampler textureSampler;

    // debug
    VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;

    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT type,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData);

    static void framebufferResizeCallback(GLFWwindow* window, int width, int height);

    void createDebugMessenger(bool enableDebug);
};
