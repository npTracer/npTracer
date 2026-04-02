#pragma once

#include "usd_plugin/NPTracerHdRenderParam.h"

#include <pxr/imaging/hd/light.h>
#include <pxr/imaging/hd/renderDelegate.h>

PXR_NAMESPACE_OPEN_SCOPE

class NPTracerHdRenderDelegate;  // forward declare

class NPTracerHdLight : public HdLight
{
public:
    NPTracerHdLight(const SdfPath& sprimId, NPTracerHdRenderDelegate* renderDelegate);
    ~NPTracerHdLight() override;

    HdDirtyBits GetInitialDirtyBitsMask() const override;
    void Sync(HdSceneDelegate* delegate, HdRenderParam* renderParam,
              HdDirtyBits* dirtyBits) override;

private:
    NPTracerHdRenderDelegate* _pCreator;
};

PXR_NAMESPACE_CLOSE_SCOPE
