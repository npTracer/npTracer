#pragma once

#include <pxr/imaging/hd/renderDelegate.h>
#include <pxr/pxr.h>

PXR_NAMESPACE_OPEN_SCOPE

// object created by the render delegate to transport top-level scene state to each prim during Sync().
class NPTracerHdRenderParam final : public HdRenderParam
{
public:
    NPTracerHdRenderParam();
    virtual ~NPTracerHdRenderParam() override = default;
};

PXR_NAMESPACE_CLOSE_SCOPE
