#pragma once

#include "context.h"

class App
{
    static constexpr bool enableDebug =
#ifdef NDEBUG
        false;
#else
        true;
#endif

    static constexpr uint32_t WIDTH = 2560;
    static constexpr uint32_t HEIGHT = 1440;

private:
    Context context;

    GLFWwindow* window = nullptr;

    void create();
    void render();
    void destroy();

public:
    void run();
};
