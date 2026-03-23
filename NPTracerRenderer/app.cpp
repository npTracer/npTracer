#include "app.h"

void App::create()
{
    context.createWindow(window, WIDTH, HEIGHT);
    context.createInstance(enableDebug);
    context.createSurface(window);
    context.createPhysicalDevice();
    context.createLogicalDeviceAndQueues();
    context.createAllocator();

    context.createSwapchain(window);
    context.createSwapchainImageViews();

    context.createDescriptorSetLayout();
    context.createDepthImage();
    context.createGraphicsPipeline();
    context.createSyncAndFrameObjects();
    context.createRenderingResources();

    context.createDescriptorPool();
    context.createDescriptorSets();
}

void App::render()
{
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
        context.drawFrame(window);
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
