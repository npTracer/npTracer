#include "app.h"

#include <iostream>
#include <stdexcept>
#include <string>

int main(int argc, char** argv)
{
    App app;
    app.create();
    if (argc == 2)
    {
        std::string scenePath = argv[1];

        try
        {
            app.loadScene(scenePath.c_str());
            app.createRenderingResources();
            app.run();
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
