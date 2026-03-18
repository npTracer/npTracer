#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <iostream>
#include <stdexcept>
#include <vector>
#include <vulkan/vulkan.h>

class Context
{
public:
    void createWindow(GLFWwindow*& window, int width, int height);
    void createInstance(bool enableDebug);
    void createSurface(GLFWwindow* window);
    void createPhysicalDevice();
    void createLogicalDeviceAndQueues();

    void destroy();

    void destroyDebugMessenger();

private:
    VkInstance instance = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;

    VkQueue graphicsQueue = VK_NULL_HANDLE;
    uint32_t graphicsQueueFamilyIndex = 0;

    // debug
    VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;

    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT type,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData);

    void createDebugMessenger(bool enableDebug);
};
