#include "app.h"

void App::create()
{
    context.createWindow(window, WIDTH, HEIGHT);
    context.createInstance(enableDebug);
    context.createSurface(window);
    context.createPhysicalDevice();
    context.createLogicalDeviceAndQueues();
    context.createSwapchain(window);
}

void App::render()
{
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
    }
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
