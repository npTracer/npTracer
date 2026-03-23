#include "app.h"

void App::create()
{
    // create vulkan basics
    context.createWindow(window, WIDTH, HEIGHT);
    context.createInstance(enableDebug);
    context.createSurface(window);
    context.createPhysicalDevice();
    context.createLogicalDeviceAndQueues();
    context.createAllocator();
    context.createSwapchain(window);
    context.createSwapchainImageViews();

    // create resources for rendering
    context.createDescriptorSetLayout();
    context.createDepthImage();
    context.createGraphicsPipeline();
    context.createSyncAndFrameObjects();
    context.createRenderingResources();

    // create descriptors
    context.createDescriptorPool();
    context.createDescriptorSets();
}

void App::render()
{
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
        context.executeDrawCall(window);
        context.updateUniformBuffer();
    }

    context.waitIdle();
}

void App::run()
{
    create();
    render();
    destroy();
}

void App::destroy()
{
    context.destroy();

    if (window)
    {
        glfwDestroyWindow(window);
        window = nullptr;
    }

    glfwTerminate();
}
