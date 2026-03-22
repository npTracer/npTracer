#pragma once

#include <cstdint>

class NPTracerIntegrator
{
public:
    void Initialize();
    void RenderToBuffer(float* dst, uint32_t width, uint32_t height);

private:
    bool _initialized = false;
};
