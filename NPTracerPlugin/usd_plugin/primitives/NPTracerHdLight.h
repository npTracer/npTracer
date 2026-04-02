#pragma once

#include "usd_plugin/NPTracerHdRenderParam.h"
#include <NPTracerRenderer/structs.h>

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

protected:
    NPTracerHdRenderDelegate* _pCreator;
    NPLight* _pLight = nullptr;

    void _AddToScene();
    void _RemoveFromScene();

    virtual void _PrepareLight() = 0;
};

// in USD (specifically Houdini Solaris USD), sphere light is essentially a point light
class NPTracerHdSphereLight : public NPTracerHdLight
{
public:
    NPTracerHdSphereLight(const SdfPath& sprimId, NPTracerHdRenderDelegate* renderDelegate);
    void Sync(HdSceneDelegate* delegate, HdRenderParam* renderParam,
              HdDirtyBits* dirtyBits) override;

private:
    void _PrepareLight() override;
};

PXR_NAMESPACE_CLOSE_SCOPE
