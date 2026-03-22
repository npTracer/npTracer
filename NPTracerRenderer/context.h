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
    void createVertexBuffer();

    void beginCommandBuffer(VkCommandBuffer commandBuffer);
    void recordRenderingCommands(VkCommandBuffer commandBuffer, uint32_t imageIndex);
    void transitionImageLayout(VkCommandBuffer commandBuffer, VkImage image,
                               VkImageLayout oldLayout, VkImageLayout newLayout,
                               VkAccessFlags2 srcAccessMask, VkAccessFlags2 dstAccessMask,
                               VkPipelineStageFlags2 srcStageMask,
                               VkPipelineStageFlags2 dstStageMask);

    void drawFrame(GLFWwindow* window);
    void waitIdle();
    void recreateSwapchain(GLFWwindow* window);
    void cleanupSwapchain();

    // vma stuff
    void createAllocator();


    VkShaderModule createShaderModule(const std::vector<char>& code) const;
    void destroy();
    void destroyDebugMessenger();

private:
    const std::vector<Vertex> vertices = { { { 0.0f, -0.5f }, { 1.0f, 1.0f, 1.0f } },
                                           { { 0.5f, 0.5f }, { 0.0f, 1.0f, 0.0f } },
                                           { { -0.5f, 0.5f }, { 0.0f, 0.0f, 1.0f } } };

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

    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;

    std::unordered_map<QueueFamily, Queue> queues;
    VkCommandBuffer transferCommandBuffer = VK_NULL_HANDLE;

    VmaAllocator allocator = VK_NULL_HANDLE;
    Buffer vertexBuffer;

    // debug
    VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;

    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT type,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData);

    static void framebufferResizeCallback(GLFWwindow* window, int width, int height);

    void createDebugMessenger(bool enableDebug);
};
