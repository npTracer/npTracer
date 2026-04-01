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
    int FRAME_COUNT = 0;
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
    std::array<uint32_t, 2> queueFamilyIndices;
    VkCommandBuffer transferCommandBuffer = VK_NULL_HANDLE;

    void setFrameCount(const int frameCount)
    {
        FRAME_COUNT = frameCount;
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
    void endCommandBuffer(VkCommandBuffer commandBuffer, NPQueueType queueFamily);

    // buffers
    bool createBuffer(NPBuffer& handle, VkDeviceSize size, VkBufferUsageFlags usage,
                      VmaAllocationCreateFlags allocationFlags);
    bool createDeviceLocalBuffer(NPBuffer& handle, const void* data, VkDeviceSize size,
                                 VkBufferUsageFlags usage);
    void copyBuffer(NPBuffer& src, NPBuffer& dst, VkDeviceSize size);
    VkDeviceAddress getBufferDeviceAddress(NPBuffer& buffer);

    // images
    void createImage(NPImage& handle, VkImageType type, VkFormat format, uint32_t width,
                     uint32_t height, VkImageUsageFlags usage,
                     VmaAllocationCreateFlags allocationFlags,  VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT, bool shouldCreateView = true) const;
    void createTextureImage(NPImage& handle, void* pixels, uint32_t width, uint32_t height, TextureOwnership ownership);
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

    // acceleration structures
    void createBottomLevelAccelerationStructure(
        VkCommandBuffer& commandBuffer, 
        NPAccelerationStructure& handle, 
        VkDeviceAddress vertexAddress, 
        VkDeviceAddress indexAddress,
        uint32_t firstVertex,
        uint32_t vertexCount,
        uint32_t firstIndex,
        uint32_t indexCount);
    
    void createTopLevelAccelerationStructure(
        VkCommandBuffer& commandBuffer, 
        NPAccelerationStructure& handle, 
        std::vector<FLOAT4X4>& transforms,
        std::vector<NPAccelerationStructure>& blasses);
    
    // descriptors
    void createDescriptorSetLayout(NPDescriptorSetLayout& descriptorSetLayout, std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding>& bindings);
    void allocateDesciptorSet(VkDescriptorSet& descriptorSet, NPDescriptorSetLayout& descriptorSetLayout);
    void writeDescriptorSetBuffers(VkDescriptorSet& descriptorSet,
    std::unordered_map<uint32_t, NPBuffer*>& bindingBufferMap, std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding>& bindingMap);
    void writeDescriptorSetImages(
        VkDescriptorSet& descriptorSet, 
        uint32_t binding, 
        const std::vector<NPImage>& images,
        VkSampler& sampler, 
        VkDescriptorType type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    void writeDescriptorSetAccelerationStructures(
        VkDescriptorSet& descriptorSet, 
        std::unordered_map<uint32_t, NPAccelerationStructure*>& bindingASMap,
        std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding>& bindingMap
        );
    
    // utility
    NPFrame& getCurrentFrame(uint32_t currentFrame);

    VkShaderModule createShaderModule(const std::vector<char>& code) const;
    void waitIdle();
    void destroyDebugMessenger();
    void destroy();

private:
    static void framebufferResizeCallback(GLFWwindow* window, int width, int height);
    
    // debug
    VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;

    void createDebugMessenger(bool enableDebug);

    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT type,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData);
};
