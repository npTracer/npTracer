#pragma once

#include <pxr/imaging/hd/renderDelegate.h>
#include <pxr/imaging/hd/rendererPlugin.h>

PXR_NAMESPACE_OPEN_SCOPE

class NPTracerHdRendererPlugin final : public HdRendererPlugin
{
public:
    HdRenderDelegate* CreateRenderDelegate() override;
    void DeleteRenderDelegate(HdRenderDelegate* delegate) override;

    bool IsSupported(bool gpuEnabled) const override;
};

PXR_NAMESPACE_CLOSE_SCOPE
