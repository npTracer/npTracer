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
    context.createGraphicsPipeline();
    context.createCommandPool();
    context.createSyncAndFrameObjects();
    context.createVertexBuffer();
}

void App::render()
{
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
        context.drawFrame(window);
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
