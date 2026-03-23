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
    // vulkan
    void createWindow(GLFWwindow*& window, int width, int height);
    void createInstance(bool enableDebug);
    void createSurface(GLFWwindow* window);
    void createPhysicalDevice();
    void createLogicalDeviceAndQueues();
    void createAllocator();
    void createSwapchain(GLFWwindow* window);
    void createSwapchainImageViews();
    void createGraphicsPipeline();
    void createSyncAndFrameObjects();

    void createRenderingResources();  // placeholder function for testing rendering functionality

    // command buffers
    void createCommandBuffer(VkCommandBuffer& commandBuffer, QueueFamily queueFamily);
    void beginCommandBuffer(VkCommandBuffer commandBuffer, VkCommandBufferUsageFlags flags = 0);
    void endCommandBuffer(VkCommandBuffer commandBuffer, QueueFamily queueFamily);

    // buffers
    void createBuffer(Buffer& handle, VkDeviceSize size, VkBufferUsageFlags usage,
                      VmaAllocationCreateFlags allocationFlags);
    void createDeviceLocalBuffer(Buffer& handle, void* data, VkDeviceSize size,
                                 VkBufferUsageFlags usage);
    void copyBuffer(Buffer& src, Buffer& dst, VkDeviceSize size);


    // images
    void createImage(Image& handle, VkImageType type, VkFormat format, uint32_t width,
                     uint32_t height, VkImageUsageFlags usage,
                     VmaAllocationCreateFlags allocationFlags);
    void createTextureImage();
    void createDepthImage();
    void createTextureSampler();
    void copyBufferToImage(VkCommandBuffer commandBuffer, Buffer& src, Image& dst, uint32_t width,
                           uint32_t height);
    void transitionImageLayout(VkCommandBuffer commandBuffer, VkImage image,
                               VkImageLayout oldLayout, VkImageLayout newLayout,
                               VkAccessFlags2 srcAccessMask, VkAccessFlags2 dstAccessMask,
                               VkPipelineStageFlags2 srcStageMask,
                               VkPipelineStageFlags2 dstStageMask,
                               VkImageAspectFlags aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT);

    // descriptors
    void createDescriptorSetLayout();
    void createDescriptorPool();
    void createDescriptorSets();

    // draw call
    void executeDrawCall(GLFWwindow* window);
    void populateDrawCall(VkCommandBuffer commandBuffer, uint32_t imageIndex);
    void updateUniformBuffer();

    // utility
    void recreateSwapchain(GLFWwindow* window);
    void cleanupSwapchain();

    VkShaderModule createShaderModule(const std::vector<char>& code) const;
    void waitIdle();
    void destroyDebugMessenger();
    void destroy();

private:
    const std::vector<Vertex> vertices = {
        { { -0.5f, -0.5f, 0.0f }, { 1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f } },
        { { 0.5f, -0.5f, 0.0f }, { 0.0f, 1.0f, 0.0f }, { 1.0f, 0.0f } },
        { { 0.5f, 0.5f, 0.0f }, { 0.0f, 0.0f, 1.0f }, { 1.0f, 1.0f } },
        { { -0.5f, 0.5f, 0.0f }, { 1.0f, 1.0f, 1.0f }, { 0.0f, 1.0f } },

        { { -0.5f, -0.5f, -0.5f }, { 1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f } },
        { { 0.5f, -0.5f, -0.5f }, { 0.0f, 1.0f, 0.0f }, { 1.0f, 0.0f } },
        { { 0.5f, 0.5f, -0.5f }, { 0.0f, 0.0f, 1.0f }, { 1.0f, 1.0f } },
        { { -0.5f, 0.5f, -0.5f }, { 1.0f, 1.0f, 1.0f }, { 0.0f, 1.0f } }
    };

    const std::vector<uint16_t> indices = { 0, 1, 2, 2, 3, 0, 4, 5, 6, 6, 7, 4 };

    static constexpr int FRAME_COUNT = 2;
    uint32_t currentFrame = 0;
    bool framebufferResized = false;

    // vulkan basics
    VkInstance instance = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VmaAllocator allocator = VK_NULL_HANDLE;

    // swapchain
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    SwapchainParams swapchainParams;
    std::vector<VkImage> swapchainImages;
    std::vector<VkImageView> swapchainImageViews;
    std::vector<VkSemaphore> doneRenderingSemaphores;
    std::vector<Frame> frames;

    // queues
    std::unordered_map<QueueFamily, Queue> queues;
    std::array<uint32_t, 2> queueFamilyIndices;
    VkCommandBuffer transferCommandBuffer = VK_NULL_HANDLE;

    // pipeline
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;

    // descriptors
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;

    // rendering resources
    Buffer vertexBuffer;
    Buffer indexBuffer;

    Image textureImage;
    Image depthImage;
    VkSampler textureSampler;

    // debug
    VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;

    void createDebugMessenger(bool enableDebug);

    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT type,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData);

    static void framebufferResizeCallback(GLFWwindow* window, int width, int height);
};
