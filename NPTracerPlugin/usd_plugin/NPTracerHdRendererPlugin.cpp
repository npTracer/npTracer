#include "usd_plugin/NPTracerHdRendererPlugin.h"

#include "usd_plugin/NPTracerDebugCodes.h"
#include "usd_plugin/NPTracerHdRenderDelegate.h"

#include <pxr/imaging/hd/rendererPluginRegistry.h>

PXR_NAMESPACE_OPEN_SCOPE

TF_REGISTRY_FUNCTION(TfType)
{
    HdRendererPluginRegistry::Define<NPTracerHdRendererPlugin>();
}

HdRenderDelegate* NPTracerHdRendererPlugin::CreateRenderDelegate()
{
#if NPTRACER_DEBUG
    TfDebug::Enable(NPTRACER_RENDER);  // enable debugging at entrypoint
#endif
    return new NPTracerHdRenderDelegate();
}

void NPTracerHdRendererPlugin::DeleteRenderDelegate(HdRenderDelegate* delegate)
{
    delete delegate;
#if NPTRACER_DEBUG
    TfDebug::Disable(NPTRACER_RENDER);
#endif
}

bool NPTracerHdRendererPlugin::IsSupported(bool gpuEnabled) const
{
    return true;
}

PXR_NAMESPACE_CLOSE_SCOPE
