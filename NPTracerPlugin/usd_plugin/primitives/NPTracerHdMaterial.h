#pragma once

#include "usd_plugin/NPTracerHdRenderParam.h"
#include <NPTracerRenderer/structs.h>

#include <pxr/imaging/hd/material.h>

PXR_NAMESPACE_OPEN_SCOPE

class NPTracerHdRenderDelegate;

class NPTracerHdMaterial : public HdMaterial
{
public:
    NPTracerHdMaterial(const SdfPath& id, NPTracerHdRenderDelegate* renderDelegate);
    ~NPTracerHdMaterial() override;

    HdDirtyBits GetInitialDirtyBitsMask() const override;

    void Sync(HdSceneDelegate* delegate, HdRenderParam* renderParam,
              HdDirtyBits* dirtyBits) override;

protected:
    void _AddToScene();
    void _RemoveFromScene();

private:
    NPTracerHdRenderDelegate* _pCreator;
    np::NPMaterial* _pMaterial = nullptr;
};

PXR_NAMESPACE_CLOSE_SCOPE
