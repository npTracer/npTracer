#include "context.h"
#include <optional>

void Context::createDebugMessenger(bool enableDebug)
{
    if (!enableDebug)
    {
        return;
    }

    VkDebugUtilsMessengerCreateInfoEXT createInfo{
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
                           | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
        .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
                       | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
                       | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
        .pfnUserCallback = debugCallback,
        .pUserData = nullptr
    };

    auto fn = (PFN_vkCreateDebugUtilsMessengerEXT)
        vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (fn)
    {
        fn(instance, &createInfo, nullptr, &debugMessenger);
    }
    else
    {
        throw std::runtime_error("debug layer not function proc addr not found");
    }
}

VKAPI_ATTR VkBool32 VKAPI_CALL Context::debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT type,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData)
{
    std::cerr << "validation: " << pCallbackData->pMessage << std::endl;
    return VK_FALSE;
}

void Context::destroyDebugMessenger()
{
    if (debugMessenger == VK_NULL_HANDLE)
    {
        return;
    }
    auto fn = (PFN_vkDestroyDebugUtilsMessengerEXT)
        vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");

    if (fn)
    {
        fn(instance, debugMessenger, nullptr);
    }

    debugMessenger = VK_NULL_HANDLE;
}

void Context::createWindow(GLFWwindow*& window, int width, int height)
{
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    window = glfwCreateWindow(width, height, "Engine", nullptr, nullptr);
}

void Context::createInstance(bool enableDebug)
{
    // glfw extensions
    uint32_t glfwExtensionCount = 0;
    auto glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
    if (enableDebug)
    {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    // layers
    std::vector<char const*> layers;
    if (enableDebug)
    {
        const std::vector<char const*> validationLayers = { "VK_LAYER_KHRONOS_validation" };
        layers.assign(validationLayers.begin(), validationLayers.end());
    }

    // TODO poll if layers/extensions are supported

    constexpr VkApplicationInfo appInfo{ .pApplicationName = "Engine",
                                         .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
                                         .pEngineName = "Bum",
                                         .engineVersion = VK_MAKE_VERSION(1, 0, 0),
                                         .apiVersion = VK_API_VERSION_1_4 };

    VkInstanceCreateInfo createInfo{ .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
                                     .pApplicationInfo = &appInfo,
                                     .enabledLayerCount = static_cast<uint32_t>(layers.size()),
                                     .ppEnabledLayerNames = layers.data(),
                                     .enabledExtensionCount = static_cast<uint32_t>(
                                         extensions.size()),
                                     .ppEnabledExtensionNames = extensions.data() };

    if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS)
    {
        throw std::runtime_error("instance creation failed");
    }

    if (enableDebug)
    {
        createDebugMessenger(enableDebug);
    }
}

void Context::createSurface(GLFWwindow* window)
{
    if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS)
    {
        throw std::runtime_error("surface creation failed");
    }
}

void Context::createPhysicalDevice()
{
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

    for (const auto& device : devices)
    {
        VkPhysicalDeviceProperties deviceProperties;
        vkGetPhysicalDeviceProperties(device, &deviceProperties);
        VkPhysicalDeviceFeatures deviceFeatures;
        vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

        if (deviceProperties.deviceType
            == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)  // TODO better selection criteria
        {
            physicalDevice = device;
            break;
        }
    }
}

void Context::createLogicalDeviceAndQueues()
{
    uint32_t propertyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &propertyCount, nullptr);
    std::vector<VkQueueFamilyProperties> properties(propertyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &propertyCount, properties.data());

    std::optional<uint32_t> graphicsIndex;
    for (uint32_t i = 0; i < properties.size(); i++)
    {
        const auto& family = properties[i];

        if (family.queueFlags & VK_QUEUE_GRAPHICS_BIT)
        {
            graphicsIndex = i;
            break;
        }
    }

    float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueCreateInfo{ .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                                             .queueFamilyIndex = graphicsIndex.value(),
                                             .queueCount = 1,
                                             .pQueuePriorities = &queuePriority };

    // TODO add device features
    VkPhysicalDeviceFeatures deviceFeatures{

    };

    VkDeviceCreateInfo createInfo{ .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
                                   .queueCreateInfoCount = 1,
                                   .pQueueCreateInfos = &queueCreateInfo,
                                   .pEnabledFeatures = &deviceFeatures };

    if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create logical device");
    }

    graphicsQueueFamilyIndex = graphicsIndex.value();
    vkGetDeviceQueue(device, graphicsQueueFamilyIndex, 0, &graphicsQueue);
}

void Context::destroy()
{
    if (device != VK_NULL_HANDLE)
    {
        vkDestroyDevice(device, nullptr);
    }

    if (surface != VK_NULL_HANDLE)
    {
        vkDestroySurfaceKHR(instance, surface, nullptr);
        surface = VK_NULL_HANDLE;
    }

    destroyDebugMessenger();

    if (instance != VK_NULL_HANDLE)
    {
        vkDestroyInstance(instance, nullptr);
        instance = VK_NULL_HANDLE;
    }
}
