#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <iostream>
#include <stdexcept>
#include <vector>
#include <vulkan/vulkan.h>

#include "utils.h"
#include "config.h"

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
    void createCommandPool();
    void createCommandBuffer(VkCommandBuffer& commandBuffer);
    void createSyncAndFrameObjects();

    void beginCommandBuffer(VkCommandBuffer commandBuffer);
    void recordRenderingCommands(VkCommandBuffer commandBuffer, uint32_t imageIndex);
    void transitionImageLayout(VkCommandBuffer commandBuffer, VkImage image,
                               VkImageLayout oldLayout,
                               VkImageLayout newLayout, VkAccessFlags2 srcAccessMask,
                               VkAccessFlags2 dstAccessMask, VkPipelineStageFlags2 srcStageMask,
                               VkPipelineStageFlags2 dstStageMask);

    void drawFrame();
    void waitIdle();

    VkShaderModule createShaderModule(const std::vector<char>& code) const;
    void destroy();
    void destroyDebugMessenger();

private:
    struct SwapchainParams
    {
        VkSurfaceFormatKHR format;
        VkPresentModeKHR presentMode;
        VkExtent2D extent;
    };

    static constexpr int FRAME_COUNT = 2;
    uint32_t currentFrame = 0;

    VkInstance instance = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;

    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    SwapchainParams swapchainParams;
    std::vector<VkImage> swapchainImages;
    std::vector<VkImageView> swapchainImageViews;

    // per image
    std::vector<VkSemaphore> doneRenderingSemaphores;

    // per frame
    std::vector<VkSemaphore> donePresentingSemaphores;
    std::vector<VkFence> doneExecutingFences;
    std::vector<VkCommandBuffer> commandBuffers;

    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;

    VkCommandPool commandPool = VK_NULL_HANDLE;

    VkQueue graphicsQueue = VK_NULL_HANDLE;
    uint32_t graphicsQueueFamilyIndex = 0;

    // debug
    VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;

    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT type,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData);

    void createDebugMessenger(bool enableDebug);
};
