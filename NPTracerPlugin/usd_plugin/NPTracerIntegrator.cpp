#include "usd_plugin/NPTracerIntegrator.h"

#include "usd_plugin/NPTracerHdRenderDelegate.h"

void NPTracerIntegrator::Initialize()
{
    // TODO: call Vulkan init here
}

void NPTracerIntegrator::RenderToBuffer(float* dst, UINT32 width, UINT32 height)
{
    if (!_initialized)
    {
        Initialize();
        _initialized = true;
    }

    // TODO: vulkan render frame offscreen. map GPU buffer to CPU buffer?
}
