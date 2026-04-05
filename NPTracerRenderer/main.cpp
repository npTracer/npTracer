#include "app.h"

#include <iostream>
#include <stdexcept>
#include <string>

constexpr np::RendererConstants RENDERER_CONSTANTS = {
    .executionMode = np::eExecutionMode::SWAPCHAIN, .sceneType = np::eSceneType::ASSIMP
};

int main(int argc, char** argv)
{
    np::App app;
    app.create(RENDERER_CONSTANTS);
    if (argc == 2)
    {
        std::string scenePath = argv[1];

        try
        {
            app.loadSceneFromPath(scenePath.c_str());
            app.createRenderingResources();
            app.render();
        }
        catch (const std::exception e)
        {
            std::cerr << e.what() << std::endl;
            return 1;
        }
    }
    else
    {
        std::cerr
            << "Specify the path to a 3D scene file (GLB, FBX, etc.) as the 1st program argument."
            << std::endl;
        return 1;
    }

    return 0;
}
