#include "usd_plugin/NPTracerHdRendererPlugin.h"

#include "usd_plugin/NPTracerHdRenderDelegate.h"

#include <pxr/imaging/hd/rendererPluginRegistry.h>

PXR_NAMESPACE_OPEN_SCOPE

TF_REGISTRY_FUNCTION(TfType)
{
    HdRendererPluginRegistry::Define<NPTracerHdRendererPlugin>();
}

HdRenderDelegate* NPTracerHdRendererPlugin::CreateRenderDelegate()
{
    return new NPTracerHdRenderDelegate();
}

void NPTracerHdRendererPlugin::DeleteRenderDelegate(HdRenderDelegate* delegate)
{
    delete delegate;
}

bool NPTracerHdRendererPlugin::IsSupported(bool gpuEnabled) const
{
    return true;
}

PXR_NAMESPACE_CLOSE_SCOPE
