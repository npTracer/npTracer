#include "usd_plugin/primitives/NPTracerHdLight.h"

#include "usd_plugin/NPTracerHdRenderDelegate.h"

PXR_NAMESPACE_OPEN_SCOPE

NPTracerHdLight::NPTracerHdLight(const SdfPath& sprimId, NPTracerHdRenderDelegate* renderDelegate)
    : HdLight(sprimId), _pCreator(renderDelegate)
{
}

NPTracerHdLight::~NPTracerHdLight() {}

HdDirtyBits NPTracerHdLight::GetInitialDirtyBitsMask() const
{
    return DirtyParams;
}

void NPTracerHdLight::Sync(HdSceneDelegate* delegate, HdRenderParam* renderParam,
                           HdDirtyBits* dirtyBits)
{
}

PXR_NAMESPACE_CLOSE_SCOPE
